"""
Cockpit 001 (open canopy: trapezoid front window between two diagonal struts that run from the top
corners, where the two round monitors sit, down to the console; a header with pipes across the top;
console with the master monitor flanked by two side monitors; see-through side windows outside the
struts). Layout traced from VFX/COCKPIT/001.

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
      --python cockpit_001.py -- [--data DIR] [--out DIR] [--no-render | --quick]
"""

import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cockpitlib import *  # noqa: E402,F401,F403
import cockpitlib as L  # noqa: E402

L.setup("001")
set_hull(center=(0.0, 0.04, -0.2), axes=(1.08, 1.12, 1.08), floor_z=-0.98)
M.update(
    teal=material("Teal", (0.020, 0.150, 0.150), 0.6, 0.4),
    navy=material("Navy", (0.015, 0.030, 0.060), 0.5, 0.5),
    steel=material("Steel", (0.500, 0.520, 0.550), 0.9, 0.25),
    olive=material("Olive", (0.110, 0.120, 0.040), 0.3, 0.5),
)


def mirror(pts):
    """The art is symmetric about x = 319.5: mirror pixel points (x, y, ...) to the other side."""
    return [(639 - p[0], *p[1:]) for p in pts]


def w_m(ctrl, w_px, h=0.0):
    """Beam widths in metres for apparent widths in pixels measured across the path in the image
    (beam's w_px uses the tangential scale, too wide where the view stretches radially). h: beam
    depth; the widest part of its profile sits 0.35 h in front of the path and sets the outline."""
    P = np.array([c[:2] for c in ctrl], float)
    T = unit(np.gradient(P, axis=0))
    N = np.stack([-T[:, 1], T[:, 0]], 1)
    wp = np.broadcast_to(np.asarray(w_px, float), (len(P),))
    return np.array([np.linalg.norm(at(*(p + n * w / 2), d) - at(*(p - n * w / 2), d))
                     * (d - 0.35 * h) / d for p, n, w, (_, _, d) in zip(P, N, wp, ctrl)])


def on_hull(px, py, gap):
    """Control point (px, py, dist) gap metres in front of the hull (for points off the frame)."""
    return (px, py, float(hull_dist(unit(ray_np(px, py)))) - gap)


def lamp_disc(name, panel, px, py, r_px=6, z=0.002):
    """Flat lamp lens on a panel for DES lamps the art shows switched off: the grazing rim of a
    lamp() sphere gets the lit fallback in the texture bake and shows up as a bright ring."""
    ring = [panel.world(*panel.proj(x, y), z) for x, y in circle(px, py, r_px, 24)]
    return mesh_obj(name, ring, [tuple(range(24))], M["lamp"], COLL["Instruments"], smooth=False,
                    inward=True)


def des_lamp(section):
    """Centre of the lamp rectangle of a DES monitor section (pixels)."""
    x, y, w, h = (int(L.DES[section.lower()][k]) for k in ("lampx1", "lampy1", "lampwidth",
                                                            "lampheight"))
    return x + w / 2, y + h / 2


def inset(pts, px=1.0):
    """Offset a closed pixel outline inwards by px: prism bevels grow the outline outwards by the
    bevel radius (2-3 px at arm's length), which shows where a part borders the window. A pixel of
    overshoot is left on purpose: art on the part's rim beats a seam of art on the hull behind."""
    P = np.array(pts, float)
    area = np.sum(P[:, 0] * np.roll(P[:, 1], -1) - np.roll(P[:, 0], -1) * P[:, 1])
    e = unit(np.roll(P, -1, 0) - P)
    n = np.stack([-e[:, 1], e[:, 0]], 1) * np.sign(area)   # inward normal of edge i -> i+1
    b = unit(n + np.roll(n, 1, 0))
    return [tuple(q) for q in P + b * (px / np.maximum((b * n).sum(1), 0.4))[:, None]]


# --- hull: glass wherever the art is see-through, side windows continued beyond the frame -------
masked_hull(extend_az=72, extend_el=(-35, 30))
standard_interior()

