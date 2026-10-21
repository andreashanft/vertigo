"""
Export a cockpit's art as a plain 640x480 PNG for an image upscaler. The see-through (window)
area is filled with the surrounding colours so upscalers don't form dark halos at its edge; the
mask itself comes from COCKPIT.R16 again in compose_texture.py --art.

  Blender -b --factory-startup --python tools/export_art.py -- --cockpit 000 [--data DIR]

Writes out/NNN/art/cockpit_NNN_art.png. Needs numpy and OpenImageIO only.
"""

import os
import sys

import numpy as np
import OpenImageIO as oiio

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
import cockpitdata as CD  # noqa: E402

ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
cid = ARGV[ARGV.index("--cockpit") + 1] if "--cockpit" in ARGV else "000"
cp = CD.Cockpit(cid, ARGV[ARGV.index("--data") + 1] if "--data" in ARGV else None)
rgb, filled = cp.art.astype(np.float64).copy(), ~cp.see
for _ in range(400):
    if filled.all():
        break
    acc, n = np.zeros_like(rgb), np.zeros(filled.shape)
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        f, c = np.roll(filled, (dy, dx), (0, 1)), np.roll(rgb, (dy, dx), (0, 1))
        acc += c * f[..., None]
        n += f
    grow = ~filled & (n > 0)
    rgb[grow] = acc[grow] / n[grow, None]
    filled |= grow
out = os.path.join(HERE, "out", cid, "art")
os.makedirs(out, exist_ok=True)
path = os.path.join(out, f"cockpit_{cid}_art.png")
o = oiio.ImageOutput.create(path)
o.open(path, oiio.ImageSpec(640, 480, 3, oiio.UINT8))
o.write_image((np.clip(rgb, 0, 1) * 255 + 0.5).astype(np.uint8))
o.close()
print("[art] written", path)
