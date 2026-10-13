"""
Cockpit 002 (open canopy: two hazard-striped pillars from the top corners down to the console,
monitors in both top corners, orange crown at the top, sill beam across the front, console with
the master monitor between two round monitors, corrugated hoses). Layout traced from
VFX/COCKPIT/002.

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
      --python cockpit_002.py -- [--data DIR] [--out DIR] [--no-render | --quick]
"""

import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cockpitlib import *  # noqa: E402,F401,F403
import cockpitlib as L  # noqa: E402

L.setup("002")
set_hull(center=(0.0, 0.05, -0.15), axes=(1.2, 1.15, 1.02), floor_z=-1.0)
M.update(  # colours of this cockpit's art (the base set is the green/bronze one of 000)
    hazard=material("Hazard", (0.45, 0.32, 0.03), 0.3, 0.45),
    orange=material("Orange", (0.55, 0.13, 0.03), 0.3, 0.4),
    lime=material("Lime", (0.30, 0.45, 0.03), 0.2, 0.4),
    steel=material("Steel", (0.13, 0.16, 0.18), 0.7, 0.4),
    navy=material("Navy", (0.03, 0.04, 0.10), 0.5, 0.45),
    hose_grey=material("HoseGrey", (0.22, 0.24, 0.28), 0.6, 0.35),
)


# --- local helpers ------------------------------------------------------------------------------
def mirror(pts):
    """Left-side pixel outline -> right side (the art is symmetric about the optical centre)."""
    return [(2 * L.CX - x, y) for x, y in pts]


def mirror_ctrl(ctrl):
    return [(2 * L.CX - x, y, *rest) for x, y, *rest in ctrl]


def fit_across(ctrl, w_px, near=0.0):
    """Members traced as a centre line (px, py, dist) plus a width in pixels measured across the
    line in the image -> control points and metric widths for beam()/cable(). Their cross-section
    lies perpendicular to the member and to the view ray, which the flat 105 deg image shows
    skewed and unevenly stretched off-centre (up to 15 % too wide near the edges). So the width is
    solved in the image, at the near side of the member (`near` metres in front of the line), and
    the control point is moved until both edges straddle the traced centre."""
    P2 = np.array([(c[0], c[1]) for c in ctrl], float)
    T2 = np.gradient(P2, axis=0)
    N = np.stack([-T2[:, 1], T2[:, 0]], -1) / np.linalg.norm(T2, axis=1, keepdims=True)
    T3 = unit(np.gradient(path3d(ctrl), axis=0))
    wp = np.broadcast_to(np.asarray(w_px, float), (len(ctrl),))
    out, w = [], []
    for (px, py, d), n, t, s in zip(ctrl, N, T3, wp):
        c, W = np.array([px, py], float), 0.0
        for _ in range(4):
            P = at(*c, d - near)
            Wd = unit(np.cross(t, -unit(P)))
            if W == 0.0:  # first guess from the local scale
                W = s * 0.02 / abs(np.dot(to_px(P + Wd * 0.01) - to_px(P - Wd * 0.01), n))
            e1, e2 = to_px(P - Wd * W / 2), to_px(P + Wd * W / 2)
            W *= s / abs(np.dot(e2 - e1, n))
            c -= n * np.dot((e1 + e2) / 2 - (px, py), n)
        out.append((float(c[0]), float(c[1]), d))
        w.append(float(W))
    return out, w


def beam_px(name, ctrl, mat, w_px, h=0.06, round_frac=0.3, **kw):
    """beam() whose silhouette is w_px wide across the traced centre line."""
    pts, w = fit_across(ctrl, w_px, near=(0.5 - round_frac / 2) * h)
    return beam(name, pts, mat, w=w, h=h, round_frac=round_frac, **kw)


def cable_px(name, ctrl, mat, r_px, amp=0.18, **kw):
    """cable() whose silhouette (crest of the corrugation) has radius r_px across the path."""
    pts, w = fit_across(ctrl, 2 * r_px)
    r = float(np.mean(w)) / 2
    return cable(name, pts, mat, r=r / (1 + amp) if kw.get("pitch") else r, amp=amp, **kw)


def px_metres(px, py, dist, dx, dy):
    """Metres spanned by the pixel offset (dx, dy) centred on (px, py) at `dist` (unlike m_per_px
    this includes the radial stretch of the flat image, up to 1.6x in the corners)."""
    a, b = at(px - dx / 2, py - dy / 2, dist), at(px + dx / 2, py + dy / 2, dist)
    return float(np.linalg.norm(b - a))


def bevel_px(px, py, dist, bevel):
    """A prism bevel grows the outline by about this many pixels."""
    return bevel / px_metres(px, py, dist, 1, 0)


