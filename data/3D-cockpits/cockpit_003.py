"""
Cockpit 003 (open canopy: a thick ring frame arching over the view with two round sonar screens on
its shoulders, rectangular monitors hanging in the top corners, the master monitor console at the
bottom, ribbed feet of the arch and big corrugated hoses in the bottom corners). Layout traced from
VFX/COCKPIT/003.

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
      --python cockpit_003.py -- [--data DIR] [--out DIR] [--no-render | --quick]
"""

import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cockpitlib import *  # noqa: E402,F401,F403
import cockpitlib as L  # noqa: E402

L.setup("003")
set_hull(center=(0.0, 0.1, -0.1), axes=(1.35, 1.3, 1.15), floor_z=-1.0)


# --- the arch plane -------------------------------------------------------------------------------
# The arch keeps an almost constant width in the art, which fits a frame lying in one plane nearly
# parallel to the image plane: y = ARCH_Y0 + ARCH_KZ * z (the top leans slightly forward).
ARCH_Y0, ARCH_KZ = 0.70, 0.10


def arch_dist(px, py):
    """Distance from the eye to the arch plane along the ray through pixel (px, py)."""
    d = unit(ray_np(px, py))
    return float(ARCH_Y0 / (d[1] - ARCH_KZ * d[2]))


def on_arch(pts):
    """(px, py, n_px) -> (px, py, dist, n_px) on the arch plane."""
    return [(x, y, arch_dist(x, y), n) for x, y, n in pts]


# --- hull: a glass canopy ------------------------------------------------------------------------
# Inside the frame the hull is glass down to the bottom edge of the window (lowest see-through
# pixel per column), also behind the arch and the monitors: the modelled parts carry that art, and
# an opaque imprint of them on the hull would show beside them once the head moves. Outside the
# frame: a band at the height of the side windows and a roof strip between the struts, bounded by
# constant azimuth/elevation so the coarse hull faces there don't make staircases.
SILL = np.array([np.nonzero(L.SEE[:, x])[0].max() for x in range(640)])


def canopy(az, el):
    """glass_fn for masked_hull()."""
    a, e = math.radians(az), math.radians(el)
    d = np.array([math.sin(a) * math.cos(e), math.cos(a) * math.cos(e), math.sin(e)])
    if d[1] > 0.02:
        x, y = to_px(d)
        if 0 <= x < 640 and 0 <= y < 480:
            return y <= SILL[int(x)]
    return (abs(az) <= 74 and -12 <= el <= 33) or (abs(az) <= 27 and el <= 64)


masked_hull(glass_fn=canopy)
standard_interior()

# --- the arch -----------------------------------------------------------------------------------
# Centre line and width traced from the see-through boundary inside the arch and the outer edge
# (hidden behind the round monitors, where it runs from the side pieces into the top bar).
# The arch is a flat frame, so the traced centre stays put (recentre=False).
arch_px = [
    (-4, 353, 76), (7, 319, 73), (17, 297, 72), (25, 283, 71), (38, 264, 70), (55, 245, 67),
    (74, 216, 72), (94, 193, 70), (116, 172, 73), (137, 158, 70), (163, 137, 90), (192, 118, 72),
    (218, 106, 64), (245, 102, 54), (275, 98, 54), (296, 95, 54), (320, 94, 54), (346, 96, 54),
    (362, 98, 54), (393, 102, 54), (420, 108, 64), (446, 119, 72), (475, 137, 90), (502, 159, 66),
    (525, 172, 70), (544, 192, 65), (566, 215, 72), (581, 238, 68), (598, 259, 68),
    (616, 288, 72), (628, 313, 73), (642, 353, 76)]
beam("Arch", on_arch(arch_px), M["rib"], h_ratio=0.85, round_frac=0.6, recentre=False)

