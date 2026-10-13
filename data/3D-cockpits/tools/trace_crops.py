"""
Reference images for tracing a cockpit: the original COCKPIT.R16 brightened, see-through area in
magenta, with a pixel grid (grey every 10 px, yellow every 50 px).

  Blender -b --factory-startup --python tools/trace_crops.py -- --cockpit 001 \
      [--crop name,x0,y0,x1,y1,scale ...] [--data DIR]

Writes out/NNN/trace/reference.png (2x, whole image) and one PNG per --crop.
"""

import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import cockpitlib as L  # noqa: E402

L.load(L.arg("--cockpit", "000"))
out = os.path.join(L.OUT, "trace")
os.makedirs(out, exist_ok=True)
rgb = np.where(L.SEE[..., None], np.array([1.0, 0.0, 1.0]), np.clip(L.ART * 2.0, 0, 1))


def save(name, x0, y0, x1, y1, s):
    c = np.repeat(np.repeat(rgb[y0:y1, x0:x1], s, 0), s, 1).copy()
    for gx in range(x0, x1):
        if gx % 10 == 0:
            col = (gx - x0) * s
            c[:, col] = [1, 1, 0] if gx % 50 == 0 else c[:, col] * 0.5 + 0.25
    for gy in range(y0, y1):
        if gy % 10 == 0:
            row = (gy - y0) * s
            c[row, :] = [1, 1, 0] if gy % 50 == 0 else c[row, :] * 0.5 + 0.25
    L.save_rgba(c.astype(np.float32), os.path.join(out, name + ".png"))


save("reference", 0, 0, 640, 480, 2)
for spec in L.ARGV:
    if spec.count(",") == 5:
        n, x0, y0, x1, y1, s = spec.split(",")
        save(n, int(x0), int(y0), int(x1), int(y1), int(s))
print("[trace] written to", out)