# --- canopy frame: struts from the top corners down to the console, header arch, side windows ---
# Strut centre line and width fitted to the mask: inner edge x = 161 + 0.675 (y - 220), outer edge
# x = 85 + 0.748 (y - 220) (pulled in by up to 5 px where the receding beam came out too wide),
# plus 1 px of overshoot per side; the strut runs at 35 deg, so the width across it is 0.815 of
# the horizontal one. Above the frame strut and header arch meet in a node at the hull. The right
# side is the mirror image.
NODE = on_hull(-60, -60, 0.03)
SY = np.array([0, 45, 90, 135, 180, 220, 260, 300, 345])
X_IN = 161 + 0.675 * (SY - 220) + 6 * np.clip((140 - SY) / 30, 0, 1)   # bulge under the mounts
X_OUT = (85 + 0.748 * (SY - 220) + np.clip((SY - 140) / 40, 0, 5)
         - 25 * np.clip((160 - SY) / 70, 0, 1))   # flares out under the round monitor
STRUT = [NODE] + [((a + b) / 2 + 1, y, d) for a, b, y, d in
                  zip(X_IN, X_OUT, SY, (0.92, 0.93, 0.93, 0.93, 0.92, 0.91, 0.88, 0.85, 0.82))]
STRUT_W = np.concatenate([[100], (X_IN - X_OUT + 2) * 0.815])
beam("Header_Beam", [NODE, (40, -20, 0.92), (140, -12, 0.89), (320, -10, 0.88), (500, -12, 0.89),
                     (600, -20, 0.92), *mirror([NODE])], M["navy"], w_px=40, h=0.08)
# side window frame: the top member's lower edge is the window edge (5,160)-(73,215); beyond the
# frame top member and sill hose run along the hull at the window edges (y = 157 and 383 in the
# extended mask) back to a post at the rear end of the window (az 73 deg)
RIM = [(96, 214, 0.91), (66, 191, 0.96), (42, 171, 0.99), (13, 151, 1.0), on_hull(-120, 146, 0.05),
       on_hull(-300, 146, 0.05), on_hull(-490, 148, 0.05)]
RIM_W = np.concatenate([w_m(RIM[:4], 24, 0.07), [0.06, 0.06, 0.06]])
SILL = [(100, 390, 0.80), (60, 389, 0.85), (30, 391, 0.90), (0, 395, 0.95),
        on_hull(-150, 397, 0.05), on_hull(-420, 400, 0.06), on_hull(-480, 470, 0.07)]
POST = [on_hull(-490, 150, 0.05), on_hull(-490, 270, 0.06), on_hull(-490, 395, 0.05)]
for side, mx in (("L", list), ("R", mirror)):
    beam(f"Strut_{side}", mx(STRUT), M["navy"], w=w_m(mx(STRUT), STRUT_W, 0.11), h=0.11)
    ellipsoid(f"FrameNode_{side}", at(*mx([NODE])[0]), (0.09, 0.09, 0.09), M["teal"])
    beam(f"SideFrame_{side}", mx(RIM), M["navy"], w=RIM_W, h=0.07)
    beam(f"SideFramePost_{side}", mx(POST), M["navy"], w=0.07, h=0.06)
    # golden piston along the front face of the strut
    a, b = (np.array(at(*p)) for p in mx([(55, 95, 0.90), (112, 232, 0.84)]))
    cylinder_between(f"Strut_{side}_Piston", a, b, 0.017, M["bronze"])
    for k, p in enumerate((a, b)):
        box_between(f"Strut_{side}_PistonMount_{k}", p, p * 1.09, 0.03, 0.03, M["teal"])
    # hose along the side window sill
    cable(f"Hose_Sill_{side}", mx(SILL), M["olive"], r=0.022, pitch=0.02, amp=0.2, sides=16,
          end=True)