def inset(pts, d):
    """Offset a pixel outline inwards by d px (negative: outwards). d must stay below half the
    smallest feature of the outline, or edges flip and the bevelled curve fill breaks up."""
    P = np.asarray(pts, float)
    area = np.sum(P[:, 0] * np.roll(P[:, 1], -1) - np.roll(P[:, 0], -1) * P[:, 1])
    E = np.roll(P, -1, 0) - P
    N = np.stack([-E[:, 1], E[:, 0]], -1) * np.sign(area)
    N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
    n1, n2 = np.roll(N, 1, 0), N                      # normals of the edges into / out of a vertex
    miter = (n1 + n2) / np.maximum(1 + (n1 * n2).sum(1, keepdims=True), 0.25)
    return [tuple(p) for p in P + d * miter]


def prism_px(name, panel, outlines, z0, z1, mat, bevel=0.004, **kw):
    """prism() with the outer outline inset (holes outset) by its bevel, so that the silhouette
    matches the traced outline instead of growing by the bevel."""
    o = np.array(panel.o)
    b = bevel_px(*to_px(o), np.linalg.norm(o), min(bevel, (z1 - z0) / 2 * 0.9))
    loops = [inset(outlines[0], b)] + [inset(hole, -b) for hole in outlines[1:]]
    return prism(name, panel, loops, z0, z1, mat, bevel=bevel, **kw)


def sky_px(az, el):
    """Pixel coordinates (also outside the 640x480 frame) of the direction azimuth/elevation."""
    t = math.tan(math.radians(az))
    return L.CX + L.F * t, L.CY - L.F * math.tan(math.radians(el)) * math.sqrt(1 + t * t)


# --- hull: glass wherever the art is see-through; the side windows (el -8..17 deg at the frame
# edge) continue as straight strips to the pilot's shoulders ------------------------------------
# (a step that divides the 111 deg dense band evenly, like 0.25, makes masked_hull emit a
# zero-width face column at +-55.5 deg az that shows as a shading crease)
masked_hull(step_deg=0.28, glass_fn=lambda az, el: 52.0 < abs(az) < 92.0 and -8.0 < el < 17.0)
standard_interior()

# --- top-corner monitors (SlaveMonitor0/1) ------------------------------------------------------
# Traced on the left monitor; the right one is its mirror image except for the screen and the foot.
for section, side in (("SlaveMonitor0", 1), ("SlaveMonitor1", -1)):
    X = (lambda pts: pts) if side > 0 else mirror
    scr = rrect_px(10, 5, 113, 84, 7) if side > 0 else rrect_px(526, 6, 631, 83, 7)
    b = bevel_px(*X([(60, 60)])[0], 0.76, 0.005)  # rect_monitor's bezel bevel, in pixels
    p = rect_monitor(section, 0.76, inset(X(rrect_px(-2, -2, 118, 104, 8)), b), inset(scr, -b),
                     facing=1.0, bezel_z=(-0.035, 0.0), screen_z=-0.02,
                     body_px=X(rrect_px(6, 6, 112, 98, 8)), body_z=(-0.16, -0.03),
                     mat_bezel=M["steel"])
    prism_px(f"{section}_Vent", p, [X([(3, 97), (70, 97), (70, 104), (58, 114.5), (17, 114.5),
                                       (4, 103)])], -0.04, 0.0, M["steel"], bevel=0.003)
    for k, x0 in enumerate((18, 30, 41)):
        prism(f"{section}_VentTab_{k}", p, [X(rrect_px(x0 + 0.5, 110, x0 + 6, 116, 1))], -0.02,
              -0.004, M["dark"], bevel=0.0008)
    prism_px(f"{section}_BracketUpper", p, [X(rrect_px(112, -25, 142, 38, 5))], -0.06, 0.012,
             M["lime"], bevel=0.005)
    foot = [(110, 46), (131, 44), (131, 96), (120, 107), (93, 111), (82, 104), (84, 97), (110, 94)]
    if side < 0:  # the right foot ends flush with the pillar
        foot = [(110, 46), (131, 44), (131, 96), (120, 106), (92, 106), (92, 97), (110, 94)]
    prism_px(f"{section}_BracketLower", p, [X(foot)], -0.06, 0.012, M["lime"], bevel=0.005)
    lamp(f"Lamp_{section}", p, *X([(54, 94.5)])[0], r=0.006, z=0.004)
    # hung from the roof, braced against the pillar behind the bracket
    mount_up(f"{section}_Mount", p.world_px(*X([(61, 10)])[0], -0.09), 0.018)
    box_between(f"{section}_Arm", p.world_px(*X([(121, 72)])[0], -0.05),
                at(*X([(140, 88)])[0], 0.875), 0.035, 0.03, M["lime"])

