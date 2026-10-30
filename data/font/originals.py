#!/usr/bin/env python3
# Writes originals.json, the game's DPLARGE glyphs as PNGs, for the editor to compare against.
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

font = glyphs(os.path.join(gfx, 'FNT', 'DPLARGE.FNT'), palette(os.path.join(gfx, 'PAL', 'GUI', 'BORDER.PAL')))
# the editor's ids are CP437 codes; this font keeps Ö and Ü at 0x95 and 0x96
INDEX = {'99': 0x95, '9a': 0x96}
out = {}
for g in json.load(open(os.path.join(HERE, 'glyphs.json'), encoding='utf-8'))['glyphs']:
    img = font[INDEX.get(g['id'], int(g['id'], 16)) - 32]
    b = io.BytesIO(); img.save(b, 'PNG')
    out[g['id']] = {'png': base64.b64encode(b.getvalue()).decode(), 'ow': img.width, 'oh': img.height}
json.dump(out, open(os.path.join(HERE, 'originals.json'), 'w'))
print(len(out), 'glyphs written to originals.json')
