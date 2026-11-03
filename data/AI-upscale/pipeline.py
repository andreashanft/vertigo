"""The AI upscaling of one image after the other, and the set made from them.

What each original gets, by kind:
  texture  (the models' textures, 256x256)  one edit at 1024x1024
  picture  (stations, rooms, title, ...)    an edit in tiles of about a megapixel, blended
  map      (the sea and area maps)          as a picture, then the labels, lines and frames put
                                            back from the Real-ESRGAN copy (their colours mark them)
  icon     (the items' icons, the depot's buttons)
                                            too small alone: 16 to a sheet of 1024x1024, cut apart
  movie    (the depot's items turning)      four pictures in a row to a sheet, then steadied
                                            against Real-ESRGAN (steady.py), as they play
  greyed   (the depot's greyed-out buttons)  their button's copy, with their transparency
  keep     (the effects' textures, the autopilot's destinations, the cursor, the credits, frames,
            the depot's boats turning over and its smallest marks)
                                            the Real-ESRGAN copy stays
Smoke and fire, which the AI turns into technical structure, keep the Real-ESRGAN copy too: in the
parts of a texture config/esrgan-regions.json names, or all of it. Where an original has
transparent parts (the 2D cockpits' windows, some maps' edges) the colour there is made black for
the model, and the copy takes the Real-ESRGAN copy's transparency.

An edit is Qwen Image 2.1 drawing the original, enlarged 4x, anew with it as the reference: at
about a megapixel it keeps everything in place. Text an image has (config/texts.json) is named in
the prompt of the tiles it is in, and read back by Qwen3-VL; a tile drawn off as a whole is moved back
by the shift measured against its input, and one still shifted or with a text that doesn't read right
is drawn again with other seeds. Small texts can be drawn again from
close up, upside-down ones turned upright for it. Afterwards the colours are matched to the
original's (below 2 pixels of the original, the AI's detail; above, the original's colour), the
vertical streaks and the dithering some station pictures have in their open water are smoothed away,
and a copy with a tile still off by more than half a pixel is not used.
"""
import difflib
import glob
import json
import multiprocessing
import os
import shutil
import time
from concurrent.futures import ProcessPoolExecutor

import numpy as np
from PIL import Image, ImageFilter

from comfy import Comfy
from measure import gray, likeness, measure, shift
from paths import (ESRGAN_SET, ORIGINALS, OUT, REGIONS, SETS, STREAKS, TEXTS, TILES, VIBRANT, esrgan_image, movies,
                   original, sources)
import prompts
import steady
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
VIBRANT_COLOURS = 0.75  # how far a vibrant picture's broad colours are the original's
CALM_AI = 0.15  # how much of the AI's own detail stays in the calm space round a logo
CALM_REACH = 31  # how far round a logo the space is calm (original pixels), before it fades out
CALM_FADE = 40  # the blur over which it fades (original pixels)
STEADY_WORKERS = max(1, min(8, (os.cpu_count() or 2) - 2))  # movies steadied side by side
LIKENESS = 0.8  # a text drawn less like the original (another size, doubled) is drawn again
STREAKY = 1.6  # from this ratio of column to row changes the open water counts as streaked
DITHERED = 3  # from this change from row to row the open water counts as dithered
ICON_CELL = 256  # an icon's place on a sheet (4x pixels): the largest, 48x48, with room round it
ICON_SHEET = 4  # icons a row and a column of a sheet


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
    file, picture = sources().get(key, ('', -1))
    # the depot's items turning; its boats turning over keep Real-ESRGAN's, which is less noisy there
    if picture >= 0:
        return 'movie' if file.startswith('gfx/mvi/thing/') else 'keep'
    if file.endswith(' (greyed out)'):
        return 'greyed'
    if file.startswith(('gfx/img/icon/', 'gfx/img/desktop/depot/')) and max(image.size) <= 64:
        return 'icon' if min(image.size) >= 24 else 'keep'
    # without the list of sources (an older export), the depot's by their sizes
    if not file and image.size in ((128, 128), (320, 200), (48, 48), (33, 33), (16, 32), (8, 8), (555, 350)):
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