# --- round monitors in the top corners ----------------------------------------------------------
for side, sec, mx in (("L", "RoundMonitor0", list), ("R", "RoundMonitor1", mirror)):
    (hx, hy), = mx([(46, 50)])
    p = round_monitor(sec, 0.78, 31, housing_r_px=44, housing_center_px=(hx, hy), facing=0.3,
                      depth=0.04, back_depth=0.07)
    lip = sector(46, 50, 39, 48, 15, 172)
    prism(f"{sec}_Lip", p, [mx(lip)], -0.02, 0.01, M["steel"], bevel=0.003)
    # teal mounts around the housing, on panels facing the eye so their sides stay hidden
    arm = [(84, 4), (140, 4), (140, 35), (134, 40), (126, 46), (122, 49), (118, 64), (114, 69),
           (110, 77), (106, 89), (102, 95), (96, 101), (84, 100), (78, 70), (84, 40)]
    pa = Panel(*mx([(112, 50)])[0], 0.84, 1.0)
    prism(f"{sec}_Arm", pa, [inset(mx(arm))], -0.05, 0.0, M["teal"])
    block = [(74, 74), (88, 64), (110, 72), (112, 84), (102, 98), (88, 100), (74, 90)]
    prism(f"{sec}_Block", Panel(*mx([(93, 83)])[0], 0.79, 1.0), [inset(mx(block))], -0.03, 0.02,
          M["teal"], bevel=0.005)
    foot = [(24, 84), (48, 82), (52, 108), (22, 110)]
    prism(f"{sec}_Foot", Panel(*mx([(37, 96)])[0], 0.79, 1.0), [inset(mx(foot))], -0.03, 0.02,
          M["teal"], bevel=0.005)
    lamp(f"Lamp_{sec}_Glow", pa, *mx([(97, 22)])[0], r=0.008, z=0.004)   # lit in the art
    lamp_disc(f"Lamp_{sec}", pa, *des_lamp(sec), r_px=5)
    # bracket from the can into the strut behind
    box_between(f"{sec}_Mount", p.world(*p.proj(hx, hy), -0.06), at(*mx([(9, 60)])[0], 0.93),
                0.05, 0.05, M["dark"])

# --- header across the top ----------------------------------------------------------------------
ph = Panel(320, 30, 0.86, facing=1.0)
prism("Header_Unit", ph, [inset([(240, -30), (400, -30), (400, 24), (392, 30), (372, 33),
                                 (270, 33), (248, 30), (240, 20)])], -0.08, 0.0, M["dark"])
prism("Header_CheekL", ph, [inset([(246, 20), (300, 20), (300, 51), (294, 56), (290, 58),
                                   (282, 57), (274, 55), (270, 50), (262, 49), (258, 47),
                                   (254, 42), (250, 36), (246, 30)], 1.5)],
      -0.10, 0.015, M["teal"], bevel=0.006)
prism("Header_CheekR", ph, [inset([(342, 20), (398, 20), (398, 25), (394, 30), (390, 36),
                                   (386, 42), (382, 46), (378, 48), (370, 49), (366, 55),
                                   (358, 57), (350, 58), (346, 57), (342, 51)], 1.5)],
      -0.10, 0.015, M["teal"], bevel=0.006)
prism("Header_Grill", ph, [inset([(298, 28), (342, 28), (342, 51), (298, 51)])], -0.08, -0.03,
      M["dark"])
for k in range(5):
    y = 31 + 4 * k
    prism(f"Header_GrillSlat_{k}", ph, [rrect_px(297, y, 343, y + 2.4, 1)], -0.05, -0.012,
          M["steel"], bevel=0.001)
cylinder_between("Header_Bar", ph.world_px(285, 14, 0.012), ph.world_px(355, 14, 0.012), 0.006,
                 M["steel"])
pl = Panel(190, 15, 0.88, facing=1.0)
prism("Header_ClampsL", pl, [inset([(138, -20), (244, -20), (242, 24), (238, 18), (218, 18),
                                    (214, 20), (206, 22), (198, 24), (194, 24), (190, 19),
                                    (186, 25), (182, 25), (178, 20), (174, 26), (166, 26),
                                    (162, 28), (154, 31), (146, 33), (138, 35)])],
      -0.06, 0.0, M["teal"])
