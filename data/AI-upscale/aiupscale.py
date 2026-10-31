"""The AI upscale of Vertigo's graphics with Qwen Image 2.1 in ComfyUI, step by step (see README.md).

  aiupscale.py check             can ComfyUI be reached, and does it have the models?
  aiupscale.py export            the game's originals, as Vertigo decodes them, into work/originals
  aiupscale.py prepare           the game's Real-ESRGAN set copied into work/esrgan
  aiupscale.py names             which picture file each original is (config/names.json)
  aiupscale.py texts             Qwen3-VL proposes the texts in the pictures, to check by eye
  aiupscale.py run [KEY ...]     the upscale, of all originals or the given ones, into work/out
  aiupscale.py set NAME          a set for the game in work/sets/NAME
  aiupscale.py review            a page comparing the results with the originals and Real-ESRGAN
  aiupscale.py use NAME|esrgan   switches the game to that set, or back
  aiupscale.py compare KEY X Y W H   a picture of a part of the original next to the result
"""
import argparse
import glob
import html
import json
import os
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import numpy as np  # noqa: E402
from PIL import Image, ImageDraw, ImageFont  # noqa: E402

from paths import (ESRGAN_SET, FOLDER, NAMES, ORIGINALS, OUT, SETS, TEXTS, UPSCALED,  # noqa: E402
                   UPSCALED_ASIDE, WORK, app_dir, data_dir, esrgan, keys, original)

FOUND = os.path.join(WORK, 'texts-found.json')


def load(path):
    return json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}


