"""The AI upscaling of one image after the other, and the set made from them.

What each original gets, by kind:
  texture  (the models' textures, 256x256)  one edit at 1024x1024
  picture  (stations, rooms, title, ...)    an edit in tiles of about a megapixel, blended
  map      (the sea and area maps)          as a picture, then the labels, lines and frames put
                                            back from the Real-ESRGAN copy (their colours mark them)
  keep     (the effects' textures, the autopilot's destinations, the cursor, the credits, frames)
                                            the Real-ESRGAN copy stays
Smoke and fire, which the AI turns into technical structure, keep the Real-ESRGAN copy too: in the
parts of a texture config/esrgan-regions.json names, or all of it. Where an original has
transparent parts (the 2D cockpits' windows, some maps' edges) the colour there is made black for
the model, and the copy takes the Real-ESRGAN copy's transparency.

An edit is Qwen Image 2.1 drawing the original, enlarged 4x, anew with it as the reference: at
about a megapixel it keeps everything in place. Text an image has (config/texts.json) is named in
the prompt of the tiles it is in, and read back by Qwen3-VL; a tile that comes out shifted or with
a text that doesn't read right is drawn again with other seeds. Small texts can be drawn again from
close up, upside-down ones turned upright for it. Afterwards the colours are matched to the
original's (below 2 pixels of the original, the AI's detail; above, the original's colour), the
vertical streaks some station pictures have in their open water are smoothed away, and a copy with
a tile still off by more than half a pixel is not used.
"""
import difflib
import glob
import json
import os
import shutil
import time

import numpy as np
from PIL import Image, ImageFilter

from comfy import Comfy
from measure import gray, measure, shift
from paths import ESRGAN_SET, ORIGINALS, OUT, REGIONS, SETS, STREAKS, TEXTS, TILES, esrgan, original
import prompts
import workflows

SCALE = 4
TILE = 1024
SINGLE = 1.15e6  # up to this many pixels an image is drawn in one piece
OVERLAP = 256
GRID = 32
MAX_SHIFT = 0.5
RETRIES = 3
TEXT_RETRIES = 5  # for tiles with text, which must also read back right
MIN_CONTRAST = 15
STREAKY = 1.6  # from this ratio of column to row changes the open water counts as streaked


def load(path):
    return json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}


def regions():
    return load(REGIONS)


def kind(key):
    image = Image.open(original(key))
    transparent = image.convert('RGBA').getchannel('A').getextrema()[0] < 255
    if os.path.exists(os.path.join(ORIGINALS, 'text', key + '.png')):
        # the autopilot's destinations (180x180) have their labels in the picture, which the AI
        # redraws wrongly
        return 'map' if image.size == (304, 284) else 'keep'
    if regions().get(key) == 'all':
        return 'keep'
    if image.size == (256, 256):
        return 'keep' if transparent else 'texture'
    return 'picture'


def starts(length, tile, overlap):
    """Tile positions covering the length, evenly spread, overlapping by at least overlap."""
    if length <= tile:
        return [0]
    count = int(np.ceil((length - overlap)/(tile - overlap)))
    return [round(i*(length - tile)/(count - 1)) for i in range(count)]


def ramp(length, before, after):
    w = np.ones(length)
    if before:
        w[:before] = np.linspace(0, 1, before + 2)[1:-1]
    if after:
        w[-after:] = np.linspace(1, 0, after + 2)[1:-1]
    return w


def texts_in(texts, x, y, w, h):
    """The texts at least a third of whose box (original pixels) lies in the tile (4x pixels)."""
    found = []
    for item in texts:
        x1, y1, x2, y2 = (v*SCALE for v in item['box'])
        inside = max(0, min(x2, x + w) - max(x1, x))*max(0, min(y2, y + h) - max(y1, y))
        if inside >= (x2 - x1)*(y2 - y1)/3:
            found.append(item)
    return found


def normal(text):
    """A text as compared: in capitals, without spaces, quotes and full stops, dashes as hyphens."""
    text = text.upper().replace('\u2013', '-').replace('\u2014', '-')
    return ''.join(c for c in text if not c.isspace() and c not in '"\'.`')


def matches(comfy, result, box, text, folder, name, turned=False):
    """How well the text in its box of a drawn tile (in the tile's pixels) reads back, 0 to 1: 1 when
    Qwen3-VL's reading has it, as it is or turned upside down (the reading may take in a
    neighbouring text too; one written upside down must read so); otherwise how alike they are.
    Returns that and the reading."""
    answer = '' if turned else reads(comfy, result, box, folder, name)
    if normal(text) in normal(answer):
        return 1.0, answer
    upside_down = reads(comfy, result, box, folder, name + '-turned', turn=True)
    if normal(text) in normal(upside_down):
        return 1.0, upside_down + ' (upside down)'
    return max(difflib.SequenceMatcher(None, normal(a), normal(text)).ratio() for a in (answer, upside_down)), answer or upside_down