# --- hazard-striped pillars: from the crown down past the monitors to the sill ------------------
# (px, py, dist, apparent width px); the top bends towards the crown behind the monitor brackets.
pillar_l = [(252, 42, 0.90, 44), (228, 45, 0.895, 46), (198, 47, 0.89, 50), (163, 73, 0.88, 54),
            (129, 104, 0.875, 58), (124, 125, 0.87, 56), (129, 150, 0.87, 56), (133, 178, 0.865, 54),
            (130, 195, 0.865, 45), (136, 210, 0.865, 51), (141, 223, 0.86, 46), (141, 238, 0.86, 47),
            (147, 262, 0.86, 48), (152, 279, 0.86, 53), (156, 294, 0.86, 64), (164, 312, 0.865, 70),
            (170, 330, 0.875, 64)]  # the foot ends inside the sill
for name, ctrl in (("Pillar_L", pillar_l), ("Pillar_R", mirror_ctrl(pillar_l))):
    beam_px(name, [c[:3] for c in ctrl], M["hazard"], [c[3] for c in ctrl], h=0.12)

# --- crown at the top centre (canopy hub where the pillars meet) --------------------------------
pk = Panel(320, 35, 0.86, facing=1.0)
crown = [(214, -40), (426, -40), (426, 75), (416, 74), (405, 72), (395, 73), (387, 72), (380, 71),
         (373, 70), (369, 67), (366, 63), (363, 60), (355, 58.5), (351, 59), (348, 61.5), (338, 62),
         (331, 61), (325, 60), (320, 59), (309, 60), (303, 61.5), (293, 62), (290, 60), (286, 58),
         (279, 59), (276, 60), (274, 63), (271, 66), (268, 69), (260, 71), (250, 72), (245, 73),
         (238, 72), (230, 72), (220, 74), (214, 75)]
prism_px("Crown", pk, [crown], -0.05, 0.0, M["orange"], bevel=0.004)
prism("Crown_Body", pk, [rrect_px(240, -40, 400, 58, 10)], -0.13, -0.04, M["dark"])
mount_up("Crown_Mount", pk.world_px(320, 20, -0.1), 0.05)

# --- sill beam across the front, below the windscreen -------------------------------------------
# Outside the frame it runs on at the elevation of its frame-edge end (-12.6 deg) under the side
# window and turns into the hull (control points spaced like the traced ones, else the spline
# bulges; widths there in metres).
sill = [(*sky_px(az, -12.6), 0.97 + (-52.5 - az) * 0.008)
        for az in (-72.5, -68.5, -64.5, -60.5, -56.5)]
sill = [(px, py, d, 0.09 / px_metres(px, py, d, 0, 1)) for px, py, d in sill]
sill += [(0, 318, 0.97, 54), (24, 322.5, 0.955, 50), (48, 326.5, 0.94, 49), (97, 330, 0.91, 47),
         (160, 333, 0.88, 40), (230, 338, 0.87, 36), (320, 340, 0.87, 36)]
sill = sill + mirror_ctrl(sill[-2::-1])
beam_px("Sill", [c[:3] for c in sill], M["navy"], [c[3] for c in sill], h=0.10, start=True,
        end=True)

# lower side windows: hazard-striped struts from the sill down to the floor
struts = {"Strut_LowerL": [(59, 345, 0.95), (78, 450, 0.97)],
          "Strut_EdgeL": [(-12, 345, 0.97), (11, 462, 0.99)]}
struts.update({k[:-1] + "R": mirror_ctrl(v) for k, v in list(struts.items())})
for name, ctrl in struts.items():
    beam_px(name, ctrl, M["hazard"], 18 if "Lower" in name else 24, h=0.06, end=True)

# --- console: master monitor between the two round monitors -------------------------------------
bezel = [(226, 500), (226, 318), (230, 316), (235, 313.5), (236, 310.5), (238, 309.5), (244, 309.5),
         (249, 312), (252, 313), (259, 313), (263, 312), (266, 309.5), (268, 309), (270, 305),
         (272, 303.5), (295, 303.5), (297, 305), (301, 309), (304, 310.5), (339, 310.5), (342, 309),
         (346, 305), (348, 303.5), (371, 303.5), (373, 305), (375, 309), (378, 311), (380, 312),
         (384, 313), (390, 313), (394, 312), (396, 311), (399, 309.5), (405, 309.5), (407, 311),
         (408, 313.5), (414, 316), (417, 318), (418, 500)]
b = bevel_px(320, 330, 0.70, 0.005)
pc = rect_monitor("MasterMonitor", 0.70, inset(bezel, b), inset(rrect_px(231, 331, 413, 490, 8), -b),
                  bezel_z=(-0.045, 0.0), screen_z=-0.028, body_px=rrect_px(224, 316, 420, 560, 12),
                  body_z=(-0.30, -0.04), mat_bezel=M["navy"])