def save(data, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(data, open(path, 'w', encoding='utf-8'), indent=1, ensure_ascii=False, sort_keys=True)


def check(args):
    from comfy import Comfy
    import workflows
    comfy = Comfy()
    stats = comfy.check()
    devices = ', '.join(d['name'] for d in stats.get('devices', []))
    print(f'ComfyUI {stats["system"]["comfyui_version"]} at {comfy.server}: {devices}')
    missing = []
    for folder, name in (('diffusion_models', workflows.QWEN_UNET), ('text_encoders', workflows.QWEN_CLIP),
                         ('vae', workflows.QWEN_VAE)):
        if name not in comfy._json(f'/models/{folder}'):
            missing.append(f'{folder}/{name}')
    nodes = comfy._json('/object_info')
    missing += [f'node {n}' for n in ('TextEncodeQwenImage21', 'QwenImage21Cache', 'TextGenerate', 'PreviewAny') if n not in nodes]
    print('Missing: ' + ', '.join(missing) if missing else 'Everything the pipeline needs is there.')


def export(args):
    """Vertigo's own upscaling run in a throwaway home with a stand-in for Real-ESRGAN, which keeps
    the originals Vertigo hands over."""
    if sys.platform == 'win32':
        raise SystemExit('The export runs on macOS and Linux only.')
    vertigo = args.vertigo or next((p for p in (os.path.join(FOLDER, '..', '..', 'build', 'vertigo'), shutil.which('vertigo'))
                                    if p and os.path.exists(p)), None)
    if not vertigo:
        raise SystemExit('Where is Vertigo? Pass --vertigo PATH.')
    home = os.path.join(WORK, 'home')
    shutil.rmtree(home, ignore_errors=True)
    app = app_dir(home)
    tool = os.path.join(app, 'Real-ESRGAN')
    os.makedirs(os.path.join(tool, 'models'))
    with open(os.path.join(app, 'settings.ini'), 'w') as file:
        file.write(f'[General]\ndatadir={data_dir(args.data)}\n')
    for model in ('realesrgan-x4plus.bin', 'realesr-animevideov3-x4.bin'):
        open(os.path.join(tool, 'models', model), 'w').close()
    stand_in = os.path.join(tool, 'realesrgan-ncnn-vulkan')
    with open(stand_in, 'w') as file:
        file.write(f'#!/bin/sh\nexec "{sys.executable}" -I "{os.path.join(FOLDER, "standin.py")}" "{ORIGINALS}" "$@"\n')
    os.chmod(stand_in, 0o755)
    shutil.rmtree(ORIGINALS, ignore_errors=True)
    env = dict(os.environ, CFFIXED_USER_HOME=home, HOME=home, XDG_DATA_HOME=os.path.join(home, '.local', 'share'))
    subprocess.run([vertigo, '--upscale', '--timeout', '1800'], env=env, check=True)
    shutil.rmtree(home)
    print(f'{len(keys())} originals in {ORIGINALS}')


def prepare(args):
    """The game's Real-ESRGAN set copied, as the base of the AI set and for what keeps it."""
    source = args.source
    if not source:
        app = app_dir()
        aside, current = os.path.join(app, UPSCALED_ASIDE), os.path.join(app, UPSCALED)
        source = aside if os.path.isdir(aside) else current
        if os.path.islink(source):
            raise SystemExit(f'{source} is a set from here, not the Real-ESRGAN one; pass --source.')
    if not os.path.exists(os.path.join(source, 'source.txt')):
        raise SystemExit(f'No Real-ESRGAN set in {source}: make it in the game first (Options › Upscaled graphics).')
    shutil.rmtree(ESRGAN_SET, ignore_errors=True)
    shutil.copytree(source, ESRGAN_SET)
    copies = [name for name in os.listdir(ESRGAN_SET) if name.endswith(('.jpg', '.png')) and len(name) == 20]
    print(f'{len(copies)} copies from {source} in {ESRGAN_SET}')


def fnv(data, h=14695981039346656037):
    for byte in data:
        h = ((h ^ byte)*1099511628211) & 0xffffffffffffffff
    return h


def names(args):
    """Each picture's file, found by decoding the PCX pictures as the game does (rooms and
    backgrounds with their palette cut to RGB565) and hashing them like it; the few the game decodes
    a little differently by likeness. The R16 pictures and the textures stay unnamed."""
    wanted = set(keys())
    found, candidates = {}, []
    data = data_dir(args.data)
    for path in sorted(glob.glob(os.path.join(data, '[Gg][Ff][Xx]', '[Pp][Ii][Cc]', '**', '*'), recursive=True)):
        if not path.lower().endswith('.pcx'):
            continue
        image = Image.open(path)
        if image.mode != 'P':
            continue
        palette = np.array(image.getpalette()[:768], dtype=np.uint8).reshape(-1, 3)
        indices = np.asarray(image)
        relative = os.path.relpath(path, data).lower()
        for table in (palette, palette & np.array([0xf8, 0xfc, 0xf8], dtype=np.uint8)):
            rgb = table[indices]
            candidates.append((relative, rgb.astype(np.int16)))
            rgba = np.concatenate([rgb, np.full(rgb.shape[:2] + (1,), 255, np.uint8)], axis=2)
            key = f'{fnv(rgba.tobytes(), fnv(struct.pack("<ii", rgba.shape[1], rgba.shape[0]))):016x}'
            if key in wanted:
                found[key] = relative
    unnamed = {k: np.asarray(Image.open(original(k)).convert('RGB'), dtype=np.int16) for k in wanted if k not in found}
    named = set(found.values())
    for relative, rgb in candidates:
        if relative in named:
            continue
        best, difference = None, 4.0
        for key, pixels in unnamed.items():
            if pixels.shape == rgb.shape and np.abs(pixels - rgb).mean() < difference:
                best, difference = key, np.abs(pixels - rgb).mean()
        if best:
            found[best] = relative
            named.add(relative)
            del unnamed[best]
    save(found, NAMES)
    print(f'{len(found)} of {len(wanted)} named in {NAMES}')


def texts(args):
    """Qwen3-VL's proposals of the texts in the pictures and textures (work/texts-found.json), and
    sheets of them to check by eye (work/texts-sheet-N.png)."""
    from comfy import Comfy
    from pipeline import kind
    import workflows
    found = load(FOUND)
    comfy = Comfy()
    folder = os.path.join(WORK, 'texts')
    os.makedirs(folder, exist_ok=True)
    for key in args.keys or keys():
        if key in found or kind(key) not in ('picture', 'texture'):
            continue
        source = Image.open(original(key)).convert('RGB')
        scale = max(1, 1024//max(source.size))
        path = os.path.join(folder, key + '.png')
        source.resize((source.width*scale*2, source.height*scale*2), Image.LANCZOS).save(path)
        answer = comfy.text(workflows.ask(comfy.upload(path), workflows.FIND_TEXT)).strip()
        answer = answer.removeprefix('```json').removesuffix('```').strip()
        try:
            items = json.loads(answer)
        except json.JSONDecodeError:
            print(key, 'no answer to use')
            continue
        found[key] = [{'text': item['text'], 'box': [round(item['bbox_2d'][0]*source.width/1000), round(item['bbox_2d'][1]*source.height/1000),
                                                     round(item['bbox_2d'][2]*source.width/1000), round(item['bbox_2d'][3]*source.height/1000)]}
                      for item in items if item.get('text') and item.get('bbox_2d')]
        save(found, FOUND)
        print(key, [t['text'] for t in found[key]], flush=True)
    checked = load(TEXTS)
    items = [(k, t) for k, ts in sorted(found.items()) for t in ts if k not in checked]
    width, height, columns = 300, 110, 4
    for part in range(0, len(items), 40):
        chunk = items[part:part + 40]
        sheet = Image.new('RGB', (columns*width, (len(chunk) + columns - 1)//columns*(height + 14)), (40, 40, 40))
        draw = ImageDraw.Draw(sheet)
        for n, (key, item) in enumerate(chunk):
            source = Image.open(original(key)).convert('RGB')
            x1, y1, x2, y2 = item['box']
            crop = source.crop((max(0, x1 - 4), max(0, y1 - 4), min(source.width, x2 + 4), min(source.height, y2 + 4)))
            s = min(width/max(1, crop.width), height/max(1, crop.height), 8)
            crop = crop.resize((max(1, int(crop.width*s)), max(1, int(crop.height*s))), Image.NEAREST)
            x, y = n % columns*width, n//columns*(height + 14)
            sheet.paste(crop, (x, y + 14))
            draw.text((x + 2, y + 1), f'{key} "{item["text"]}" {item["box"]}', fill='yellow')
        sheet.save(os.path.join(WORK, f'texts-sheet-{part//40 + 1}.png'))
    print(f'{len(items)} proposed texts in {WORK}/texts-sheet-*.png; put the right ones into {TEXTS}')


def run(args):
    from pipeline import run as upscale
    upscale(args.keys or keys())


def make_set(args):
    from pipeline import make_set as build
    build(args.name)


REVIEW_PAGE = '''<!doctype html><meta charset="utf-8"><title>AI upscale review</title>
<style>
body { background: #111; color: #ddd; font: 14px system-ui; margin: 16px; }
header { position: sticky; top: 0; background: #111; padding: 8px 0; border-bottom: 1px solid #333; }
header label { margin-left: 12px; }
h2 { font-size: 15px; margin: 24px 0 4px; } p { margin: 0 0 8px; color: #999; }
img { max-width: 100%; height: auto; cursor: pointer; display: block; }
img.pixels { image-rendering: pixelated; }
</style>
<header>Hold the mouse button on an image to see instead:
<label><input type="radio" name="other" value="original"> the original (enlarged, its pixels kept)</label>
<label><input type="radio" name="other" value="esrgan"> the Real-ESRGAN copy</label></header>
ROWS
<script>
let other = 'original';
try { other = localStorage.getItem('other') || other; } catch (e) {}
for (const radio of document.querySelectorAll('input[name=other]')) {
  radio.checked = radio.value === other;
  radio.addEventListener('change', () => { other = radio.value; try { localStorage.setItem('other', other); } catch (e) {} });
}
for (const img of document.querySelectorAll('section img')) {
  const show = (e) => { e.preventDefault(); img.src = img.dataset[other]; img.classList.toggle('pixels', other === 'original'); };
  const back = () => { img.src = img.dataset.ai; img.classList.remove('pixels'); };
  img.addEventListener('mousedown', show);
  img.addEventListener('touchstart', show);
  for (const e of ['mouseup', 'mouseleave', 'touchend', 'touchcancel']) img.addEventListener(e, back);
}
</script>'''


def review(args):
    """A page with each AI copy; holding the mouse button on it shows the original instead, enlarged
    to the same size, or the Real-ESRGAN copy, as chosen at the top."""
    from pipeline import kind
    known = load(NAMES)
    rows = []
    for path in sorted(glob.glob(os.path.join(OUT, '*.json')), key=lambda p: (json.load(open(p))['kind'], p)):
        key = os.path.basename(path)[:-5]
        if kind(key) == 'keep':
            continue
        report = json.load(open(path))
        ai = os.path.join(OUT, key + '.png')
        width, height = Image.open(original(key)).size
        title = f'{key} · {report["kind"]} · {known.get(key, "")}'
        said = ', '.join(t.get('text') or t.get('hint', '') for t in report['texts'])
        note = f'{report["seconds"]} s, difference {report["difference"]}' + ('' if report['usable'] else ' · NOT USED (shifted)')
        files = {name: html.escape('file://' + path) for name, path in (('ai', ai), ('esrgan', esrgan(key)), ('original', original(key)))}
        # the size the copy has, which the original takes too when it is shown instead
        rows.append(f'<section><h2>{html.escape(title)}</h2><p>{html.escape(note)}{" · text: " + html.escape(said) if said else ""}</p>'
                    f'<img src="{files["ai"]}" width="{width*4}" height="{height*4}" data-ai="{files["ai"]}" '
                    f'data-esrgan="{files["esrgan"]}" data-original="{files["original"]}"></section>')
    out = os.path.join(WORK, 'review.html')
    open(out, 'w').write(REVIEW_PAGE.replace('ROWS', '\n'.join(rows)))
    print(f'{out}: {len(rows)} images')


def use(args):
    """The game's set switched: to a set from here (a link; the Real-ESRGAN set is put aside the
    first time), or back to the Real-ESRGAN set. The game should not be running."""
    app = app_dir()
    current, aside = os.path.join(app, UPSCALED), os.path.join(app, UPSCALED_ASIDE)
    if args.name == 'esrgan':
        if os.path.islink(current):
            os.remove(current)
        if os.path.isdir(aside) and not os.path.exists(current):
            os.rename(aside, current)
    else:
        target = os.path.join(SETS, args.name)
        if not os.path.exists(os.path.join(target, 'source.txt')):
            raise SystemExit(f'No set {target} (make it with "aiupscale.py set {args.name}").')
        if os.path.isdir(current) and not os.path.islink(current):
            if os.path.exists(aside):
                raise SystemExit(f'{aside} is in the way.')
            os.rename(current, aside)
        if os.path.islink(current):
            os.remove(current)
        os.symlink(target, current)
    print(f'{current} -> {os.readlink(current) if os.path.islink(current) else "the Real-ESRGAN set"}')


def compare(args):
    """A part of the original (enlarged, its pixels kept) next to the same part of the result."""
    source = Image.open(original(args.key)).convert('RGBA')
    result = Image.open(os.path.join(OUT, args.key + '.png')).convert('RGBA')
    x, y, w, h = args.box
    zoom = args.zoom
    panels = []
    for image, scale, resample in ((source, 1, Image.NEAREST), (result, 4, Image.LANCZOS)):
        crop = image.crop((x*scale, y*scale, (x + w)*scale, (y + h)*scale)).resize((w*zoom, h*zoom), resample)
        backed = Image.new('RGB', crop.size)
        backed.paste(crop, (0, 0), crop)
        panels.append(backed)
    label = 28
    gap = 8
    picture = Image.new('RGB', (2*w*zoom + gap, h*zoom + label), (20, 20, 20))
    draw = ImageDraw.Draw(picture)
    font = ImageFont.load_default(size=18)
    for n, (panel, title) in enumerate(zip(panels, ('Original', 'AI upscale'))):
        picture.paste(panel, (n*(w*zoom + gap), label))
        draw.text((n*(w*zoom + gap) + 8, 4), title, fill=(230, 230, 230), font=font)
    out = args.out or os.path.join(WORK, f'compare-{args.key}.jpg')
    picture.save(out, quality=88)
    print(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('check').set_defaults(do=check)
    p = commands.add_parser('export')
    p.add_argument('--vertigo', help='the Vertigo program (default: build/vertigo of this checkout, or on the PATH)')
    p.add_argument('--data', help='the game data (default: VERTIGO_DATA, or Vertigo\'s datadir setting)')
    p.set_defaults(do=export)
    p = commands.add_parser('prepare')
    p.add_argument('--source', help='the Real-ESRGAN set (default: the one next to Vertigo\'s saves)')
    p.set_defaults(do=prepare)
    p = commands.add_parser('names')
    p.add_argument('--data')
    p.set_defaults(do=names)
    p = commands.add_parser('texts')
    p.add_argument('keys', nargs='*')
    p.set_defaults(do=texts)
    p = commands.add_parser('run')
    p.add_argument('keys', nargs='*')
    p.set_defaults(do=run)
    p = commands.add_parser('set')
    p.add_argument('name')
    p.set_defaults(do=make_set)
    commands.add_parser('review').set_defaults(do=review)
    p = commands.add_parser('use')
    p.add_argument('name')
    p.set_defaults(do=use)
    p = commands.add_parser('compare')
    p.add_argument('key')
    p.add_argument('box', type=int, nargs=4, metavar='N', help='x y width height, in the original\'s pixels')
    p.add_argument('--zoom', type=int, default=4)
    p.add_argument('--out')
    p.set_defaults(do=compare)
    args = parser.parse_args()
    os.makedirs(WORK, exist_ok=True)
    args.do(args)


if __name__ == '__main__':
    main()
