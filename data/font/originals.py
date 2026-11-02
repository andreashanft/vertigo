#!/usr/bin/env python3
# Writes originals.json, the game's DPLARGE glyphs as PNGs, for the editor to compare against, and
# what its main menu mock needs: the menu's picture, its bar, the DPMEDIUM glyphs and the texts.
# The game data is not part of the repository, so this file is made locally and ignored by git.
#   python3 originals.py "<game data folder with GFX>"
import base64, io, json, os, struct, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
gfx = os.path.join(sys.argv[1], 'GFX')

def palette(path):
    d = open(path, 'rb').read()
    off = 14 if struct.unpack('<H', d[:2])[0] == 0x0802 else 4
    return [tuple(d[off + i * 3:off + i * 3 + 3]) for i in range(256)]

def glyphs(path, pal):
    # header: type byte, glyph count at 6; per glyph w, h, x, y, length and rows of (skip, opaque) runs
    d = open(path, 'rb').read()
    n, p, out = struct.unpack('<I', d[6:10])[0], 18, []
    for _ in range(n):
        w, h, _x, _y, length = struct.unpack('<IIiiI', d[p:p + 20]); p += 20
        q, img = p, Image.new('RGBA', (w, h), (0, 0, 0, 0)); px = img.load()
        for y in range(h):
            x = 0
            while x < w and q < len(d):
                skip, run = struct.unpack('<HH', d[q:q + 4]); q += 4; x += skip
                for _ in range(run):
                    px[x, y] = pal[d[q]] + (255,); q += 1; x += 1
        p += length
        out.append(img)
    return out

def picture(path, pal):
    # type 0x0401: width and height (32 bits each), eight bytes more, then a palette index per pixel
    d = open(path, 'rb').read()
    w, h = struct.unpack('<II', d[2:10])
    img = Image.new('RGB', (w, h)); img.putdata([pal[i] for i in d[18:18 + w * h]])
    return img

def texts(path):
    # the [Text] section's lines "TextN = value", as bytes (the fonts' codes)
    out = {}
    for line in open(path, 'rb').read().splitlines():
        key, sep, value = line.partition(b' = ')
        if sep and key.startswith(b'Text') and key[4:].isdigit():
            out[int(key[4:])] = value.rstrip()
    return out

def png(img):
    b = io.BytesIO(); img.save(b, 'PNG')
    return base64.b64encode(b.getvalue()).decode()

pal = palette(os.path.join(gfx, 'PAL', 'GUI', 'BORDER.PAL'))
large, medium = (glyphs(os.path.join(gfx, 'FNT', name), pal) for name in ('DPLARGE.FNT', 'DPMEDIUM.FNT'))
# the editor's ids are CP437 codes; these fonts keep Ö and Ü at 0x95 and 0x96
INDEX = {'99': 0x95, '9a': 0x96}
ID = {v: k for k, v in INDEX.items()}
out, mediums = {}, {}
for g in json.load(open(os.path.join(HERE, 'glyphs.json'), encoding='utf-8'))['glyphs']:
    i = INDEX.get(g['id'], int(g['id'], 16)) - 32
    out[g['id']] = {'png': png(large[i]), 'ow': large[i].width, 'oh': large[i].height}
    mediums[g['id']] = {'png': png(medium[i]), 'ow': medium[i].width, 'oh': medium[i].height}
# the main menu: its title (Text118) and items, top to bottom: new game, load, training, credits,
# options and quit
text = texts(os.path.join(sys.argv[1], 'TXT', 'DEEPTEXT.DES'))
ids = lambda t: [ID.get(c, '%02x' % c) for c in t]
out['menu'] = {
    'picture': png(Image.open(os.path.join(gfx, 'PIC', 'TITLE', 'TITLE.PCX')).convert('RGB')),
    'bar': png(bar := picture(os.path.join(gfx, 'IMG', 'TITLE', 'HORLINE.IMG'), pal)), 'barSize': bar.size,
    'medium': mediums, 'mediumHeight': max(img.height for img in medium),
    'title': ids(text[118]), 'items': [ids(text[n]) for n in (3, 110, 112, 111, 19, 4)],
}
json.dump(out, open(os.path.join(HERE, 'originals.json'), 'w'))
print(len(out) - 1, 'glyphs and the main menu written to originals.json')