pr = Panel(470, 20, 0.88, facing=1.0)
prism("Header_BoxR", pr, [inset([(398, -20), (500, -20), (500, 35), (455, 35), (455, 18),
                                 (398, 22)])], -0.07, -0.01, M["beige"])
cable("Pipe_HeaderR_Upper", [(396, 14, 0.87), (420, 22, 0.86), (450, 28, 0.85), (480, 37, 0.85),
                             (505, 46, 0.85), (528, 47, 0.84)], M["steel"], r_px=2.6, sides=10,
      start=True)
cable("Pipe_HeaderR_Lower", [(398, 20, 0.86), (414, 32, 0.85), (436, 43, 0.84), (462, 49, 0.84),
                             (492, 51, 0.84), (518, 49, 0.84), (530, 45, 0.83)], M["steel"],
      r_px=3.0, sides=10, start=True)

# --- console: master monitor ------------------------------------------------------------------
MM_BEZEL = [(207, 340), (214, 328), (226, 318), (230, 305), (236, 298), (248, 294), (290, 293),
            (298, 296), (320, 299), (325, 304), (329, 307), (341, 307), (345, 303), (352, 300),
            (404, 300), (410, 304), (414, 318), (432, 330), (450, 345), (452, 560), (205, 560)]
pc = rect_monitor("MasterMonitor", 0.70, inset(MM_BEZEL),
                  screen_px=rrect_px(235, 321, 410, 467, 12),
                  body_px=rrect_px(215, 310, 445, 580, 14), body_z=(-0.28, -0.04),
                  mat_bezel=M["beige"])
for k, x in enumerate((290, 350)):
    prism(f"MasterMonitor_LightBar_{k}", pc, [rrect_px(x, 313, x + 35, 318, 1.5)], -0.01, 0.006,
          M["hose"], bevel=0.002)
for k in range(4):
    y = 362 + 12 * k
    prism(f"MasterMonitor_ButtonL_{k}", pc, [rrect_px(220, y, 231, y + 9, 1.5)], -0.01, 0.012,
          M["beige"], bevel=0.002)
for k in range(7):
    y = 362 + 11 * k
    prism(f"MasterMonitor_ButtonR_{k}", pc, [rrect_px(432, y, 449, y + 8, 1.5)], -0.01, 0.012,
          M["beige"], bevel=0.002)
prism("MasterMonitor_Indicator", pc, [rrect_px(415, 376, 422, 394, 1.5)], -0.01, 0.008,
      M["hose"], bevel=0.002)
for k, x in enumerate((247.5, 258.5, 269.5)):
    ellipsoid(f"MasterMonitor_Button_{k}", pc.world_px(x, 476, 0.002), (0.006,) * 3, M["hose"])
top = pc.world_px(321, 540, -0.15)
box_between("MasterMonitor_Pedestal", top, (top[0], 0.74, L.FLOOR_Z - 0.02), 0.22, 0.16,
            M["dark"], bevel=0.015)

# bracket where the right strut lands (carries the master monitor lamp)
pbk = Panel(430, 320, 0.73)
prism("StrutClampR", pbk, [inset([(392, 300), (420, 292), (440, 282), (468, 300), (468, 335),
                                  (452, 352), (428, 352), (418, 340), (412, 318), (394, 310)])],
      -0.06, 0.0, M["teal"], bevel=0.006)
lamp_disc("Lamp_MasterMonitor", pbk, *des_lamp("MasterMonitor"), r_px=5)
cable("Pipe_StrutL", [(160, 352, 0.76), (182, 340, 0.74), (205, 333, 0.72), (224, 320, 0.71),
                      (238, 308, 0.70), (246, 300, 0.71)], M["steel"], r_px=4, sides=10)
