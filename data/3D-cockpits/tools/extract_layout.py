"""
Write a cockpit's layout from the game data to layouts/cockpit_NNN.json, so that its model builds
without the game data: the camera (view angle and optical centre), the symmetry axis of the art,
the rectangles and lamps of the DES file's monitor sections, and the outline of the see-through
(window) area as polygons in pixels of the 640x480 view.

  Blender -b --factory-startup --python tools/extract_layout.py -- --cockpit 001 [--data DIR]

Needs numpy only (also plain Python with numpy).
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, HERE)
import cockpitdata as CD  # noqa: E402
import vectorize as VZ  # noqa: E402

ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
cid = ARGV[ARGV.index("--cockpit") + 1] if "--cockpit" in ARGV else "001"
cp = CD.Cockpit(cid, ARGV[ARGV.index("--data") + 1] if "--data" in ARGV else None)

sections = {}
for name, values in cp.des.items():
    numbers = {k: int(float(v)) for k, v in values.items()
               if k in ("x1", "y1", "width", "height", "radius", "lampx1", "lampy1", "lampwidth",
                        "lampheight")}
    if "x1" in numbers:
        sections[name] = numbers
# the window: the 0.5 contour of the smoothed see-through mask (window_field), sample [i, j] of
# the field is pixel (j + 0.5, i + 0.5)
window = [VZ.simplify(loop, 0.05) + 0.5 for loop in VZ.contours(CD.window_field(cp.see), 0.5)]
layout = dict(
    cockpit=cid,
    view_angle=cp.angle,
    center=[cp.cx, cp.cy],
    mirror_x=CD.symmetry_axis(cp.see, cp.cx),
    sections=sections,
    window=[[[round(float(x), 2), round(float(y), 2)] for x, y in loop] for loop in window
            if len(loop) > 2],
)
os.makedirs(os.path.join(HERE, "layouts"), exist_ok=True)
path = os.path.join(HERE, "layouts", f"cockpit_{cid}.json")
with open(path, "w") as f:
    json.dump(layout, f, separators=(",", ":"))
print(f"[layout] {path}: {len(sections)} sections, window {len(window)} loops, "
      f"{sum(len(l) for l in window)} points")
