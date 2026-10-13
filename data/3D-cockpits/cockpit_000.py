"""
Cockpit 000 (porthole cockpit: oval window in a ribbed green hull, top monitor, console with the
master monitor, two gear-rimmed sonar screens, hoses). Layout traced from VFX/COCKPIT/000.

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
      --python cockpit_000.py -- [--data DIR] [--out DIR] [--no-render | --quick]
"""

import math
import os
import sys

import numpy as np
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cockpitlib import *  # noqa: E402,F401,F403
import cockpitlib as L  # noqa: E402

L.setup("000")
set_hull(center=(0.0, 0.05, -0.22), axes=(1.15, 1.12, 0.98), floor_z=-1.0)


# --- hull with the oval window ------------------------------------------------------------------
# Window outline fitted to the see-through area of COCKPIT.R16: superellipse, centre 318/225,
# a=295, b=168 on top (n=2) and b=150 below (n=2.5).
def window_outline_px(t):
    c, s = np.cos(t), np.sin(t)
    top = s >= 0
    x = 318.0 + 295.0 * np.sign(c) * np.abs(c) ** np.where(top, 1.0, 0.8)
    y = 225.0 - np.where(top, 168.0 * s, 150.0 * np.sign(s) * np.abs(s) ** 0.8)
    return x, y


win = Porthole(window_outline_px, (318.0, 225.0))
win.shell()
win.glass()
NT = 256
tt = np.linspace(0, 2 * np.pi, NT, endpoint=False)
ox, oy = window_outline_px(tt)

# window rim (rounded lip), placed so its inner silhouette follows the outline
RIM_R = 0.034
H0 = win.point(tt, 0.0)
Ts = unit(win.point(tt, 0.004) - H0)
Nin = hull_normal_in(H0)
rim_c = H0 + Ts * RIM_R * 1.05 - unit(H0) * RIM_R * 0.8  # in front of the cut edge
Pr, sr = resample(np.column_stack([rim_c, Nin]), 0.01, closed=True)
tube("Window_Rim", Pr[:, :3], RIM_R * (1 + 0.05 * np.cos(2 * np.pi * sr / 0.11)), M["rim"],
     COLL["Hull"], sides=14, closed=True, normals=Pr[:, 3:])

# knuckles / clamps on the rim
for k, (px, py) in enumerate(((75, 127), (160, 79), (483, 77), (565, 128))):
    i = int(np.argmin((ox - px) ** 2 + (oy - py) ** 2))
    tng = Vector(unit(rim_c[(i + 1) % NT] - rim_c[i - 1]).tolist())
    nin = Vector(Nin[i].tolist())
    rot = Matrix((tng, nin.cross(tng), nin)).transposed()
    c = rim_c[i] - Ts[i] * 0.012 + Nin[i] * 0.012
    ellipsoid(f"Rim_Clamp_{k}", c, (0.065, 0.045, 0.05), M["rim"], COLL["Hull"], rot=rot)

# radial ribs around the window
NR, S_END = 96, 0.36
rib_s = np.linspace(0.018, S_END, 30)
rib_V, rib_F = [], []
prof = [(-0.5, -0.15, 0), (0.5, -0.15, 0), (0.5, 0.75, 1), (0.3, 1.0, 1), (-0.3, 1.0, 1),
        (-0.5, 0.75, 1)]
for k in range(NR):
    t = 2 * np.pi * (k + 0.5) / NR
    H = win.point(np.full_like(rib_s, t), rib_s)
    T, N, B = frames(H, hull_normal_in(H))
    q = rib_s / S_END
    w = 0.020 + 0.020 * q
    h = 0.012 + 0.075 * np.sqrt(np.clip(1 - q ** 2, 0, 1))
    ring = [H + B * (bu * w)[:, None] + N * (nv * (h if sh else np.full_like(h, 0.02)))[:, None]
            for bu, nv, sh in prof]
    off = sum(len(v) for v in rib_V)
    rib_V.append(np.stack(ring, 1).reshape(-1, 3))
    rib_F += [tuple(off + i for i in f) for f in loft_faces(len(rib_s), len(prof), False, True, True)]
recalc_normals(mesh_obj("Hull_Ribs", np.vstack(rib_V), rib_F, M["rib"], COLL["Hull"], sharp=50))

# ring frames behind the ribs
for k, s in enumerate((0.45, 0.58, 0.71, 0.84)):
    H = win.point(tt, s)
    nin = hull_normal_in(H)
    tube(f"Hull_Frame_{k}", H + nin * 0.03, 0.03, M["rib"], COLL["Hull"], sides=10, closed=True,
         normals=nin)

standard_interior()

# --- top monitor (SlaveMonitor0) ----------------------------------------------------------------
pt = Panel(317, 46, 0.72)
prism("SlaveMonitor_Bezel", pt, [rrect_px(252, -18, 381, 96, 9), rrect_px(266, 7, 368, 85, 6)],
      -0.035, 0.0, M["bronze"])