def moved(image, dx, dy):
    """The image moved by dx, dy pixels (its edges repeated)."""
    return image.transform(image.size, Image.AFFINE, (1, 0, -dx, 0, 1, -dy), resample=Image.BICUBIC)


def aligned(tile, result, small, texture):
    """A drawn tile moved back by the shift measured against its input, where that brings it closer:
    the model often draws a whole tile a pixel or two off. Returns it and its shift (original pixels)."""
    reference = gray(tile.resize(small, Image.BOX))
    sx, sy = shift(reference, gray(result.resize(small, Image.BOX)))
    if texture <= MIN_CONTRAST or abs(sx) + abs(sy) >= 8 or abs(sx) + abs(sy) < 0.05:
        return result, sx, sy
    back = moved(result, sx*SCALE, sy*SCALE)
    bx, by = shift(reference, gray(back.resize(small, Image.BOX)))
    return (back, bx, by) if abs(bx) + abs(by) < abs(sx) + abs(sy) else (result, sx, sy)


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
    base = prompts.PROMPTS.get(kind_, prompts.PROMPTS['scene'])
    for j, y in enumerate(ys):
        for i, x in enumerate(xs):
            tile = big.crop((x, y, x + tw, y + th))
            path = os.path.join(folder, f'{key}-{i}-{j}.png')
            tile.save(path)
            # a text is named only to the tile it is taken from: told of a whole text, a tile with
            # only its end continues it ("D E P O T P O T")
            prompt = prompts.with_text(base, [item for item in texts_in(texts, x, y, tw, th)
                                              if 'text' not in item or owner[texts.index(item)] == (i, j)])
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
                result, sx, sy = aligned(tile, result, small, texture)
                placed = abs(sx) + abs(sy) <= MAX_SHIFT or abs(sx) + abs(sy) >= 8 or texture <= MIN_CONTRAST
                read, right = {}, []
                if placed:
                    for n in own:
                        x1, y1, x2, y2 = (v*SCALE for v in texts[n]['box'])
                        box = (max(0, x1 - x), max(0, y1 - y), min(tw, x2 - x), min(th, y2 - y))
                        score, read[f'{n}: {texts[n]["text"]}'] = matches(comfy, result, (x1 - x, y1 - y, x2 - x, y2 - y), texts[n]['text'],
                                                                folder, f'{key}-{i}-{j}-read{attempt}-{n}', texts[n].get('turned', False))
                        # read right, but also drawn in the original's shape and size
                        right.append(score if likeness(tile.crop(box), result.crop(box)) >= LIKENESS else score/2)
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
    prompt = prompts.with_text(prompts.PROMPTS.get(kind_, prompts.PROMPTS['scene']),
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
        if likeness((model_input.rotate(180) if turned else model_input).crop(inner), drawn.crop(inner)) < LIKENESS:
            score /= 2
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
    return flood((b - r >= 10) & (b - g >= 5) & (b >= 24) & flat, seeds)


def flood(allowed, seeds):
    """The allowed pixels connected to the given points (x, y)."""
    h, w = allowed.shape
    mask = np.zeros((h, w), bool)
    stack = [(y, x) for x, y in seeds if allowed[y, x]]
    for y, x in stack:
        mask[y, x] = True
    while stack:
        y, x = stack.pop()
        for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
            if 0 <= ny < h and 0 <= nx < w and allowed[ny, nx] and not mask[ny, nx]:
                mask[ny, nx] = True
                stack.append((ny, nx))
    return mask


def edges(size):
    """Points along the top and side edges of a picture, where its open water is."""
    w, h = size
    return [[x, 0] for x in range(0, w, 4)] + [[0, y] for y in range(0, h, 4)] + [[w - 1, y] for y in range(0, h, 4)]


def open_water(source):
    """The open water of a picture, reached from its top and side edges."""
    return water(source, edges(source.size))


def calm_water(source):
    """The open water of a picture whatever its fine pattern: the blue or blue-green area reached
    from its top and side edges that is flat after a blur (which dithering doesn't survive)."""
    pixels = np.asarray(source.filter(ImageFilter.GaussianBlur(2)), dtype=np.float64)
    r, g, b = pixels[..., 0], pixels[..., 1], pixels[..., 2]
    light = r + g + b
    changes = np.abs(np.diff(light, axis=0, append=light[-1:])) + np.abs(np.diff(light, axis=1, append=light[:, -1:]))
    flat = np.asarray(Image.fromarray(np.clip(changes*16, 0, 255).astype(np.uint8)).filter(ImageFilter.BoxBlur(3)), dtype=np.float64)/16 < 2.5
    return flood((b - r >= 10) & (b >= g - 3) & (b >= 20) & flat, edges(source.size))


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


def dithered(source, mask):
    """How much the water changes from row to row, in steps of brightness: 1 or 2 for clear or
    streaked water, clearly more where it is dithered."""
    g = np.asarray(source.convert('L'), dtype=np.float64)
    inner = mask[1:] & mask[:-1]
    if inner.sum() < 2000:
        return 0
    return np.abs(g[1:] - g[:-1])[inner].mean()


def smooth_water(result, found, blur):
    """The result's water (found in the original) replaced by a blur of the water alone (so nothing
    of what is in front bleeds in); softly at its edge, which stays a pixel off what is in front."""
    mask = Image.fromarray((found*255).astype(np.uint8)).filter(ImageFilter.MinFilter(3))
    mask = mask.resize(result.size, Image.BILINEAR)
    m = np.asarray(mask, dtype=np.float64)/255
    pixels = np.asarray(result, dtype=np.float64)
    smooth = np.stack([blur(pixels[..., c]*m) for c in range(3)], axis=2)/np.maximum(blur(m), 1e-6)[..., None]
    soft = np.asarray(mask.filter(ImageFilter.GaussianBlur(SCALE)), dtype=np.float64)[..., None]/255
    return Image.fromarray(np.clip(pixels*(1 - soft) + smooth*soft, 0, 255).round().astype(np.uint8))


def smooth_streaks(result, source, seeds):
    """The vertical streaks in the water smoothed away, by a wide horizontal blur, which keeps the
    water's gradient from top to bottom."""
    found = open_water(source) if seeds == 'auto' else water(source, seeds)
    kernel = np.ones(12*SCALE + 1)
    return smooth_water(result, found, lambda a: np.apply_along_axis(lambda row: np.convolve(row, kernel, mode='same'), 1, a))


def smooth_dither(result, source):
    """The dithering in the water, which the AI draws as a woven pattern, smoothed away by a blur
    of about three of the original's pixels (three box blurs each way)."""
    radius = 3*SCALE

    def box(a, axis):
        ends = [(radius + 1, radius) if i == axis else (0, 0) for i in range(2)]
        total = np.cumsum(np.pad(a, ends, mode='edge'), axis=axis)
        n = total.shape[axis]
        return (np.take(total, range(2*radius + 1, n), axis=axis) - np.take(total, range(n - 2*radius - 1), axis=axis))/(2*radius + 1)

    def blur(a):
        for _ in range(3):
            a = box(box(a, 0), 1)
        return a
    return smooth_water(result, calm_water(source), blur)


def worst_shift(tiles):
    """The largest shift of a tile it can be measured on: one with enough contrast (dark or nearly
    empty tiles give no clear match) and a plausible shift (above 8 pixels the match failed)."""
    shifts = [abs(t['shift'][0]) + abs(t['shift'][1]) for t in tiles if t['contrast'] > MIN_CONTRAST]
    return max((s for s in shifts if s < 8), default=0)


def box_mean(a, r):
    """The mean over a square of 2r+1 pixels round each (its edges repeated)."""
    p = np.pad(a, ((r + 1, r), (r + 1, r)), mode='edge')
    c = p.cumsum(0).cumsum(1)
    return (c[2*r + 1:, 2*r + 1:] - c[:-2*r - 1, 2*r + 1:] - c[2*r + 1:, :-2*r - 1] + c[:-2*r - 1, :-2*r - 1])/(2*r + 1)**2


def structure(source, low=1.5, high=5.0, radius=3):
    """Where the original has structure of its own nearby, 0 to 1, at 4x: its local spread of grey,
    widened a little. Dark water and empty background have none."""
    g = gray(source)
    mean = box_mean(g, radius)
    spread = np.sqrt(np.maximum(box_mean(g*g, radius) - mean*mean, 0))
    mask = Image.fromarray((np.clip((spread - low)/(high - low), 0, 1)*255).astype(np.uint8))
    mask = mask.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(1))
    return np.asarray(mask.resize((source.width*SCALE, source.height*SCALE), Image.BILINEAR), dtype=np.float64)[..., None]/255