# ribbed feet of the arch (red/black bellows: a collar and a bulb) on columns down to the floor;
# traced edge towards the window, the outer side is out of frame
feet = {  # side: (axis x, edge side, traced edge)
    "L": (20, 1, [(40, 335), (47, 336), (49, 338), (51, 340), (55, 344), (58, 350), (61, 356),
                  (63, 362), (66, 366), (72, 367), (77, 368), (89, 372), (94, 376), (97, 380),
                  (100, 386), (101, 392), (100, 398), (96, 404), (92, 410), (85, 418), (70, 426),
                  (50, 432), (25, 438)]),
    "R": (618, -1, [(597, 335), (590, 336), (587, 338), (585, 340), (581, 344), (578, 350),
                    (575, 356), (573, 362), (570, 366), (565, 367), (560, 368), (547, 372),
                    (542, 376), (540, 380), (537, 386), (536, 392), (537, 398), (540, 404),
                    (545, 410), (552, 418), (566, 426), (586, 432), (612, 438)]),
}
for side, (ax, es, edge) in feet.items():
    bulb(f"Arch_Foot{side}", ax, 328, 470, 1.08, edge, es, M["bronze_dk"], amp=0.06)
    b = at(ax, 440, 1.08)
    cylinder_between(f"Arch_FootColumn{side}", b, (b[0], b[1], L.FLOOR_Z), 0.07, M["dark"])

# struts from the shoulders of the arch up into the ceiling (the dark wedges beside the monitors;
# only their inner edges show, the rest hides behind the slave monitors)
beam("Strut_L", [(192, 80, arch_dist(192, 80), 44), (177, 50, 1.0, 44), (148, 0, 0.97, 44),
                 (131, -30, 0.96, 44), (119, -122, 1.11, 44)], M["rib"], h_ratio=0.9, end=True)
beam("Strut_R", [(448, 80, arch_dist(448, 80), 44), (469, 48, 1.0, 44), (497, 0, 0.97, 44),
                 (514, -30, 0.96, 44), (526, -122, 1.11, 44)], M["rib"], h_ratio=0.9, end=True)

# --- round monitors on the shoulders of the arch ------------------------------------------------
rm_specs = {  # section: (screen centre, top band angles, block, knob, bracket, skirt below)
    "RoundMonitor0": ((185, 112), (-160, -35), rrect_px(212, 74, 242, 106, 4),
                      rrect_px(218, 62, 234, 86, 6),
                      [(203, 142), (236, 137), (229, 157), (224, 163), (210, 170), (203, 166)],
                      [(160, 150), (216, 150), (216, 169), (198, 171), (186, 174), (183, 176),
                       (165, 180)]),
    "RoundMonitor1": ((451, 111), (-145, -20), rrect_px(393, 72, 422, 106, 4),
                      rrect_px(401, 60, 420, 85, 6),
                      [(404, 138), (436, 138), (437, 171), (424, 169), (418, 164), (409, 158),
                       (404, 149)],
                      [(425, 150), (480, 150), (479, 180), (477, 178), (454, 176), (452, 174),
                       (445, 172), (437, 170), (424, 168)]),
}
for sec, ((cx, cy), (a0, a1), block, knob, bracket, skirt) in rm_specs.items():
    p = round_monitor(sec, 0.80, 34, housing_r_px=46, center_px=(cx, cy), facing=0.5, depth=0.05,
                      back_depth=0.17, mat_housing=M["dark"])
    # red band over the top, the block with the beige knob towards the top bar, bracket below
    prism_eye(f"{sec}_TopBand", p, [sector(cx, cy, 45, 65, a0, a1)], -0.035, 0.012, M["bronze"])
    prism_eye(f"{sec}_Block", p, [block], -0.04, 0.015, M["bronze"])
    prism_eye(f"{sec}_Knob", p, [knob], -0.02, 0.03, M["beige"], bevel=0.004)
    prism_eye(f"{sec}_Bracket", p, [bracket], -0.03, 0.02, M["beige"])
    prism_eye(f"{sec}_Skirt", p, [skirt], -0.06, -0.01, M["dark"])
    # the screen can sits on the arch
    cylinder_between(f"{sec}_Mount", p.world(0, 0, -0.15), at(cx, cy, arch_dist(cx, cy)), 0.035,
                     M["dark"])
    lamp(f"Lamp_{sec}", p, *des_lamp(sec), z=0.016)