prism("SlaveMonitor_Body", pt, [rrect_px(258, -14, 375, 90, 10)], -0.17, -0.03, M["dark"])
prism("SlaveMonitor_BottomBar", pt, [rrect_px(262, 84, 376, 102, 4)], -0.06, 0.008, M["bronze"])
for k, x in enumerate((323, 333, 343)):
    prism(f"SlaveMonitor_Plug_{k}", pt, [rrect_px(x, 96, x + 7, 105, 1.5)], -0.03, 0.004,
          M["bronze_dk"], bevel=0.002)
for k, x0 in enumerate((240, 379)):
    prism(f"SlaveMonitor_Bracket_{k}", pt, [rrect_px(x0, -10, x0 + 14, 62, 3)], -0.13, -0.02,
          M["bronze_dk"])
bolt = pt.world_px(362.5, 92.5, 0.01)
cylinder_between("SlaveMonitor_Bolt", bolt - pt.n * 0.012, bolt, 0.006, M["dark"], sides=12)
screen("Screen_SlaveMonitor0", pt, pt.poly(rrect_px(266, 7, 368, 85, 6)), -0.018, M["screen"],
       des_section="SlaveMonitor0", des_rect=des_rect("SlaveMonitor0"))
top_c = pt.world_px(316, -10, -0.10)
mount_up("SlaveMonitor_Mount", top_c, 0.032)
for k, dx in enumerate((-0.07, 0.07)):
    mount_up(f"SlaveMonitor_Strut_{k}", top_c + np.array([dx, 0.03, 0]), 0.012, M["bronze_dk"])
cable("SlaveMonitor_CableL", [(244, -40, 0.82), (233, 3, 0.76), (226, 30, 0.74), (229, 55, 0.73),
                              (241, 70, 0.72), (257, 74, 0.71)], M["cable"],
      r_px=4.5, pitch=0.008, start=True)
cable("SlaveMonitor_CableR", [(394, -40, 0.82), (405, 3, 0.76), (410, 30, 0.74), (408, 58, 0.73),
                              (396, 72, 0.72), (380, 76, 0.71)], M["cable"],
      r_px=4.5, pitch=0.008, start=True)

# --- console with the master monitor ------------------------------------------------------------
pc = Panel(314, 387, 0.74)
bezel_outline = [(205, 325), (209, 314), (222, 311), (238, 311), (240, 295), (246, 290),
                 (262, 289), (268, 296), (270, 303), (288, 303), (290, 290), (344, 290),
                 (346, 303), (360, 303), (362, 292), (372, 290), (410, 292), (416, 300),
                 (418, 316), (418, 560), (205, 560)]
prism("MasterMonitor_Bezel", pc, [bezel_outline, rrect_px(226, 316, 402, 458, 10)],
      -0.045, 0.0, M["bronze"], bevel=0.006)
prism("MasterMonitor_Body", pc, [rrect_px(212, 300, 412, 600, 14)], -0.32, -0.04, M["dark"])
prism("MasterMonitor_Grill", pc, [rrect_px(290, 290, 344, 308, 3)], -0.09, 0.02, M["beige"])
for k in range(5):
    x = 294 + 10 * k
    prism(f"MasterMonitor_GrillSlot_{k}", pc, [rrect_px(x, 298, x + 7, 305, 1)], 0.012, 0.026,
          M["dark"], bevel=0.001)
prism("MasterMonitor_LightBar", pc, [rrect_px(272, 461, 352, 467, 2)], -0.01, 0.01, M["beige"],
      bevel=0.002)
prism("MasterMonitor_Recess", pc, [rrect_px(276, 469, 350, 495, 2)], -0.02, 0.002, M["dark"],
      bevel=0.002)
for k, (x, y) in enumerate(((402.5, 306), (226, 300))):
    p = pc.world_px(x, y, 0.0)
    cylinder_between(f"MasterMonitor_Bolt_{k}", p - pc.n * 0.01, p + pc.n * 0.006, 0.008,
                     M["dark"], sides=12)
screen("Screen_MasterMonitor", pc, pc.poly(rrect_px(224, 314, 404, 460, 11)), -0.028, M["screen"],
       des_section="MasterMonitor", des_rect=des_rect("MasterMonitor"))
lamp("Lamp_Console", pc, 412, 367)
bot = pc.world_px(314, 590, -0.18)
box("MasterMonitor_Pedestal", (bot[0], bot[1], (bot[2] + L.FLOOR_Z) / 2),
    (0.30, 0.22, abs(bot[2] - L.FLOOR_Z) + 0.05), M["dark"], bevel=0.015)

# --- sonar monitors (RoundMonitor0/1) -----------------------------------------------------------
pl = round_monitor("RoundMonitor0", 0.74, 36, center_px=(108, 409), housing_center_px=(105, 400),
                   housing_r_px=55, teeth=9, tooth_px=6, stalk_to_floor=True)