def held(image, source, clean, strength=VIBRANT_COLOURS, radius=8*SCALE, floor=0.15):
    """A vibrant picture held to the original: its broad colours the original's by strength, and the
    AI's finer parts only where the original has structure (elsewhere, a little of them), so that
    what it adds in the dark background (echoes of buildings, new ones) stays out. The finer parts
    in their place come from the clean copy (Real-ESRGAN's), which has no dithering."""
    reference = source.resize(image.size, Image.LANCZOS)
    blur = ImageFilter.GaussianBlur(radius)
    a, c = np.asarray(image, dtype=np.float64), np.asarray(clean, dtype=np.float64)
    low_a, low_r, low_c = (np.asarray(i.filter(blur), dtype=np.float64) for i in (image, reference, clean))
    g = floor + (1 - floor)*structure(source)
    result = low_r + (1 - strength)*(low_a - low_r) + g*(a - low_a) + (1 - g)*(c - low_c)
    return Image.fromarray(np.clip(result, 0, 255).round().astype(np.uint8))


def logo(source, texts):
    """A picture's logo: the boxes of its texts and the warm-coloured parts joined to them (the
    title's red emblem), widened by two pixels."""
    rgb = np.asarray(source, dtype=np.float64)
    boxes = np.zeros(rgb.shape[:2], bool)
    for item in texts:
        x1, y1, x2, y2 = item['box']
        boxes[y1:y2, x1:x2] = True
    warm = (rgb[..., 0] > rgb[..., 2] + 30) & (rgb[..., 0] > 60)
    ys, xs = np.nonzero(boxes)
    found = Image.fromarray((flood(boxes | warm, list(zip(xs, ys)))*255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5))
    return np.asarray(found) > 0