def reads(comfy, result, item, folder, name, turn=False):
    """What Qwen3-VL reads in a box of a drawn tile (in the tile's pixels)."""
    x1, y1, x2, y2 = item
    margin = 4*SCALE
    crop = result.crop((max(0, x1 - margin), max(0, y1 - margin), min(result.width, x2 + margin), min(result.height, y2 + margin)))
    if turn:
        crop = crop.rotate(180)
    if max(crop.size) < 512:
        crop = crop.resize((crop.width*512//max(crop.size), crop.height*512//max(crop.size)), Image.LANCZOS)
    path = os.path.join(folder, name + '.png')
    crop.save(path)
    answer = comfy.text(workflows.ask(comfy.upload(path), workflows.READ_TEXT))
    return answer.strip()


def edit(comfy, key, kind_, big, texts, folder, seed=1):
    """The big image redrawn in tiles; returns it and each tile's shift."""
    W, H = big.size
    tw, th = (W, H) if W*H <= SINGLE else (min(TILE, W), min(TILE, H))
    xs, ys = starts(W, tw, OVERLAP), starts(H, th, OVERLAP)
    # a text in the overlap of tiles is taken from one of them, the one it is deepest in, as each
    # draws its letters a little differently and blending them doubles them
    owner = {}
    for n, item in enumerate(texts):
        x1, y1, x2, y2 = (v*SCALE for v in item['box'])
        depth = {(i, j): min(x1 - x, x + tw - x2, y1 - y, y + th - y2) for j, y in enumerate(ys) for i, x in enumerate(xs)}
        owner[n] = max(depth, key=depth.get)
    pieces = {}
    total = np.zeros((H, W, 3))
    weight = np.zeros((H, W, 1))
    shifts = []
    base = prompts.PROMPTS['texture' if kind_ == 'texture' else 'scene']
    for j, y in enumerate(ys):
        for i, x in enumerate(xs):
            tile = big.crop((x, y, x + tw, y + th))
            path = os.path.join(folder, f'{key}-{i}-{j}.png')
            tile.save(path)
            prompt = prompts.with_text(base, texts_in(texts, x, y, tw, th))
            image = comfy.upload(path)
            small = (tw//SCALE, th//SCALE)
            texture = float(np.asarray(tile.convert('L'), dtype=np.float64).std())
            # a tile that comes out shifted, or with a text of its own that doesn't read back right, is
            # drawn again with other seeds, keeping the best
            own = [n for n in owner if owner[n] == (i, j) and 'text' in texts[n]]
            best = None
            for attempt in range(TEXT_RETRIES if own else RETRIES):
                out = os.path.join(folder, f'{key}-{i}-{j}-out{attempt}.png')
                comfy.run(workflows.qwen21(image, prompt, seed + attempt, f'vertigo/{key}-{i}-{j}'), out)
                result = Image.open(out).convert('RGB')
                if result.size != tile.size:
                    result = result.resize(tile.size, Image.LANCZOS)
                sx, sy = shift(gray(tile.resize(small, Image.BOX)), gray(result.resize(small, Image.BOX)))
                placed = abs(sx) + abs(sy) <= MAX_SHIFT or abs(sx) + abs(sy) >= 8 or texture <= MIN_CONTRAST
                read, right = {}, []
                if placed:
                    for n in own:
                        x1, y1, x2, y2 = (v*SCALE for v in texts[n]['box'])
                        score, read[f'{n}: {texts[n]["text"]}'] = matches(comfy, result, (x1 - x, y1 - y, x2 - x, y2 - y), texts[n]['text'],
                                                                folder, f'{key}-{i}-{j}-read{attempt}-{n}', texts[n].get('turned', False))
                        right.append(score)
                score = (placed, sum(right), -(abs(sx) + abs(sy)))
                if best is None or score > best[0]:
                    best = (score, result, sx, sy, attempt, read)
                if placed and all(r == 1.0 for r in right):
                    break
            _, result, sx, sy, attempt, read = best
            shifts.append({'tile': [i, j], 'shift': [round(sx, 2), round(sy, 2)], 'contrast': round(texture, 1),
                           'seed': seed + attempt, **({'read': read} if read else {})})
            w = np.outer(ramp(th, ys[j - 1] + th - y if j > 0 else 0, y + th - ys[j + 1] if j + 1 < len(ys) else 0),
                         ramp(tw, xs[i - 1] + tw - x if i > 0 else 0, x + tw - xs[i + 1] if i + 1 < len(xs) else 0))[..., None]
            total[y:y + th, x:x + tw] += np.asarray(result, dtype=np.float64)*w
            weight[y:y + th, x:x + tw] += w
            pieces[(i, j)] = (result, x, y)
    blended = Image.fromarray(np.clip(total/weight, 0, 255).round().astype(np.uint8))
    for n, item in enumerate(texts):
        result, x, y = pieces[owner[n]]
        margin = 6*SCALE
        x1, y1, x2, y2 = (v*SCALE for v in item['box'])
        box = (max(x, x1 - margin), max(y, y1 - margin), min(x + tw, x2 + margin), min(y + th, y2 + margin))
        mask = Image.new('L', (box[2] - box[0], box[3] - box[1]), 0)
        mask.paste(255, (margin//2, margin//2, mask.width - margin//2, mask.height - margin//2))
        mask = mask.filter(ImageFilter.GaussianBlur(margin/4))
        blended.paste(result.crop((box[0] - x, box[1] - y, box[2] - x, box[3] - y)), box[:2], mask)
    return blended, shifts


def close_up(comfy, key, kind_, big, result, item, folder, n, seed=1):
    """A small text drawn again from close up: the part of the enlarged original round its box,
    enlarged further to about a megapixel, redrawn until it reads back right (or the best of a
    few), and put back into the result, softly at its edges. One written upside down ("turned") is
    turned upright for the model, which draws it upright whatever the prompt says, and back."""
    margin = 8*SCALE
    x1, y1, x2, y2 = (v*SCALE for v in item['box'])
    box = (max(0, x1 - margin), max(0, y1 - margin), min(big.width, x2 + margin), min(big.height, y2 + margin))
    crop = big.crop(box)
    zoom = TILE/max(crop.size)
    size = tuple(max(GRID, round(side*zoom/GRID)*GRID) for side in crop.size)
    path = os.path.join(folder, f'{key}-close{n}.png')
    turned = item.get('turned', False)
    model_input = crop.resize(size, Image.LANCZOS)
    (model_input.rotate(180) if turned else model_input).save(path)
    image = comfy.upload(path)
    prompt = prompts.with_text(prompts.PROMPTS['texture' if kind_ == 'texture' else 'scene'],
                               [{k: v for k, v in item.items() if k != 'note'} if turned else item])
    if turned:
        x1, y1, x2, y2 = box[0] + box[2] - x2, box[1] + box[3] - y2, box[0] + box[2] - x1, box[1] + box[3] - y1
    inner = tuple(round(v) for v in ((x1 - box[0])*size[0]/crop.width, (y1 - box[1])*size[1]/crop.height,
                                      (x2 - box[0])*size[0]/crop.width, (y2 - box[1])*size[1]/crop.height))
    best = None
    for attempt in range(TEXT_RETRIES):
        out = os.path.join(folder, f'{key}-close{n}-out{attempt}.png')
        comfy.run(workflows.qwen21(image, prompt, seed + attempt, f'vertigo/{key}-close{n}'), out)
        drawn = Image.open(out).convert('RGB').resize(size, Image.LANCZOS)
        score, answer = matches(comfy, drawn, inner, item['text'], folder, f'{key}-close{n}-read{attempt}')
        if best is None or score > best[0]:
            best = (score, drawn, answer)
        if score == 1.0:
            break
    score, drawn, answer = best
    if turned:
        drawn = drawn.rotate(180)
    print(f'  close-up of "{item["text"]}"{" (upside down)" if turned else ""}: read "{answer}"', flush=True)
    mask = Image.new('L', crop.size, 0)
    mask.paste(255, (margin//2, margin//2, crop.width - margin//2, crop.height - margin//2))
    mask = mask.filter(ImageFilter.GaussianBlur(margin/4))
    result = result.copy()
    result.paste(drawn.resize(crop.size, Image.LANCZOS), box[:2], mask)
    return result


def water(source, seeds):
    """The open water round the given points: the blue, flat area connected to them, its colour
    taken after a horizontal blur (which the vertical streaks don't survive). Blue means the blue
    channel clearly above the red and the green, which the dark parts of what is in front aren't;
    flat, hardly changing from row to row, which the seafloor's play of light isn't."""
    pixels = np.asarray(source, dtype=np.float64)
    kernel = np.ones(7)/7
    r, g, b = (np.apply_along_axis(lambda row: np.convolve(row, kernel, mode='same'), 1, pixels[..., c]) for c in range(3))
    rows = np.abs(np.diff(r + g + b, axis=0, append=(r + g + b)[-1:]))
    flat = np.asarray(Image.fromarray(np.clip(rows*16, 0, 255).astype(np.uint8)).filter(ImageFilter.BoxBlur(3)), dtype=np.float64)/16 < 2.5
    blue = (b - r >= 10) & (b - g >= 5) & (b >= 24) & flat
    h, w = blue.shape
    mask = np.zeros((h, w), bool)
    stack = [(y, x) for x, y in seeds if blue[y, x]]
    for y, x in stack:
        mask[y, x] = True
    while stack:
        y, x = stack.pop()
        for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
            if 0 <= ny < h and 0 <= nx < w and blue[ny, nx] and not mask[ny, nx]:
                mask[ny, nx] = True
                stack.append((ny, nx))
    return mask


def open_water(source):
    """The open water of a picture, reached from its top and side edges."""
    w, h = source.size
    seeds = [[x, 0] for x in range(0, w, 4)] + [[0, y] for y in range(0, h, 4)] + [[w - 1, y] for y in range(0, h, 4)]
    return water(source, seeds)


def streaky(source, mask):
    """How much more the water changes from column to column than from row to row: about 1 for
    clear water, clearly more with vertical streaks."""
    g = np.asarray(source.convert('L'), dtype=np.float64)
    inner = mask[1:-1, 1:-1] & mask[1:-1, 2:] & mask[2:, 1:-1]
    if inner.sum() < 2000:
        return 0
    dx = np.abs(g[1:-1, 2:] - g[1:-1, 1:-1])[inner].mean()
    dy = np.abs(g[2:, 1:-1] - g[1:-1, 1:-1])[inner].mean()
    return dx/max(dy, 0.05)


def smooth_streaks(result, source, seeds):
    """The vertical streaks in the water smoothed away: there, a wide horizontal blur of the
    result's water alone (so nothing of what is in front bleeds in), which keeps its gradient from
    top to bottom; softly at the water's edge, which stays a pixel off what is in front."""
    found = open_water(source) if seeds == 'auto' else water(source, seeds)
    mask = Image.fromarray((found*255).astype(np.uint8)).filter(ImageFilter.MinFilter(3))
    mask = mask.resize(result.size, Image.BILINEAR)
    m = np.asarray(mask, dtype=np.float64)[..., None]/255
    pixels = np.asarray(result, dtype=np.float64)
    width = 12*SCALE + 1
    kernel = np.ones(width)

    def blur(a):
        return np.apply_along_axis(lambda row: np.convolve(row, kernel, mode='same'), 1, a)
    weight = blur(m[..., 0])[..., None]
    smooth = np.stack([blur(pixels[..., c]*m[..., 0]) for c in range(3)], axis=2)/np.maximum(weight, 1e-6)
    soft = np.asarray(mask.filter(ImageFilter.GaussianBlur(SCALE)), dtype=np.float64)[..., None]/255
    return Image.fromarray(np.clip(pixels*(1 - soft) + smooth*soft, 0, 255).round().astype(np.uint8))


def worst_shift(tiles):
    """The largest shift of a tile it can be measured on: one with enough contrast (dark or nearly
    empty tiles give no clear match) and a plausible shift (above 8 pixels the match failed)."""
    shifts = [abs(t['shift'][0]) + abs(t['shift'][1]) for t in tiles if t['contrast'] > MIN_CONTRAST]
    return max((s for s in shifts if s < 8), default=0)


def match_colours(image, reference, radius=2*SCALE):
    """The image's detail with the reference's colours above the radius."""
    blur = ImageFilter.GaussianBlur(radius)
    a = np.asarray(image, dtype=np.float64)
    low_a = np.asarray(image.filter(blur), dtype=np.float64)
    low_r = np.asarray(reference.filter(blur), dtype=np.float64)
    return Image.fromarray(np.clip(a - low_a + low_r, 0, 255).round().astype(np.uint8))


def map_overlay(source):
    """Where a map's labels, lines, circles and frames are: its strongly coloured pixels, widened by
    a pixel and softened, at 4x."""
    hsv = np.asarray(source.convert('HSV'), dtype=np.float64)/255
    mask = (hsv[..., 1] > 0.45) & (hsv[..., 2] > 0.3)
    image = Image.fromarray((mask*255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))
    return image.resize((source.width*SCALE, source.height*SCALE), Image.BILINEAR).filter(ImageFilter.GaussianBlur(SCALE/2))


def run(keys):
    """Upscales these originals, each into out/KEY.png (4x) with a report out/KEY.json; those
    done before are left as they are."""
    os.makedirs(OUT, exist_ok=True)
    folder = TILES
    os.makedirs(folder, exist_ok=True)
    texts = load(TEXTS)
    streaks = load(STREAKS)
    comfy = Comfy()
    for n, key in enumerate(keys):
        k = kind(key)
        report_path = os.path.join(OUT, key + '.json')
        if k == 'keep' or os.path.exists(report_path):
            print(f'{n + 1}/{len(keys)} {key} {k}: {"kept" if k == "keep" else "done before"}', flush=True)
            continue
        start = time.time()
        rgba = Image.open(original(key)).convert('RGBA')
        transparent = rgba.getchannel('A').getextrema()[0] < 255
        source = Image.new('RGB', rgba.size)
        source.paste(rgba, (0, 0), rgba)
        size = (source.width*SCALE, source.height*SCALE)
        big = np.asarray(source.resize(size, Image.LANCZOS))
        padded = tuple((side + GRID - 1)//GRID*GRID for side in size)
        big = Image.fromarray(np.pad(big, ((0, padded[1] - size[1]), (0, padded[0] - size[0]), (0, 0)), mode='edge'))
        result, shifts = edit(comfy, key, k, big, texts.get(key, []), folder)
        for n, item in enumerate(texts.get(key, [])):
            if item.get('zoom'):
                result = close_up(comfy, key, k, big, result, item, folder, n)
        result = result.crop((0, 0) + size)
        result = match_colours(result, source.resize(size, Image.LANCZOS))
        if k == 'map':
            copy = Image.open(esrgan(key)).convert('RGB')
            result.paste(copy, (0, 0), map_overlay(source))
        # the vertical streaks in some station pictures' open water (an artefact of the original)
        # smoothed away: where config/streaks.json says, or found by themselves in the backgrounds
        smoothed = key in streaks or (k == 'picture' and source.size == (640, 480)
                                      and streaky(source, open_water(source)) >= STREAKY)
        if smoothed:
            result = smooth_streaks(result, source, streaks.get(key, 'auto'))
        for x1, y1, x2, y2 in regions().get(key, []):
            box = (x1*SCALE, y1*SCALE, x2*SCALE, y2*SCALE)
            result.paste(Image.open(esrgan(key)).convert('RGB').crop(box), box[:2])
        if transparent:
            result.putalpha(Image.open(esrgan(key)).convert('RGBA').getchannel('A'))
        result.save(os.path.join(OUT, key + '.png'))
        sx, sy, difference, colour = measure(source, result.convert('RGB'))
        worst = worst_shift(shifts)
        report = {'kind': k, 'seconds': round(time.time() - start), 'texts': texts.get(key, []), 'tiles': shifts,
                  'shift': [round(sx, 2), round(sy, 2)], 'difference': round(difference, 2),
                  'colour': [round(c, 1) for c in colour], 'usable': bool(worst <= MAX_SHIFT), 'streaks smoothed': bool(smoothed)}
        json.dump(report, open(report_path, 'w'), indent=1, ensure_ascii=False)
        print(f'{n + 1}/{len(keys)} {key} {k}: {report["seconds"]} s, {len(shifts)} tiles, worst shift {worst:.2f}, '
              f'difference {difference:.2f}{"" if report["usable"] else "  NOT USABLE"}', flush=True)


def make_set(name):
    """The Real-ESRGAN set copied, with the usable AI copies in place (as the game keeps them:
    JPEG at quality 92, PNG with transparency)."""
    target = os.path.join(SETS, name)
    if os.path.exists(target):
        shutil.rmtree(target)
    shutil.copytree(ESRGAN_SET, target)
    used = 0
    for report_path in sorted(glob.glob(os.path.join(OUT, '*.json'))):
        key = os.path.basename(report_path)[:-5]
        if kind(key) == 'keep':
            continue
        if not json.load(open(report_path))['usable']:
            print(f'{key}: not used (shifted)')
            continue
        for old in glob.glob(os.path.join(target, key + '.*')):
            os.remove(old)
        image = Image.open(os.path.join(OUT, key + '.png'))
        if image.mode == 'RGBA':
            image.save(os.path.join(target, key + '.png'))
        else:
            image.convert('RGB').save(os.path.join(target, key + '.jpg'), quality=92)
        used += 1
    print(f'{target}: {used} AI copies, the rest Real-ESRGAN')