cable("Cable_RoundMonitor0", [(236, 122, 0.80, 5), (242, 132, 0.80, 5.5), (243, 148, 0.80, 5.5),
                              (241.5, 158, 0.80, 5.5), (236, 168, 0.80, 5.5), (227, 172, 0.80, 5),
                              (221, 167, 0.80, 5)], M["cable"], pitch=0.008, amp=0.25, sides=10)

# --- slave monitors in the top corners ----------------------------------------------------------
sm_specs = {  # section: (housing outline, screen outline, hanging from)
    "SlaveMonitor0": ([(-30, -35), (152, -35), (152, 96), (142, 108), (100, 112), (20, 112),
                       (18, 110), (16, 106), (7, 104), (0, 101), (-30, 96)],
                      rrect_px(18, 7, 128, 92, 8), (70, -20)),
    "SlaveMonitor1": ([(488, -35), (670, -35), (670, 98), (632, 103), (622, 105), (619, 110),
                       (617, 113), (505, 113), (488, 96)], rrect_px(512, 7, 622, 92, 8),
                      (570, -20)),
}
for sec, (housing, scr, (hx, hy)) in sm_specs.items():
    x, y, w, h = des_rect(sec)
    ps = Panel(x + w / 2, y + h / 2, 0.82)
    prism_eye(f"{sec}_Bezel", ps, [housing, scr], -0.04, 0.0, M["bronze"])
    prism(f"{sec}_Body", ps, [inset(ps.poly(housing), 0.02)], -0.2, -0.035, M["dark"], local=True)
    screen(f"Screen_{sec}", ps, ps.poly(scr), -0.022, M["screen"], des_section=sec,
           des_rect=des_rect(sec))
    lamp(f"Lamp_{sec}", ps, *des_lamp(sec))
    mount_up(f"{sec}_Mount", ps.world_px(hx, hy, -0.12), 0.03)

# the cable loop under the left monitor, the cable and the support post under the right one
cable("Cable_SlaveMonitor0", [(31, 104, 0.82, 7.5), (31.5, 115, 0.82, 7.5), (35.5, 125, 0.82, 7.5),
                              (41, 135, 0.82, 8), (49, 143, 0.82, 8), (59, 147, 0.82, 8),
                              (69, 143, 0.82, 8), (77, 135, 0.82, 8), (83.5, 125, 0.82, 7.5),
                              (90.5, 115, 0.82, 7.5), (92, 106, 0.82, 7.5)], M["cable"],
      pitch=0.012, amp=0.25, sides=12)
cable("Cable_SlaveMonitor1", [(534, 106, 0.82, 7), (535, 118, 0.84, 7), (538, 128, 0.88, 7),
                              (543, 138, 0.93, 7), (550, 147, 0.98, 7)], M["cable"], pitch=0.012,
      amp=0.25, sides=12)
beam("SlaveMonitor1_Post", [(563, 104, 0.83, 14), (563, 126, 0.9, 14), (561, 146, 0.97, 13)],
     M["dark"], h_ratio=0.9)

# --- console with the master monitor ------------------------------------------------------------
pc = Panel(315, 395, 0.66)
console_outline = [
    (176, 389), (180, 382), (184, 374), (185, 364), (188, 358), (191, 352), (193, 344), (195, 336),
    (197, 328), (203, 324), (206, 320), (210, 316), (216, 312), (224, 310), (238, 310), (240, 315),
    (246, 315), (248, 308), (254, 306), (262, 305), (273, 304), (283, 305), (295, 306), (304, 306),
    (306, 311), (322, 312), (326, 308), (328, 306), (375, 306), (379, 308), (381, 314), (386, 315),
    (394, 314), (397, 308), (401, 306), (410, 306), (416, 308), (419, 312), (422, 318), (424, 326),
    (426, 334), (426, 350), (425, 364), (422, 371), (427, 382), (435, 400), (445, 410), (455, 420),
    (459, 440), (455, 520), (185, 520), (180, 450)]
screen_hole = rrect_px(229, 330, 401, 474, 10)
prism_eye("MasterMonitor_Bezel", pc, [console_outline, screen_hole], -0.045, 0.0, M["bronze"])
prism("MasterMonitor_Body", pc, [rrect_px(205, 318, 420, 600, 14)], -0.30, -0.04, M["dark"])
prism_eye("MasterMonitor_PostL", pc, [rrect_px(200, 328, 227, 430, 6)], -0.01, 0.022, M["beige"],
          bevel=0.005)