def calm(result, drawn, source, clean, texts, radius=8*SCALE):
    """A vibrant picture with a calm space round its logo, so that the logo stands free: there the
    original's broad light and mostly the clean copy's fine detail (CALM_AI of the AI's), as busy as
    the original is; it fades out with the distance, and the logo itself stays as drawn."""
    found = logo(source, texts)
    near = Image.fromarray((found*255).astype(np.uint8)).filter(ImageFilter.MaxFilter(2*CALM_REACH + 1))
    w = np.asarray(near.filter(ImageFilter.GaussianBlur(CALM_FADE)), dtype=np.float64)/255
    w[found] = 0
    w = Image.fromarray((w*255).astype(np.uint8)).resize(result.size, Image.BILINEAR).filter(ImageFilter.GaussianBlur(2*SCALE))
    w = np.asarray(w, dtype=np.float64)[..., None]/255
    blur = ImageFilter.GaussianBlur(radius)
    a, c = np.asarray(drawn, dtype=np.float64), np.asarray(clean, dtype=np.float64)
    reference = source.resize(result.size, Image.LANCZOS)
    low_a, low_r, low_c = (np.asarray(i.filter(blur), dtype=np.float64) for i in (drawn, reference, clean))
    quiet = low_r + CALM_AI*(a - low_a) + (1 - CALM_AI)*(c - low_c)
    mixed = np.asarray(result, dtype=np.float64)*(1 - w) + quiet*w
    return Image.fromarray(np.clip(mixed, 0, 255).round().astype(np.uint8))


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