lamp("Lamp_MasterMonitor", pc, 242, 323)
bot = pc.world_px(320, 560, -0.17)
box("MasterMonitor_Pedestal", (bot[0], bot[1], (bot[2] + L.FLOOR_Z) / 2),
    (0.30, 0.22, abs(bot[2] - L.FLOOR_Z) + 0.05), M["dark"], bevel=0.015)

for section, c, hc, lamp_px in (("RoundMonitor0", (174, 412), (175, 416), (216, 376)),
                                ("RoundMonitor1", (469, 412), (468, 416), (427, 376))):
    b = bevel_px(*hc, 0.72, 0.005)  # shrink radially: the 5 px teeth are too small to inset()
    housing = gear(*hc, 67 - b, 72 - b, 16, 0.42, 5)
    pr = round_monitor(section, 0.72, 40, housing_px=housing, center_px=c, housing_center_px=hc,
                       mat_housing=M["navy"], stalk_to_floor=True)
    lamp(f"Lamp_{section}", pr, *lamp_px, r=0.008, z=0.006)

# --- hoses and cables ---------------------------------------------------------------------------
cable_px("Hose_PillarL", [(150, 312, 0.86), (168, 315, 0.80), (195, 323, 0.76), (215, 337, 0.73),
                          (230, 354, 0.72)], M["hose_grey"], 15, pitch=0.014, amp=0.2, sides=14)
cable_px("Hose_PillarR", [(503, 40, 0.88), (506, 80, 0.84), (502, 120, 0.83), (497, 160, 0.83),
                          (500, 180, 0.84), (503, 186, 0.87)], M["hose_grey"], 15, pitch=0.014,
         amp=0.22, sides=14, start=True)  # plugs into the pillar
cable_px("Cable_PillarR", [(420, 358, 0.74), (440, 328, 0.78), (478, 302, 0.81), (518, 304, 0.83),
                           (548, 322, 0.86), (568, 340, 0.92)], M["navy"], 9, pitch=0.009,
         amp=0.25, sides=10)
cable_px("Hose_BottomL0", [(-40, 448, 0.80), (0, 470, 0.77), (25, 495, 0.76)], M["hose_grey"], 18,
         pitch=0.02, amp=0.2, sides=16, start=True, end=True)
cable_px("Hose_BottomL", [(26, 505, 0.76), (42, 474, 0.75), (61, 461, 0.74), (80, 466, 0.74),
                          (97, 483, 0.73), (106, 505, 0.73)], M["hose_grey"], 20, pitch=0.02,
         amp=0.2, sides=16, start=True, end=True)
cable_px("Hose_BottomL2", [(107, 455, 0.70), (125, 463, 0.69), (148, 480, 0.68), (158, 505, 0.68)],
         M["bronze"], 12, pitch=0.012, amp=0.2, sides=12, end=True)
cable_px("Hose_BottomR", [(518, 515, 0.70), (540, 490, 0.70), (558, 480, 0.71), (575, 470, 0.72),
                          (595, 466, 0.73), (622, 468, 0.75), (650, 476, 0.78)], M["hose_grey"], 23,
         pitch=0.022, amp=0.2, sides=16, start=True, end=True)
cable_px("Hose_BottomR2", [(430, 505, 0.68), (440, 474, 0.68), (458, 470, 0.68), (474, 479, 0.68),
                           (480, 505, 0.68)], M["hose_grey"], 13, pitch=0.012, amp=0.2, sides=12,
         start=True, end=True)
for k, x in enumerate((492, 516)):
    cable_px(f"Cable_RoundMonitor1_{k}", [(x - 4, 440, 0.71), (x, 470, 0.71), (x + 2, 500, 0.72)],
             M["navy"], 6, pitch=0.008, amp=0.25, sides=8, end=True)
cable_px("Cable_TopL", [(158, -20, 0.95), (157, 30, 0.86), (150, 62, 0.82), (128, 86, 0.78),
                        (106, 96, 0.77)], M["navy"], 5, pitch=0.007, amp=0.25, sides=8, start=True)
cable_px("Hose_TopL", [(130, -40, 1.0), (170, 0, 0.95), (210, 30, 0.92), (232, 42, 0.90)],
         M["dark"], 10, pitch=0.014, amp=0.25, sides=12, start=True)
cable_px("Hose_TopR", [(352, 0, 0.92), (405, 38, 0.92), (455, 50, 0.92), (500, 30, 0.95),
                       (540, -20, 1.0)], M["dark"], 11, pitch=0.014, amp=0.25, sides=12, end=True)

finish()