prism_eye("MasterMonitor_PanelR", pc, [[(404, 360), (422, 372), (432, 396), (451, 418),
                                        (455, 442), (440, 450), (404, 450)]], -0.01, 0.018,
          M["bronze"], bevel=0.005)
for k, x in enumerate((430, 443)):
    b = pc.world_px(x, 430, 0.018)
    cylinder_between(f"MasterMonitor_Knob_{k}", b - pc.n * 0.01, b + pc.n * 0.012, 0.0075,
                     M["dark"], sides=12)
screen("Screen_MasterMonitor", pc, pc.poly(screen_hole), -0.028, M["screen"],
       des_section="MasterMonitor", des_rect=des_rect("MasterMonitor"))
lamp("Lamp_MasterMonitor", pc, *des_lamp("MasterMonitor"))
bot = pc.world_px(315, 600, -0.18)
box("MasterMonitor_Pedestal", (bot[0], bot[1] + 0.1, (bot[2] + L.FLOOR_Z) / 2),
    (0.32, 0.2, abs(bot[2] - L.FLOOR_Z) + 0.05), M["dark"], bevel=0.015)

# dark cowl left and right of the console (its top forms the bottom edge of the window)
cowl_specs = {
    "L": ((140, 440), [(205, 384), (176, 388), (162, 390), (157, 392), (151, 394), (146, 396),
                       (141, 398), (135, 400), (130, 402), (128, 406), (125, 408), (116, 409),
                       (108, 412), (100, 414), (90, 414), (60, 430), (40, 520), (215, 520)]),
    "R": ((500, 430), [(415, 372), (423, 374), (433, 376), (441, 378), (447, 380), (453, 382),
                       (458, 384), (460, 388), (470, 390), (478, 390), (483, 392), (489, 394),
                       (494, 396), (499, 398), (505, 400), (510, 402), (512, 406), (520, 410),
                       (528, 410), (532, 412), (537, 414), (541, 416), (545, 418), (550, 420),
                       (556, 423), (580, 440), (600, 520), (415, 520)]),
}
for side, (c, outline) in cowl_specs.items():
    pw = Panel(*c, 0.74, facing=0.6)
    prism_eye(f"Console_Cowl{side}", pw, [outline], -0.14, 0.0, M["dark"], bevel=0.006)
    # a block under it, down to the floor (beside the pilot's legs)
    a, b = (pw.world_px(x, 520, -0.07) for x in ((60, 190) if side == "L" else (450, 580)))
    top = min(a[2], b[2]) + 0.02
    box(f"Console_CowlBase{side}", ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2 + 0.1,
                                     (top + L.FLOOR_Z) / 2),
        (abs(b[0] - a[0]), 0.32, top - L.FLOOR_Z), M["dark"], bevel=0.015)

# --- big corrugated hoses in the bottom corners -------------------------------------------------
# (they come out of the columns under the arch feet, loop round out of frame and end in the
# blocks under the cowl)
cable("Hose_L", [(20, 475, 1.12, None), (-73, 502, 0.96, None), (-126, 495, 0.76, None),
                 (-40, 462, 0.68, 27), (0, 454, 0.66, 27), (40, 442, 0.65, 27), (70, 438, 0.65, 27),
                 (92, 444, 0.66, 27), (102, 465, 0.68, 27), (110, 500, 0.74, None),
                 (125, 560, 0.85, None)], M["hose"], pitch=0.024, amp=0.16, sides=20)
cable("Hose_R", [(618, 474, 1.12, None), (713, 502, 0.96, None), (766, 495, 0.76, None),
                 (690, 470, 0.68, 27), (640, 456, 0.66, 27), (605, 441, 0.65, 27),
                 (575, 440, 0.65, 27), (550, 455, 0.66, 27), (532, 478, 0.68, 27),
                 (522, 510, 0.74, None), (515, 560, 0.85, None)], M["hose"], pitch=0.024, amp=0.16,
      sides=20)

finish()