def flat(key):
    """The original on black where it is transparent, and whether it is."""
    rgba = Image.open(original(key)).convert('RGBA')
    source = Image.new('RGB', rgba.size)
    source.paste(rgba, (0, 0), rgba)
    return source, rgba.getchannel('A').getextrema()[0] < 255


def finish(key, kind_, result, source, transparent, start, shifts, extra=None):
    """The copy saved with its transparency and its report; returns the report."""
    if transparent:
        result.putalpha(esrgan_image(key).convert('RGBA').getchannel('A'))
    result.save(os.path.join(OUT, key + '.png'))
    sx, sy, difference, colour = measure(source, result.convert('RGB'))
    report = {'kind': kind_, 'seconds': round(time.time() - start), 'texts': load(TEXTS).get(key, []), 'tiles': shifts,
              'shift': [round(sx, 2), round(sy, 2)], 'difference': round(difference, 2),
              'colour': [round(c, 1) for c in colour], 'usable': bool(worst_shift(shifts) <= MAX_SHIFT), **(extra or {})}
    json.dump(report, open(os.path.join(OUT, key + '.json'), 'w'), indent=1, ensure_ascii=False)
    return report


def icons(comfy, keys, folder):
    """Icons too small to redraw alone, each enlarged 4x into its place on a sheet on black, the
    sheet redrawn as one piece and cut apart again; each with its own shift, as a tile."""
    per_sheet = ICON_SHEET*ICON_SHEET
    for first in range(0, len(keys), per_sheet):
        start = time.time()
        group = keys[first:first + per_sheet]
        sheet = Image.new('RGB', (ICON_SHEET*ICON_CELL,)*2)
        places = []
        for n, key in enumerate(group):
            source, transparent = flat(key)
            size = (source.width*SCALE, source.height*SCALE)
            x = n % ICON_SHEET*ICON_CELL + (ICON_CELL - size[0])//2
            y = n//ICON_SHEET*ICON_CELL + (ICON_CELL - size[1])//2
            sheet.paste(source.resize(size, Image.LANCZOS), (x, y))
            places.append((key, source, transparent, (x, y, x + size[0], y + size[1])))
        drawn, _ = edit(comfy, 'icons-' + group[0], 'icon', sheet, [], folder)
        for key, source, transparent, box in places:
            result = match_colours(drawn.crop(box), source.resize((box[2] - box[0], box[3] - box[1]), Image.LANCZOS))
            sx, sy, _, _ = measure(source, result)
            contrast = float(gray(source).std())
            report = finish(key, 'icon', result, source, transparent, start,
                            [{'tile': [0, 0], 'shift': [round(sx, 2), round(sy, 2)], 'contrast': round(contrast, 1)}],
                            {'sheet': 'icons-' + group[0]})
            print(f'{key} icon: on sheet icons-{group[0]}, shift {abs(sx) + abs(sy):.2f}, difference {report["difference"]:.2f}'
                  f'{"" if report["usable"] else "  NOT USABLE"}', flush=True)