cable("Pipe_ConsoleR", [(410, 405, 0.735), (418, 425, 0.70), (428, 447, 0.70), (434, 460, 0.70),
                        (452, 462, 0.71), (470, 470, 0.73), (490, 490, 0.76)], M["steel"],
      r_px=5, sides=10, end=True)

# --- console: side monitors ---------------------------------------------------------------------
SM_BEZEL = {
    "SlaveMonitor0": [(80, 356), (86, 350), (100, 346), (140, 346), (160, 341), (200, 342),
                      (208, 350), (208, 458), (160, 460), (100, 460), (84, 452), (78, 440),
                      (78, 370)],
    "SlaveMonitor1": [(445, 344), (470, 339), (490, 343), (520, 347), (546, 346), (560, 350),
                      (568, 362), (570, 440), (564, 455), (548, 460), (500, 460), (445, 458)],
}
SM_SCREEN = {"SlaveMonitor0": (100, 361, 200, 441), "SlaveMonitor1": (447, 361, 548, 441)}
SM_WING = {"SlaveMonitor0": (80, 208), "SlaveMonitor1": (445, 568)}
SM_BRACKET = {
    "SlaveMonitor0": [(80, 368), (85, 355), (98, 351), (102, 356), (102, 452), (96, 458),
                      (84, 455), (79, 440)],
    "SlaveMonitor1": [(545, 356), (549, 351), (561, 354), (568, 367), (569, 440), (564, 455),
                      (552, 458), (545, 452)],
}
for sec in ("SlaveMonitor0", "SlaveMonitor1"):
    x0, y0, x1, y1 = SM_SCREEN[sec]
    ps = rect_monitor(sec, 0.72, inset(SM_BEZEL[sec]), screen_px=rrect_px(x0, y0, x1, y1, 7),
                      facing=0.9, body_px=rrect_px(x0 - 8, y0 - 4, x1 + 8, y1 + 14, 8),
                      body_z=(-0.22, -0.035), mat_bezel=M["navy"])
    prism(f"{sec}_Bracket", ps, [inset(SM_BRACKET[sec], 1.5)], -0.05, 0.016,
          M["bronze"] if sec.endswith("0") else M["teal"], bevel=0.006)
    prism(f"{sec}_BottomBar", ps, [rrect_px(x0 + 2, 440, x1 - 2, 447, 2)], -0.02, 0.008,
          M["beige"], bevel=0.002)
    for k in range(3):
        px = x0 + 22 + 10 * k
        prism(f"{sec}_Plug_{k}", ps, [rrect_px(px, 444, px + 6, 456, 1)], -0.02, 0.012,
              M["beige"], bevel=0.0015)
    lamp_disc(f"Lamp_{sec}", ps, *des_lamp(sec), r_px=5, z=0.018)
    # console wing below the monitor, joining it to the master monitor
    prism(f"{sec}_Wing", ps, [rrect_px(SM_WING[sec][0], 448, SM_WING[sec][1], 560, 6)],
          -0.20, -0.03, M["dark"])

# --- hoses, bars and coils in the lower corners -------------------------------------------------
for side, mx in (("L", list), ("R", mirror)):
    # side console block under the window sill, behind bar and coil
    prism(f"SideBlock_{side}", Panel(*mx([(30, 445)])[0], 0.97, 1.0),
          [mx([(-60, 399), (84, 399), (84, 500), (-60, 500)])], -0.06, 0.0, M["dark"])
    cable(f"Bar_Low_{side}", mx([(80, 412, 0.80), (40, 418, 0.86), (0, 426, 0.92),
                                 (-80, 432, 0.98)]), M["steel"], r_px=6, sides=10, end=True)
    cable(f"Coil_Low_{side}", mx([(70, 446, 0.80), (40, 446, 0.85), (10, 448, 0.90),
                                  (-40, 452, 0.97)]), M["steel"], r_px=8.5, pitch=0.012,
          amp=0.6, sides=12, end=True)

finish()