prism("RoundMonitor0_Bracket", pl, [sector(105, 400, 49, 57, -100, 5)], -0.07, -0.01,
      M["bronze_dk"])
lamp("Lamp_RoundMonitor0", pl, 85, 366, r=0.008, z=0.006)
box_between("RoundMonitor0_ArmUpper", at(150, 394, 0.76), at(222, 390, 0.78), 0.045, 0.032,
            M["bronze_dk"])
box_between("RoundMonitor0_ArmLower", at(148, 430, 0.735), at(200, 428, 0.745), 0.05, 0.035,
            M["beige"], bevel=0.01)

pr = round_monitor("RoundMonitor1", 0.74, 36, center_px=(529, 400), housing_center_px=(527, 401),
                   housing_r_px=55, teeth=9, tooth_px=6, stalk_to_floor=True)
prism("RoundMonitor1_Bracket", pr, [rrect_px(472, 365, 495, 397, 4)], -0.06, 0.01, M["bronze_dk"])
for k, (x, y) in enumerate(((525, 447), (545, 447), (565, 439))):
    b = pr.world_px(x, y, 0.0)
    cylinder_between(f"RoundMonitor1_Bolt_{k}", b - pr.n * 0.01, b + pr.n * 0.012, 0.0065,
                     M["bronze"], sides=10)
box_between("RoundMonitor1_Arm", at(416, 400, 0.76), at(476, 395, 0.75), 0.035, 0.03,
            M["bronze_dk"])
pb = Panel(451, 424, 0.70)
prism("RoundMonitor1_Junction", pb, [rrect_px(435, 414, 467, 434, 3)], -0.035, 0.0, M["beige"],
      bevel=0.005)

# --- hoses and cables ---------------------------------------------------------------------------
cable("Hose_TopLeft", [(-90, -35, 0.95), (-30, 5, 0.90), (10, 30, 0.86), (45, 55, 0.80),
                       (85, 70, 0.76), (118, 66, 0.77), (140, 45, 0.82), (150, 15, 0.87),
                       (153, -20, 0.92), (152, -60, 0.97)], M["hose"],
      r=0.056, pitch=0.026, amp=0.16, sides=18, start=True, end=True)
cable_bundle("CableBundle_TopRight", [
    (486, -60, 0.95), (490, 0, 0.90), (505, 27, 0.88), (525, 57, 0.87), (552, 87, 0.86),
    (580, 110, 0.86), (604, 118, 0.86), (628, 104, 0.87), (645, 80, 0.89), (668, 40, 0.94)],
    6, 0.005, M["hose"], spacing=0.0105)
cable_specs = {  # name: (control points, anchor the end)
    "Cable_ConsoleToSonarL": ([(266, 298, 0.71), (247, 292, 0.70), (224, 293, 0.69),
                               (201, 302, 0.69), (183, 316, 0.70), (168, 334, 0.71),
                               (154, 348, 0.72), (141, 357, 0.73)], False),
    "Cable_SonarLToWall": ([(100, 348, 0.74), (70, 340, 0.78), (42, 337, 0.83), (15, 343, 0.90),
                            (-20, 355, 0.98)], True),
    "Cable_SonarLDown_0": ([(78, 452, 0.74), (55, 463, 0.76), (30, 478, 0.80), (0, 500, 0.85)], True),
    "Cable_SonarLDown_1": ([(100, 449, 0.74), (90, 468, 0.75), (82, 490, 0.77), (78, 520, 0.80)], True),
    "Cable_SonarLDown_2": ([(122, 449, 0.74), (118, 470, 0.75), (115, 495, 0.76), (113, 525, 0.78)], True),
    "Cable_ConsoleToSonarR": ([(414, 380, 0.74), (437, 384, 0.73), (458, 397, 0.72),
                               (471, 422, 0.72), (475, 448, 0.72), (473, 478, 0.72),
                               (468, 520, 0.73)], True),
}
for name, (ctrl, loose) in cable_specs.items():
    cable(name, ctrl, M["cable"], r=0.012, pitch=0.0095, amp=0.22, sides=10, end=loose)
cable("Hose_BottomRight", [(360, 520, 0.62), (395, 478, 0.62), (425, 462, 0.62), (455, 456, 0.62),
                           (485, 462, 0.63), (512, 476, 0.64), (545, 505, 0.66)],
      M["bronze_dk"], r=0.023, pitch=0.016, amp=0.2, sides=14, start=True, end=True)
cable("Hose_Right", [(600, 540, 0.80), (590, 470, 0.80), (596, 430, 0.82), (615, 400, 0.85),
                     (640, 385, 0.88), (690, 368, 0.97)], M["cable"],
      r=0.043, pitch=0.022, amp=0.18, sides=16, start=True, end=True)
cable("Hose_RightUpper", [(552, 392, 0.93), (562, 352, 0.87), (600, 343, 0.88), (630, 350, 0.92),
                          (680, 364, 1.0)], M["cable"],
      r=0.024, pitch=0.016, amp=0.2, sides=12, start=True, end=True)

finish()