def draw_movie(comfy, file, folder):
    """A movie's pictures, four in a row enlarged 4x onto a sheet (2x2, 1024x1024) that is redrawn
    as one piece, so that they come out alike, then cut apart, into the tiles folder (KEY-drawn.png,
    with each sheet's shifts in movie-KEY.json), so that a stopped run goes on where it was."""
    keys = movies()[file]
    start = time.time()
    for first in range(0, len(keys), 4):
        group = keys[first:first + 4]
        paths = [os.path.join(folder, f'{key}-drawn.png') for key in group]
        report = os.path.join(folder, f'movie-{group[0]}.json')
        if all(os.path.exists(p) for p in paths + [report]):
            continue
        sheet = Image.new('RGB', (1024, 1024))
        for n, key in enumerate(group):
            sheet.paste(flat(key)[0].resize((512, 512), Image.LANCZOS), (n % 2*512, n//2*512))
        result, tiles = edit(comfy, f'movie-{group[0]}', 'movie', sheet, [], folder)
        for n, (key, path) in enumerate(zip(group, paths)):
            box = (n % 2*512, n//2*512, n % 2*512 + 512, n//2*512 + 512)
            match_colours(result.crop(box), flat(key)[0].resize((512, 512), Image.LANCZOS)).save(path)
        json.dump(tiles, open(report, 'w'))
    print(f'{file}: {len(keys)} pictures drawn, {round(time.time() - start)} s', flush=True)


def steady_movie(file, folder):
    """A drawn movie's pictures steadied against their Real-ESRGAN copies (steady.py), the loop
    wrapped round, into out/ with their reports. Runs in a process of its own, beside the drawing."""
    keys = movies()[file]
    start = time.time()
    drawn = [Image.open(os.path.join(folder, f'{key}-drawn.png')).convert('RGB') for key in keys]
    shifts = {}
    for first in range(0, len(keys), 4):
        tiles = json.load(open(os.path.join(folder, f'movie-{keys[first]}.json')))
        shifts.update({key: tiles for key in keys[first:first + 4]})
    backed = []
    for key in keys:
        copy = esrgan_image(key).convert('RGBA')
        back = Image.new('RGB', copy.size)
        back.paste(copy, (0, 0), copy)
        backed.append(back)
    for n, (key, image) in enumerate(zip(keys, steady.steady(drawn, backed, loop=True))):
        source, transparent = flat(key)
        finish(key, 'movie', Image.fromarray(image.round().astype(np.uint8)), source, transparent, start, shifts[key],
               {'movie': file, 'picture': n})
    print(f'{file}: steadied, {round(time.time() - start)} s', flush=True)


def run_movies(comfy, files, folder):
    """The movies drawn one after the other while those drawn are steadied in other processes, so
    that the drawing never waits."""
    if not files:
        return
    with ProcessPoolExecutor(max_workers=STEADY_WORKERS, mp_context=multiprocessing.get_context('spawn')) as pool:
        jobs = []
        for file in files:
            draw_movie(comfy, file, folder)
            jobs.append(pool.submit(steady_movie, file, folder))
        for job in jobs:
            job.result()


def greyed_out(key):
    """A greyed-out button: its button's copy (the same colours), with its own transparency. False
    while the button has none."""
    file = sources()[key][0][:-len(' (greyed out)')]
    button = next((k for k, (f, _) in sources().items() if f == file), None)
    path = os.path.join(OUT, f'{button}.png')
    if not button or not os.path.exists(path):
        return False
    source, _ = flat(key)
    finish(key, 'greyed', Image.open(path).convert('RGB'), source, True, time.time(), [], {'button': button})
    return True


def run(keys):
    """Upscales these originals, each into out/KEY.png (4x) with a report out/KEY.json; those
    done before are left as they are."""
    os.makedirs(OUT, exist_ok=True)
    folder = TILES
    os.makedirs(folder, exist_ok=True)
    texts = load(TEXTS)
    streaks = load(STREAKS)
    comfy = Comfy()
    kinds = {key: kind(key) for key in keys}
    todo = [key for key in keys if not os.path.exists(os.path.join(OUT, key + '.json'))]
    icons(comfy, [key for key in todo if kinds[key] == 'icon'], folder)
    run_movies(comfy, sorted({sources()[key][0] for key in todo if kinds[key] == 'movie'}), folder)
    for n, key in enumerate(keys):
        k = kinds[key]
        if k == 'keep' or key not in todo:
            print(f'{n + 1}/{len(keys)} {key} {k}: {"kept" if k == "keep" else "done before"}', flush=True)
            continue
        if k in ('icon', 'greyed', 'movie'):
            if k == 'greyed':
                said = "its button's copy" if greyed_out(key) else 'its button has no copy yet'
                print(f'{n + 1}/{len(keys)} {key} greyed: {said}', flush=True)
            continue
        start = time.time()
        source, transparent = flat(key)
        size = (source.width*SCALE, source.height*SCALE)
        big = np.asarray(source.resize(size, Image.LANCZOS))
        padded = tuple((side + GRID - 1)//GRID*GRID for side in size)
        big = Image.fromarray(np.pad(big, ((0, padded[1] - size[1]), (0, padded[0] - size[0]), (0, 0)), mode='edge'))
        vibrant = key in load(VIBRANT)
        result, shifts = edit(comfy, key, 'vibrant' if vibrant else k, big, texts.get(key, []), folder)
        for i, item in enumerate(texts.get(key, [])):
            if item.get('zoom'):
                result = close_up(comfy, key, 'vibrant' if vibrant else k, big, result, item, folder, i)
        result = result.crop((0, 0) + size)
        if vibrant:
            clean = esrgan_image(key).convert('RGB')
            drawn, result = result, held(result, source, clean)
            if load(VIBRANT)[key].get('calm'):
                result = calm(result, drawn, source, clean, texts.get(key, []))
        else:
            result = match_colours(result, source.resize(size, Image.LANCZOS))
        if k == 'map':
            copy = esrgan_image(key).convert('RGB')
            result.paste(copy, (0, 0), map_overlay(source))
        # the vertical streaks in some station pictures' open water (an artefact of the original)
        # smoothed away: where config/streaks.json says, or found by themselves in the backgrounds
        smoothed = key in streaks or (k == 'picture' and source.size == (640, 480)
                                      and streaky(source, open_water(source)) >= STREAKY)
        if smoothed:
            result = smooth_streaks(result, source, streaks.get(key, 'auto'))
        # the dithering in the open water of some (the AI turns it into a woven pattern) smoothed too;
        # a vibrant one has none left, and the blur would soften what stands in its distance
        dither = (not vibrant and k == 'picture' and source.size == (640, 480)
                  and dithered(source, calm_water(source)) >= DITHERED)
        if dither:
            result = smooth_dither(result, source)
        for x1, y1, x2, y2 in regions().get(key, []):
            box = (x1*SCALE, y1*SCALE, x2*SCALE, y2*SCALE)
            result.paste(esrgan_image(key).convert('RGB').crop(box), box[:2])
        report = finish(key, k, result, source, transparent, start, shifts,
                        {'streaks smoothed': bool(smoothed), 'dither smoothed': bool(dither), 'vibrant': vibrant})
        print(f'{n + 1}/{len(keys)} {key} {k}: {report["seconds"]} s, {len(shifts)} tiles, worst shift {worst_shift(shifts):.2f}, '
              f'difference {report["difference"]:.2f}{"" if report["usable"] else "  NOT USABLE"}', flush=True)


def make_set(name):
    """The Real-ESRGAN set copied, with the usable AI copies in place (as the game keeps them:
    JPEG at quality 92, the transparency beside it in KEY-alpha.png)."""
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
        for old in glob.glob(os.path.join(target, key + '.*')) + glob.glob(os.path.join(target, key + '-alpha.png')):
            os.remove(old)
        image = Image.open(os.path.join(OUT, key + '.png'))
        if image.mode == 'RGBA':
            image.getchannel('A').save(os.path.join(target, key + '-alpha.png'))
        image.convert('RGB').save(os.path.join(target, key + '.jpg'), quality=92)
        used += 1
    print(f'{target}: {used} AI copies, the rest Real-ESRGAN')
