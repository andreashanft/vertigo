"""
Export a cockpit's art as a plain 640x480 PNG for an image upscaler. The see-through (window)
area is filled with the surrounding colours so upscalers don't form dark halos at its edge; the
mask itself is re-applied from COCKPIT.R16 by project_textures.py --art.

  Blender -b --factory-startup --python tools/export_art.py -- --cockpit 000 [--data DIR]

Writes out/NNN/art/cockpit_NNN_art.png.
"""

import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import cockpitlib as L  # noqa: E402

L.load(L.arg("--cockpit", "000"))
rgb, filled = L.ART.astype(np.float64).copy(), ~L.SEE
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
out = os.path.join(L.OUT, "art")
os.makedirs(out, exist_ok=True)
path = os.path.join(out, f"cockpit_{L.CID}_art.png")
L.save_rgba(rgb.astype(np.float32), path)
print("[art] written", path)
