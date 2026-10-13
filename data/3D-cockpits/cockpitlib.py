"""
Shared toolkit for rebuilding the pre-rendered 2D cockpits of Schleichfahrt / Archimedean Dynasty
as 3D models (Blender, headless). A cockpit script (cockpit_NNN.py) calls setup("NNN"), builds its
geometry with the helpers below and ends with finish().

Conventions
  * Units are metres. Blender: +Z up, the pilot looks along +Y (glTF: +Y up, looks along -Z).
  * The pilot's eye is the origin (empty "Eye").
  * 2D layout values (pixels of the 640x480 COCKPIT.R16, rectangles of VFX/COCKPIT/NNN.DES) are
    lifted into 3D by casting rays through the cockpit camera of the DES file: ViewAngle (horizontal,
    over 640 px) and optical centre (VFX_StartX + VFX_FlightX, VFX_StartY + VFX_FlightY). Seen from
    "Eye" the model therefore lines up with the original 2D cockpit.
  * Screens are separate meshes named after their DES sections (Screen_MasterMonitor, ...), origin
    at the screen centre, local +Z towards the pilot, UVs 0..1 across the screen. The DES rectangle
    is stored as a custom property (exported as glTF extras) for render-to-texture.
"""

import math
import os
import plistlib
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

# ------------------------------------------------------------------------------------------------
# Configuration (filled in by setup())
# ------------------------------------------------------------------------------------------------
CID = None                  # cockpit id, e.g. "000"
DATA = None                 # Vertigo data directory (contains VFX/COCKPIT/...)
OUT = None                  # output directory for this cockpit
ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
DES = {}                    # parsed DES file: {section_lower: {key_lower: value_str}}
F, CX, CY = 245.55, 320.0, 240.0
ART = None                  # (480, 640, 3) float sRGB colours of COCKPIT.R16
SEE = None                  # (480, 640) bool: see-through (window) pixels of COCKPIT.R16
MIRROR_X = 319.5            # vertical symmetry axis of the art (see mirror())
HC = np.array([0.0, 0.05, -0.22])     # hull ellipsoid centre
HA = np.array([1.15, 1.12, 0.98])     # hull ellipsoid semi-axes
FLOOR_Z = -1.0
M = {}                      # materials
COLL = {}                   # collections
HERE = os.path.dirname(os.path.abspath(__file__))


def arg(name, default=None):
    return ARGV[ARGV.index(name) + 1] if name in ARGV else default


def log(*a):
    print(f"[cockpit {CID}]", *a, flush=True)


def find_ci(base, *parts):
    """Resolve a path case-insensitively (the game data mixes upper and lower case)."""
    p = base
    for part in parts:
        hit = next((e for e in os.listdir(p) if e.lower() == part.lower()), None)
        if hit is None:
            raise FileNotFoundError(os.path.join(p, part))
        p = os.path.join(p, hit)
    return p


def default_data_dir():
    if os.environ.get("VERTIGO_DATA"):
        return os.environ["VERTIGO_DATA"]
    plist = os.path.expanduser("~/Library/Preferences/com.vertigo.Vertigo.plist")
    if os.path.exists(plist):
        with open(plist, "rb") as f:
            d = plistlib.load(f).get("datadir")
        if d:
            return d
    conf = os.path.expanduser("~/.config/vertigo/Vertigo.conf")
    if os.path.exists(conf):
        for line in open(conf):
            if line.startswith("datadir="):
                return line.split("=", 1)[1].strip()
    raise SystemExit("Vertigo data directory unknown: pass --data DIR or set VERTIGO_DATA")


def parse_des(path):
    des, sec = {}, None
    for line in open(path, encoding="cp437"):
        line = line.strip()
        if not line or line.startswith(("*", ";")):
            continue
        if line.startswith("["):
            sec = des.setdefault(line.strip("[]").lower(), {})
        elif "=" in line and sec is not None:
            k, v = line.split("=", 1)
            sec[k.strip().lower()] = v.strip()
    return des


def des_int(section, key, default=0):
    return int(float(DES.get(section.lower(), {}).get(key.lower(), default)))


def load_r16(path):
    raw = np.fromfile(path, dtype='<u2').reshape(480, 640)
    rgb = np.stack([((raw >> 8) & 0xf8), ((raw >> 3) & 0xfc), ((raw << 3) & 0xf8)], -1) / 255.0
    return raw, rgb.astype(np.float32)


def see_through_mask(raw, min_size=40):
    """Colour key (pure black) pixels form the window; tiny components are genuine black art."""
    key = raw == 0
    lab = np.where(key, np.arange(key.size).reshape(key.shape), key.size)
    while True:
        m = lab.copy()
        m[1:] = np.minimum(m[1:], lab[:-1]); m[:-1] = np.minimum(m[:-1], lab[1:])
        m[:, 1:] = np.minimum(m[:, 1:], lab[:, :-1]); m[:, :-1] = np.minimum(m[:, :-1], lab[:, 1:])
        m = np.where(key, m, key.size)
        if np.array_equal(m, lab):
            break
        lab = m
    ids, counts = np.unique(lab[key], return_counts=True)
    return key & np.isin(lab, ids[counts >= min_size])


def symmetry_axis(mask, around, span=3.0):
    """x of the vertical axis (in half-pixel steps around `around`) that mirrors the see-through
    mask best onto itself: pixel column x maps to 2 * axis - x."""
    xs = np.arange(mask.shape[1])
    best = (2.0, around)
    for a in np.arange(around - span, around + span + 0.25, 0.5):
        m = np.rint(2 * a - xs).astype(int)
        ok = (m >= 0) & (m < len(xs))
        best = min(best, ((mask[:, xs[ok]] != mask[:, m[ok]]).mean(), a))
    return float(best[1])


def load(cid):
    """Read DES + COCKPIT.R16 of cockpit `cid` (camera, art, see-through mask)."""
    global CID, DATA, OUT, F, CX, CY, ART, SEE, MIRROR_X
    CID = cid
    DATA = arg("--data") or default_data_dir()
    OUT = arg("--out") or os.path.join(HERE, "out", cid)
    os.makedirs(os.path.join(OUT, "renders"), exist_ok=True)
    DES.clear()  # updated in place, so `from cockpitlib import *` sees it
    DES.update(parse_des(find_ci(DATA, "VFX", "COCKPIT", f"{cid}.DES")))
    angle = float(DES.get("camera", {}).get("viewangle", 105.0))
    F = 320.0 / math.tan(math.radians(angle) / 2)
    CX = des_int("front", "VFX_StartX") + des_int("front", "VFX_FlightX", 320)
    CY = des_int("front", "VFX_StartY") + des_int("front", "VFX_FlightY", 240)
    raw, ART = load_r16(find_ci(DATA, "VFX", "COCKPIT", cid, "COCKPIT.R16"))
    SEE = see_through_mask(raw)
    MIRROR_X = symmetry_axis(SEE, CX)
    log(f"camera: view angle {angle}, f={F:.1f}px, centre {CX}/{CY}; "
        f"{SEE.mean():.0%} of the image is see-through, symmetry axis x={MIRROR_X}")


def setup(cid):
    """load() the cockpit, reset the scene, create collections and materials."""
    load(cid)
    reset_scene()
    for n in ("Hull", "Instruments", "Cables", "Seat", "Markers", "Reference", "RenderRig"):
        COLL[n] = collection(n)
    COLL["Reference"].hide_render = True
    M.clear()
    M.update(
        hull=material("Hull", (0.020, 0.075, 0.060), 0.3, 0.45),
        rib=material("Ribs", (0.030, 0.150, 0.100), 0.5, 0.32, coat=0.4),
        rim=material("Rim", (0.040, 0.190, 0.130), 0.5, 0.28, coat=0.5),
        bronze=material("Bronze", (0.300, 0.170, 0.090), 0.7, 0.45),
        bronze_dk=material("BronzeDark", (0.120, 0.070, 0.040), 0.7, 0.5),
        beige=material("Beige", (0.560, 0.420, 0.300), 0.2, 0.5),
        dark=material("DarkMetal", (0.035, 0.030, 0.028), 0.6, 0.55),
        hose=material("HoseBlue", (0.110, 0.140, 0.240), 0.3, 0.35),
        cable=material("CableDark", (0.050, 0.035, 0.025), 0.4, 0.45),
        screen=material("Screen", (0.005, 0.02, 0.005), 0.0, 0.15, (0.15, 0.6, 0.12), 0.35),
        lamp=material("Lamp", (1.0, 0.85, 0.3), 0.0, 0.3, (1.0, 0.8, 0.25), 25.0),
        glass=material("Glass", (0.6, 0.8, 0.85), 0.0, 0.03, alpha=0.08),
        hud=material("HUD", (0.2, 1.0, 0.2), 0.0, 0.5, (0.2, 1.0, 0.2), 1.0, alpha=0.05),
        seat=material("Seat", (0.050, 0.050, 0.055), 0.2, 0.7),
        floor=material("Floor", (0.030, 0.040, 0.035), 0.6, 0.6),
        dummy=material("PilotDummy", (0.45, 0.45, 0.47), 0.0, 0.8),
    )


def set_hull(center=None, axes=None, floor_z=None):
    """Pressure hull ellipsoid around the eye (also used to anchor cables and mounts)."""
    global HC, HA, FLOOR_Z
    if center is not None:
        HC = np.asarray(center, float)
    if axes is not None:
        HA = np.asarray(axes, float)
    if floor_z is not None:
        FLOOR_Z = floor_z


def material(name, color, metallic=0.0, rough=0.5, emission=None, strength=0.0, alpha=1.0,
             transmission=0.0, coat=0.0):
    m = bpy.data.materials.new(name)
    try:
        m.use_nodes = True
    except Exception:
        pass
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    b.inputs["Alpha"].default_value = alpha
    b.inputs["Transmission Weight"].default_value = transmission
    b.inputs["Coat Weight"].default_value = coat
    if emission:
        b.inputs["Emission Color"].default_value = (*emission, 1.0)
        b.inputs["Emission Strength"].default_value = strength
    m.diffuse_color = (*(emission if emission else color), alpha)
    m.metallic, m.roughness = metallic, rough
    if alpha < 1.0:
        for attr, val in (("surface_render_method", 'BLENDED'), ("blend_method", 'BLEND')):
            try:
                setattr(m, attr, val)
            except Exception:
                pass
    return m


def reset_scene():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    for block in (bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.cameras,
                  bpy.data.lights, bpy.data.collections):
        for item in list(block):
            block.remove(item)
    sc = bpy.context.scene
    sc.unit_settings.system = 'METRIC'
    sc.unit_settings.scale_length = 1.0


def collection(name):
    c = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(c)
    return c


# ------------------------------------------------------------------------------------------------
# Projection: pixel <-> 3D
# ------------------------------------------------------------------------------------------------
def unit(v):
    v = np.asarray(v, float)
    return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-12)


def ray_np(px, py):
    """Direction (y = 1) through pixel (px, py) of the 640x480 cockpit image."""
    px, py = np.asarray(px, float), np.asarray(py, float)
    return np.stack([(px - CX) / F, np.ones_like(px), (CY - py) / F], -1)


def at(px, py, dist):
    """Point `dist` metres from the eye along the ray through pixel (px, py)."""
    return unit(ray_np(px, py)) * dist


def to_px(p):
    """Project 3D points (y > 0) to pixel coordinates."""
    p = np.asarray(p, float)
    return np.stack([CX + F * p[..., 0] / p[..., 1], CY - F * p[..., 2] / p[..., 1]], -1)


def sky_px(az, el):
    """Pixel coordinates (also outside the 640x480 frame) of the direction azimuth/elevation in
    degrees (azimuth positive to the right)."""
    t = math.tan(math.radians(az))
    return CX + F * t, CY - F * math.tan(math.radians(el)) * math.sqrt(1 + t * t)


def px_metres(px, py, dist, dx, dy):
    """Metres spanned by the pixel offset (dx, dy) centred on (px, py) at `dist`. Includes the
    radial stretch of the flat 105 deg image (up to 1.6x in the corners)."""
    a, b = at(px - dx / 2, py - dy / 2, dist), at(px + dx / 2, py + dy / 2, dist)
    return float(np.linalg.norm(b - a))


def bevel_px(px, py, dist, bevel):
    """A prism bevel grows the outline by about this many pixels."""
    return bevel / px_metres(px, py, dist, 1, 0)


# ------------------------------------------------------------------------------------------------
# Tracing helpers (pixel space)
# ------------------------------------------------------------------------------------------------
def mirror(pts, axis=None):
    """Mirror traced pixel points (x, y, ...) to the other side of the art's vertical symmetry axis
    (default MIRROR_X, measured in load(): 319.5 for 001, 320 for 002)."""
    a = MIRROR_X if axis is None else axis
    return [(2 * a - p[0], *p[1:]) for p in pts]


def inset(pts, d=1.0):
    """Offset a closed 2D outline (pixels or metres) inwards by d (outwards for d < 0). Mitred
    corners, limited at sharp spikes; d must stay below half the smallest feature, or edges flip."""
    P = np.asarray(pts, float)
    ccw = np.sum(P[:, 0] * np.roll(P[:, 1], -1) - np.roll(P[:, 0], -1) * P[:, 1]) > 0
    e = np.roll(P, -1, 0) - P
    n = unit(np.stack([-e[:, 1], e[:, 0]], -1)) * (1 if ccw else -1)   # inward edge normals
    b = unit(n + np.roll(n, 1, 0))
    return [tuple(q) for q in P + b * (d / np.maximum((b * n).sum(-1), 0.35))[:, None]]


def des_lamp(section):
    """Centre (pixels) of a DES section's lamp rectangle, where the game draws its status lamp."""
    s = DES[section.lower()]
    return (int(s["lampx1"]) + int(s["lampwidth"]) / 2,
            int(s["lampy1"]) + int(s["lampheight"]) / 2)


def fit_across(ctrl, w_px, near=0.0, recentre=True):
    """Members traced as a centre line (px, py, dist) plus an apparent width in pixels measured
    across the line in the image -> (control points, widths in metres) for beam()/cable().
    A member's cross-section lies perpendicular to the member and to the view ray, which the flat
    105 deg image shows skewed and stretched radially off-centre (up to 15 % wider near the edges).
    So each width is solved in the image, at the near side of the member (`near` metres in front of
    the centre line, scalar or per point), and each control point is moved across the line until
    both edges straddle the traced centre. recentre=False keeps the points, for flat members nearly
    parallel to the image plane, whose silhouette the flat image doesn't skew. w_px: scalar or per
    point; None marks a hidden point (e.g. out of frame), which keeps its position and gets an
    interpolated width."""
    ctrl = [tuple(c[:3]) for c in ctrl]
    n_pts = len(ctrl)
    wp = list(w_px) if np.ndim(w_px) else [w_px] * n_pts
    near = np.broadcast_to(np.asarray(near, float), (n_pts,))
    P2 = np.array([c[:2] for c in ctrl], float)
    N = unit(np.gradient(P2, axis=0)) @ np.array([[0.0, 1.0], [-1.0, 0.0]])   # image normals
    T3 = unit(np.gradient(path3d(ctrl), axis=0))
    out, w = [], []
    for (px, py, d), n, t, s, nr in zip(ctrl, N, T3, wp, near):
        if s is None:
            out.append((px, py, d))
            w.append(np.nan)
            continue
        c, W = np.array([px, py], float), 0.0
        for _ in range(4):
            P = at(*c, d - nr)
            Wd = unit(np.cross(t, -unit(P)))
            if W == 0.0:  # first guess from the local scale
                W = s * 0.02 / abs(np.dot(to_px(P + Wd * 0.01) - to_px(P - Wd * 0.01), n))
            e1, e2 = to_px(P - Wd * W / 2), to_px(P + Wd * W / 2)
            W *= s / abs(np.dot(e2 - e1, n))
            if recentre:
                c -= n * np.dot((e1 + e2) / 2 - (px, py), n)
        out.append((float(c[0]), float(c[1]), d))
        w.append(float(W))
    w = np.array(w)
    ok = ~np.isnan(w)
    k = np.arange(n_pts)
    return out, np.interp(k, k[ok], w[ok])


# ------------------------------------------------------------------------------------------------
# Hull ellipsoid
# ------------------------------------------------------------------------------------------------
def hull_dist(d, origin=(0.0, 0.0, 0.0)):
    """Distance along unit directions d from origin to the hull ellipsoid."""
    o = (np.asarray(origin, float) - HC) / HA
    dd = np.asarray(d, float) / HA
    a = (dd * dd).sum(-1)
    b = 2 * (dd * o).sum(-1)
    c = (o * o).sum() - 1
    return (-b + np.sqrt(np.maximum(b * b - 4 * a * c, 0))) / (2 * a)


def hull_normal_in(p):
    return -unit((np.asarray(p, float) - HC) / HA ** 2)


def on_hull(px, py, gap):
    """Control point (px, py, dist) `gap` metres in front of the hull, e.g. for a member that runs
    on out of the frame."""
    return (px, py, float(hull_dist(unit(ray_np(px, py)))) - gap)


class Porthole:
    """Hull with one window whose outline is given in pixel space (cockpit 000 style).
    Parameterisation: t = angle around the window, s = 0 at the rim .. 1 behind the pilot."""

    def __init__(self, outline_px, center_px):
        self.outline_px = outline_px        # t (array) -> (x, y) arrays
        self.center = center_px

    def dirs(self, t, s):
        t, s = np.broadcast_arrays(np.asarray(t, float), np.asarray(s, float))
        ox, oy = self.outline_px(t)
        dw = unit(ray_np(ox, oy))
        a0 = unit(ray_np(*self.center))
        cw = np.clip((dw * a0).sum(-1), -1, 1)
        e = unit(dw - a0 * cw[..., None])
        phi = np.arccos(cw) + (math.pi - np.arccos(cw)) * s
        return a0 * np.cos(phi)[..., None] + e * np.sin(phi)[..., None]

    def point(self, t, s, inset=0.0):
        d = self.dirs(t, s)
        return d * (hull_dist(d)[..., None] - inset)

    def shell(self, name="Hull", nt=256, ns=64):
        tt = np.linspace(0, 2 * np.pi, nt, endpoint=False)
        ss = np.linspace(0, 1, ns) ** 1.35 * 0.985
        V = self.point(tt[:, None], ss[None, :]).reshape(-1, 3)
        a0 = unit(ray_np(*self.center))
        pole = -a0 * hull_dist(-a0)
        faces = []
        for i in range(nt):
            i2 = (i + 1) % nt
            for j in range(ns - 1):
                faces.append((i * ns + j, i2 * ns + j, i2 * ns + j + 1, i * ns + j + 1))
            faces.append((i * ns + ns - 1, i2 * ns + ns - 1, nt * ns))
        uv = np.column_stack([np.repeat(tt / (2 * np.pi), ns), np.tile(ss, nt)])
        return mesh_obj(name, np.vstack([V, pole]), faces, M["hull"], COLL["Hull"], inward=True,
                        uv=np.vstack([uv, [0.5, 1.0]]))

    def glass(self, name="Window_Glass", nt=256, ng=10):
        tt = np.linspace(0, 2 * np.pi, nt, endpoint=False)
        ox, oy = self.outline_px(tt)
        c = unit(ray_np(*self.center))
        gv = [c * hull_dist(c)]
        for u in np.linspace(0, 1, ng + 1)[1:]:
            d = unit(ray_np(self.center[0] + u * (ox - self.center[0]),
                            self.center[1] + u * (oy - self.center[1])))
            gv.extend(d * hull_dist(d)[:, None])
        gf = [(0, 1 + i, 1 + (i + 1) % nt) for i in range(nt)]
        for r in range(ng - 1):
            for i in range(nt):
                a, b = 1 + r * nt, 1 + (r + 1) * nt
                gf.append((a + i, b + i, b + (i + 1) % nt, a + (i + 1) % nt))
        return mesh_obj(name, gv, gf, M["glass"], COLL["Hull"], inward=True)


def masked_hull(step_deg=0.35, coarse_deg=4.0, extend_az=None, extend_el=None, glass_fn=None,
                grow_px=0):
    """Generic hull for open canopies: a latitude/longitude shell around the eye whose faces become
    window glass wherever the original art is see-through (so every opaque pixel of the art lands
    on some surface). Outside the 640x480 frame the mask is extended from the nearest frame pixel
    up to |azimuth| <= extend_az and elevation within extend_el = (min, max) degrees; beyond that the
    hull is closed. glass_fn(az_deg, el_deg) -> bool can force extra glass. grow_px > 0 grows the
    opaque area (shrinks the glass) by that many pixels. Returns (hull, glass) objects."""
    see = SEE.copy()
    for _ in range(grow_px):
        s = see.copy()
        s[1:] &= see[:-1]; s[:-1] &= see[1:]; s[:, 1:] &= see[:, :-1]; s[:, :-1] &= see[:, 1:]
        see = s
    # dense sampling where the image is, coarse elsewhere
    az0, az1 = math.degrees(math.atan2(-CX, F)) - 3, math.degrees(math.atan2(640 - CX, F)) + 3
    el0 = math.degrees(math.atan2(CY - 480, F)) - 3
    el1 = math.degrees(math.atan2(CY, F)) + 3

    def axis(lo, hi, a_min, a_max):
        dense = np.arange(lo, hi, step_deg)
        left = np.arange(a_min, lo, coarse_deg)
        right = np.arange(hi, a_max, coarse_deg)
        v = np.unique(np.round(np.concatenate([left, dense, right, [a_max]]), 6))
        return v[np.concatenate([[True], np.diff(v) > step_deg * 0.25])]  # no zero-width faces

    az = axis(az0, az1, -180.0, 180.0)[:-1]          # wraps around
    el = axis(el0, el1, -89.0, 89.0)
    A, E = np.meshgrid(np.radians(az), np.radians(el), indexing="ij")
    D = np.stack([np.sin(A) * np.cos(E), np.cos(A) * np.cos(E), np.sin(E)], -1)
    V = D * hull_dist(D)[..., None]
    na, ne = len(az), len(el)
    # classify faces by their centre direction
    ac = np.radians((az + np.roll(az, -1) + np.where(np.arange(na) == na - 1, 360, 0)) / 2)
    ec = np.radians((el[:-1] + el[1:]) / 2)
    AC, EC = np.meshgrid(ac, ec, indexing="ij")
    DC = np.stack([np.sin(AC) * np.cos(EC), np.cos(AC) * np.cos(EC), np.sin(EC)], -1)
    is_glass = np.zeros(AC.shape, bool)
    front = DC[..., 1] > 0.02
    pxy = to_px(np.where(front[..., None], DC, (0, 1, 0)))
    inside = front & (pxy[..., 0] >= 0) & (pxy[..., 0] < 640) & (pxy[..., 1] >= 0) & (pxy[..., 1] < 480)
    xi = np.clip(pxy[..., 0].astype(int), 0, 639)
    yi = np.clip(pxy[..., 1].astype(int), 0, 479)
    is_glass[inside] = see[yi[inside], xi[inside]]
    if extend_az is not None:
        ad, ed = np.degrees(AC), np.degrees(EC)
        lo, hi = extend_el if extend_el is not None else (-60, 70)
        ext = ~inside & (np.abs(ad) <= extend_az) & (ed >= lo) & (ed <= hi)
        # nearest frame pixel along the same azimuth/elevation (clamped projection)
        tx = np.clip(np.where(np.abs(ad) < 89, CX + F * np.tan(np.radians(ad)), np.sign(ad) * 1e4), 0, 639)
        ty = np.clip(CY - F * np.tan(np.radians(ed)) / np.maximum(np.cos(np.radians(np.clip(ad, -89, 89))), 1e-3), 0, 479)
        is_glass[ext] = see[ty[ext].astype(int), tx[ext].astype(int)]
    if glass_fn is not None:
        is_glass |= np.vectorize(glass_fn)(np.degrees(AC), np.degrees(EC))
    idx = np.arange(na * ne).reshape(na, ne)
    quads = np.stack([idx, np.roll(idx, -1, 0), np.roll(idx, -1, 0)[:, [*range(1, ne)] + [0]],
                      idx[:, [*range(1, ne)] + [0]]], -1)[:, :-1]
    flat_V = V.reshape(-1, 3)
    uv = np.column_stack([(np.repeat(az, ne) + 180) / 360, np.tile((el + 90) / 180, na)])
    objs = []
    for name, sel, mat in (("Hull", ~is_glass, M["hull"]), ("Window_Glass", is_glass, M["glass"])):
        q = quads[sel]
        faces = [tuple(f) for f in q]
        if name == "Hull":  # close the poles
            top, bot = len(flat_V), len(flat_V) + 1
            faces += [(idx[i, ne - 1], idx[(i + 1) % na, ne - 1], top) for i in range(na)]
            faces += [(idx[(i + 1) % na, 0], idx[i, 0], bot) for i in range(na)]
            up, down = np.array([0, 0, 1.0]), np.array([0, 0, -1.0])
            verts = np.vstack([flat_V, [up * hull_dist(up), down * hull_dist(down)]])
            uvs = np.vstack([uv, [[0.5, 1.0], [0.5, 0.0]]])
        else:
            verts, uvs = flat_V, uv
        objs.append(mesh_obj(name, verts, faces, mat, COLL["Hull"], inward=True, uv=uvs,
                             compact=True))
    log(f"masked hull: {int((~is_glass).sum())} hull faces, {int(is_glass.sum())} glass faces")
    return objs


# ------------------------------------------------------------------------------------------------
# Mesh helpers
# ------------------------------------------------------------------------------------------------
def mesh_obj(name, V, faces, mat, coll, smooth=True, sharp=None, uv=None, inward=False,
             compact=False):
    V = np.asarray(V, float)
    if compact:  # drop unused vertices
        used = np.unique(np.array([i for f in faces for i in f], int))
        remap = -np.ones(len(V), int)
        remap[used] = np.arange(len(used))
        V = V[used]
        faces = [tuple(remap[i] for i in f) for f in faces]
        if uv is not None:
            uv = np.asarray(uv)[used]
    me = bpy.data.meshes.new(name)
    me.from_pydata(V.tolist(), [], faces)
    me.validate()
    if uv is not None:
        lay = me.uv_layers.new(name="UVMap")
        lv = np.empty(len(me.loops), dtype=np.int32)
        me.loops.foreach_get("vertex_index", lv)
        lay.data.foreach_set("uv", np.asarray(uv, np.float32)[lv].ravel())
    if inward:  # surfaces seen from the eye: normals must face the origin
        bm = bmesh.new()
        bm.from_mesh(me)
        for f in bm.faces:
            if f.normal.dot(f.calc_center_median()) > 0:
                f.normal_flip()
        bm.to_mesh(me)
        bm.free()
    if smooth:
        me.shade_smooth()
    if sharp is not None:
        me.set_sharp_from_angle(angle=math.radians(sharp))
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    return ob


def recalc_normals(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(ob.data)
    bm.free()


def loft_faces(n_rings, k, closed_path=False, cap_start=False, cap_end=False):
    faces = []
    for i in range(n_rings if closed_path else n_rings - 1):
        i2 = (i + 1) % n_rings
        for j in range(k):
            j2 = (j + 1) % k
            faces.append((i * k + j, i * k + j2, i2 * k + j2, i2 * k + j))
    if cap_start:
        faces.append(tuple(reversed(range(k))))
    if cap_end:
        faces.append(tuple((n_rings - 1) * k + j for j in range(k)))
    return faces


def catmull_rom(ctrl, n=24):
    P = np.asarray(ctrl, float)
    if len(P) == 2:
        return np.array([P[0] + (P[1] - P[0]) * t for t in np.linspace(0, 1, n + 1)])
    P = np.vstack([2 * P[0] - P[1], P, 2 * P[-1] - P[-2]])
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(n):
            t = k / n
            out.append(0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                              + (3 * p1 - p0 - 3 * p2 + p3) * t ** 3))
    out.append(P[-2])
    return np.array(out)


def resample(P, step, closed=False):
    """Uniform arc-length resampling; extra columns (e.g. radius) are interpolated too."""
    P = np.asarray(P, float)
    if closed:
        P = np.vstack([P, P[:1]])
    seg = np.linalg.norm(np.diff(P[:, :3], axis=0), axis=1)
    s = np.concatenate([[0], np.cumsum(seg)])
    n = max(2, int(math.ceil(s[-1] / step)) + 1)
    si = np.linspace(0, s[-1], n)
    Q = np.stack([np.interp(si, s, P[:, k]) for k in range(P.shape[1])], -1)
    if closed:
        Q, si = Q[:-1], si[:-1]
    return Q, si


def frames(P, normals=None, closed=False):
    if closed:
        T = unit(np.roll(P, -1, 0) - np.roll(P, 1, 0))
    else:
        T = unit(np.gradient(P, axis=0))
    if normals is not None:
        N = unit(normals - T * (normals * T).sum(-1, keepdims=True))
    else:
        N = np.zeros_like(P)
        ref = np.array([0.0, 0.0, 1.0]) if abs(T[0][2]) < 0.9 else np.array([1.0, 0.0, 0.0])
        N[0] = unit(np.cross(np.cross(T[0], ref), T[0]))
        for i in range(1, len(P)):
            v = N[i - 1] - T[i] * np.dot(N[i - 1], T[i])
            N[i] = unit(v) if np.linalg.norm(v) > 1e-9 else N[i - 1]
    return T, N, np.cross(T, N)


def tube(name, P, R, mat, coll, sides=12, caps=True, closed=False, normals=None):
    P = np.asarray(P, float)
    R = np.broadcast_to(np.asarray(R, float), (len(P),))
    T, N, B = frames(P, normals, closed)
    a = 2 * np.pi * np.arange(sides) / sides
    V = P[:, None] + R[:, None, None] * (np.cos(a)[None, :, None] * N[:, None]
                                         + np.sin(a)[None, :, None] * B[:, None])
    faces = loft_faces(len(P), sides, closed, caps and not closed, caps and not closed)
    return mesh_obj(name, V.reshape(-1, 3), faces, mat, coll)


def corrugation(s, pitch, amp):
    return 1.0 + amp * (0.5 - 0.5 * np.cos(2 * np.pi * s / pitch)) ** 0.6


def anchor(pts, start=False, end=True):
    """Continue loose ends straight on until they disappear into the hull or the floor."""
    pts = list(pts)
    for at_end in [e for e, on in ((False, start), (True, end)) if on]:
        p, q = (pts[-1], pts[-2]) if at_end else (pts[0], pts[1])
        d = unit(p - q)
        if p[2] < -0.25:  # low cables drop to the floor instead of crossing the footwell
            d = unit(d * 0.4 + np.array([0.0, 0.0, -1.0]))
        t = hull_dist(d, p)
        if d[2] < -1e-3:
            t = min(t, (FLOOR_Z - p[2]) / d[2])
        tip = p + d * (t + 0.04)
        mid = p + d * (t + 0.04) * 0.5
        if at_end:
            pts += [mid, tip]
        else:
            pts = [tip, mid] + pts
    return np.array(pts)


def path3d(ctrl):
    """Control points (px, py, dist) -> 3D points."""
    return np.array([at(px, py, d) for px, py, d in ctrl])


def cable(name, ctrl, mat, coll=None, r=None, r_px=None, pitch=None, amp=0.18, sides=12,
          start=False, end=False, recentre=True):
    """Cable/hose through control points (px, py, dist). Radius in metres (r), or the apparent
    radius in pixels across the traced centre line (r_px: scalar, per control point or as a 4th
    value of each control point; None for a hidden point, interpolated). r_px is the silhouette,
    the crest of the corrugation; the radius follows the trace along the cable (bulges, tapering
    hoses), see fit_across(). pitch/amp: corrugation. start/end: anchor that end in the hull or
    floor."""
    if r is None:
        if r_px is None:
            r_px = [c[3] for c in ctrl]
        rp = list(r_px) if np.ndim(r_px) else [r_px] * len(ctrl)
        ctrl, w = fit_across(ctrl, [None if v is None else 2 * v for v in rp], recentre=recentre)
        R = w / (2 * (1 + amp) if pitch else 2)
    else:
        R = np.full(len(ctrl), float(r))
    pts = anchor(path3d([c[:3] for c in ctrl]), start, end)
    k0 = 2 if start else 0
    R = np.concatenate([np.full(k0, R[0]), R, np.full(len(pts) - k0 - len(R), R[-1])])
    D, s = resample(catmull_rom(np.column_stack([pts, R])), (pitch / 6) if pitch else 0.01)
    R = D[:, 3] * corrugation(s, pitch, amp) if pitch else D[:, 3]
    return tube(name, D[:, :3], R, mat, coll or COLL["Cables"], sides=sides)


def cable_bundle(name, ctrl, n, r, mat, coll=None, spacing=None, start=True, end=True):
    """n parallel cables side by side (a flat ribbon as seen from the eye)."""
    Pb, _ = resample(catmull_rom(anchor(path3d(ctrl), start, end)), 0.01)
    Tb = unit(np.gradient(Pb, axis=0))
    side = unit(np.cross(Tb, unit(Pb)))
    spacing = spacing or 2.1 * r
    return [tube(f"{name}_{k}", Pb + side * (k - (n - 1) / 2) * spacing
                 + unit(Pb) * 0.4 * r * ((-1) ** k), r, mat, coll or COLL["Cables"], sides=8)
            for k in range(n)]


def bulb(name, axis_x, y0, y1, dist, edge_px, side, mat, pitch=0.03, amp=0.1, sides=24,
         coll=None):
    """Round (bellows) body on the vertical image line x = axis_x (y0..y1) at a constant distance.
    Its radius is solved per cross-section so that the silhouette's edge on one side (side=+1:
    right) follows the traced edge [(x, y), ...] (sorted by y): seen from below and beside the eye
    the cross-sections look tilted, so the radius cannot be read off per image row."""
    E = np.array(edge_px, float)
    P = path3d([(axis_x, y, dist) for y in np.arange(y0, y1 + 0.1, 1.5)])
    Wd = unit(np.cross(unit(np.gradient(P, axis=0)), unit(-P)))
    R = []
    for p, w in zip(P, Wd):
        w = w if (to_px(p + w * 1e-3) - to_px(p))[0] * side > 0 else -w

        def f(r):
            e = to_px(p + w * r)
            return (e[0] - np.interp(e[1], E[:, 1], E[:, 0], left=axis_x, right=axis_x)) * side

        lo, hi = 0.0, 0.6
        if f(lo) >= 0:
            R.append(0.0)
            continue
        for _ in range(40):
            mid = (lo + hi) / 2
            lo, hi = (mid, hi) if f(mid) < 0 else (lo, mid)
        R.append(lo)
    s = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))])
    R = np.maximum(np.array(R), 0.004) / (1 + amp) * corrugation(s, pitch, amp)
    return tube(name, P, R, mat, coll or COLL["Instruments"], sides=sides)


def beam(name, ctrl, mat, coll=None, w=None, w_px=None, h=0.06, h_ratio=None, round_frac=0.3,
         step=0.01, start=False, end=False, closed=False, twist_to_eye=True, recentre=True):
    """Structural member (strut, arch, frame) swept along control points (px, py, dist).
    Cross-section: rounded rectangle of width w (metres, scalar or per control point) and depth h
    along the view ray (metres, or h_ratio times the width), so the member's silhouette from the eye
    is its width. Instead of w: w_px, the apparent width in pixels across the traced centre line
    (scalar, per control point or as a 4th value of each control point; see fit_across(), also for
    recentre). start/end anchor the ends into hull/floor."""
    if w is None:
        traced = [c[:3] for c in ctrl]
        wp = [c[3] for c in ctrl] if w_px is None else w_px
        near = 0.5 - round_frac / 2      # the widest part of the profile, towards the eye
        if h_ratio is not None:  # the depth depends on the width: solve twice
            h = h_ratio * fit_across(traced, wp, recentre=False)[1]
        ctrl, w = fit_across(traced, wp, near * np.asarray(h, float), recentre)
    if h_ratio is not None:
        h = h_ratio * np.asarray(w, float)
    P0 = path3d([c[:3] for c in ctrl])
    n = len(ctrl)
    w = np.broadcast_to(np.asarray(w, float), (n,))
    h = np.broadcast_to(np.asarray(h, float), (n,))
    data = np.column_stack([P0, w, h])
    if start or end:
        pts = anchor(P0, start, end)
        k0 = len(pts) - n - (2 if end else 0)
        ext = np.zeros((len(pts), 5))
        ext[:, :3] = pts
        ext[k0:k0 + n, 3:] = data[:, 3:]
        ext[:k0, 3:] = data[0, 3:]
        ext[k0 + n:, 3:] = data[-1, 3:]
        data = ext
    D, _ = resample(catmull_rom(np.vstack([data, data[:1]]) if closed else data), step, closed)
    P, W, H = D[:, :3], D[:, 3], D[:, 4]
    T = unit(np.roll(P, -1, 0) - np.roll(P, 1, 0)) if closed else unit(np.gradient(P, axis=0))
    V = unit(-P) if twist_to_eye else np.tile([0, 0, 1.0], (len(P), 1))
    Wd = unit(np.cross(T, V))
    Hd = np.cross(Wd, T)
    rf = round_frac
    prof = [(-0.5, -0.5 + rf / 2), (-0.5 + rf / 2, -0.5), (0.5 - rf / 2, -0.5), (0.5, -0.5 + rf / 2),
            (0.5, 0.5 - rf / 2), (0.5 - rf / 2, 0.5), (-0.5 + rf / 2, 0.5), (-0.5, 0.5 - rf / 2)]
    ring = [P + Wd * (u * W)[:, None] + Hd * (v * H)[:, None] for u, v in prof]
    Vt = np.stack(ring, 1).reshape(-1, 3)
    ob = mesh_obj(name, Vt, loft_faces(len(P), len(prof), closed, not closed, not closed), mat,
                  coll or COLL["Instruments"], smooth=True, sharp=50)
    recalc_normals(ob)
    return ob


def box_between(name, a, b, w, h, mat, coll=None, up=(0, 0, 1), bevel=0.004):
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = unit(b - a)
    x = unit(np.cross(d, up))
    y = np.cross(x, d)
    V = []
    for p in (a, b):
        for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            V.append(p + x * sx * w / 2 + y * sy * h / 2)
    faces = [(0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]
    ob = mesh_obj(name, V, faces, mat, coll or COLL["Instruments"], smooth=False)
    recalc_normals(ob)
    if bevel:
        mod = ob.modifiers.new("Bevel", 'BEVEL')
        mod.width, mod.segments = bevel, 2
    return ob


def cylinder_between(name, a, b, r, mat, coll=None, sides=16):
    return tube(name, np.array([a, b], float), r, mat, coll or COLL["Instruments"], sides=sides)


def box(name, center, size, mat, coll=None, rot=(0, 0, 0), bevel=0.01):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=size, verts=bm.verts)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    ob.location, ob.rotation_euler = center, rot
    (coll or COLL["Instruments"]).objects.link(ob)
    if bevel:
        mod = ob.modifiers.new("Bevel", 'BEVEL')
        mod.width, mod.segments = bevel, 3
    return ob


def ellipsoid(name, center, radii, mat, coll=None, rot=None, segs=24):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=segs // 2, radius=1.0)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.shade_smooth()
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    m = Matrix.Diagonal((*radii, 1.0))
    if rot is not None:
        m = rot.to_4x4() @ m
    ob.matrix_world = Matrix.Translation(Vector(center)) @ m
    (coll or COLL["Instruments"]).objects.link(ob)
    return ob


# ------------------------------------------------------------------------------------------------
# Planar elements: placed at a pixel and distance; 2D outlines traced in pixel space are ray-cast
# onto the panel plane, so the silhouette matches the original.
# ------------------------------------------------------------------------------------------------
class Panel:
    """facing = 0: parallel to the original image plane (shapes keep their 2D look exactly);
    facing = 1: turned to face the eye (ergonomic, but stretched at the edge of a 105 deg view).
    pitch: extra tilt about the panel's x axis in degrees (positive: top leans away)."""

    def __init__(self, cx, cy, dist, facing=0.8, pitch=0.0):
        o = Vector(at(cx, cy, dist).tolist())
        n = -(Vector((0, 1, 0)).lerp(o.normalized(), facing)).normalized()
        x = (Vector((1, 0, 0)) - n * n.x).normalized()
        y = n.cross(x)
        if pitch:
            r = Matrix.Rotation(math.radians(pitch), 3, x)
            n, y = r @ n, r @ y
        self.o, self.x, self.y, self.n = o, x, y, n
        self.m = Matrix((
            (x.x, y.x, n.x, o.x), (x.y, y.y, n.y, o.y), (x.z, y.z, n.z, o.z), (0, 0, 0, 1)))

    def proj(self, px, py):
        d = Vector(ray_np(px, py).tolist())
        p = d * (self.o.dot(self.n) / d.dot(self.n)) - self.o
        return (p.dot(self.x), p.dot(self.y))

    def poly(self, pts):
        return [self.proj(px, py) for px, py in pts]

    def radius(self, cx, cy, r_px):
        c = Vector(self.proj(cx, cy))
        return float(np.mean([(Vector(self.proj(cx + r_px * math.cos(a), cy + r_px * math.sin(a)))
                               - c).length for a in np.linspace(0, 2 * math.pi, 16)]))

    def world(self, lx, ly, lz=0.0):
        return np.array((self.m @ Vector((lx, ly, lz))).to_tuple())

    def world_px(self, px, py, lz=0.0):
        return self.world(*self.proj(px, py), lz)


class WorldPlane:
    """Identity 'panel' for prisms in world space (outline in local x/y, extruded along z)."""

    def __init__(self, z=0.0):
        self.m = Matrix.Translation((0, 0, z))


def rrect_px(x0, y0, x1, y1, r, n=5):
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90),
                       (x0 + r, y0 + r, 180)):
        for k in range(n + 1):
            a = math.radians(a0 + 90 * k / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def circle(cx, cy, r, n=48):
    return [(cx + r * math.cos(a), cy + r * math.sin(a))
            for a in np.linspace(0, 2 * math.pi, n, endpoint=False)]


def gear(cx, cy, r_root, r_tip, teeth, frac=0.45, n=6, phase=0.0):
    pts = []
    step = 2 * math.pi / teeth
    for i in range(teeth):
        a0 = phase + i * step
        w = step * frac
        for k in range(n):
            a = a0 + w / 2 + (step - w) * k / n
            pts.append((cx + r_root * math.cos(a), cy + r_root * math.sin(a)))
        for k in range(n + 1):
            a = a0 + step - w / 2 + w * k / n
            pts.append((cx + r_tip * math.cos(a), cy + r_tip * math.sin(a)))
    return pts


def sector(cx, cy, r0, r1, a0, a1, n=24):
    a = np.radians(np.linspace(a0, a1, n))
    return ([(cx + r1 * math.cos(t), cy + r1 * math.sin(t)) for t in a]
            + [(cx + r0 * math.cos(t), cy + r0 * math.sin(t)) for t in a[::-1]])


def to_mesh_object(ob):
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    new = bpy.data.objects.new(ob.name, me)
    new.matrix_world = ob.matrix_world.copy()
    for c in ob.users_collection:
        c.objects.link(new)
    data, name = ob.data, ob.name
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.curves.remove(data)
    new.name = new.data.name = name
    return new


def prism(name, panel, outlines, z0, z1, mat, coll=None, bevel=0.004, local=False, sharp=40):
    """Extrude 2D outlines (first = outer, rest = holes; pixel coords unless local=True) between
    local z0..z1 of the panel (z points towards the pilot)."""
    cu = bpy.data.curves.new(name, 'CURVE')
    cu.dimensions = '2D'
    cu.fill_mode = 'BOTH'
    bevel = min(bevel, (z1 - z0) / 2 * 0.9)
    cu.extrude = (z1 - z0) / 2 - bevel
    cu.bevel_depth = bevel
    cu.bevel_resolution = 2
    for loop in outlines:
        pts = loop if local else panel.poly(loop)
        sp = cu.splines.new('POLY')
        sp.points.add(len(pts) - 1)
        for i, (x, y) in enumerate(pts):
            sp.points[i].co = (x, y, 0.0, 1.0)
        sp.use_cyclic_u = True
    ob = bpy.data.objects.new(name, cu)
    (coll or COLL["Instruments"]).objects.link(ob)
    ob.matrix_world = panel.m @ Matrix.Translation((0, 0, (z0 + z1) / 2))
    ob = to_mesh_object(ob)
    ob.data.materials.append(mat)
    ob.data.shade_smooth()
    ob.data.set_sharp_from_angle(angle=math.radians(sharp))
    return ob


def prism_eye(name, panel, outlines, z0, z1, mat, coll=None, bevel=0.003, sharp=40):
    """prism() whose silhouette from the eye is exactly the pixel outlines (first = outer, rest =
    holes): they are cast onto the front face z1 (which may stand in front of the panel), inset by
    the bevel, and the depth runs away from the eye along the view rays instead of the panel
    normal, so neither the bevel nor the side faces show beyond the traced outline."""
    o, n = panel.o, panel.n
    c = o + n * z1

    def cast(px, py):
        d = Vector(ray_np(px, py).tolist())
        p = d * (c.dot(n) / d.dot(n)) - c
        return (p.dot(panel.x), p.dot(panel.y))

    loops = [inset([cast(*q) for q in loop], bevel if k == 0 else -bevel)
             for k, loop in enumerate(outlines)]
    ob = prism(name, panel, loops, z0, z1, mat, coll, bevel=bevel, local=True, sharp=sharp)
    mw, inv = ob.matrix_world, ob.matrix_world.inverted()
    for v in ob.data.vertices:
        w = mw @ v.co
        lx, ly, lz = (w - o).dot(panel.x), (w - o).dot(panel.y), (w - o).dot(n)
        u = (o + panel.x * lx + panel.y * ly + n * z1).normalized()
        v.co = inv @ (u * ((lz + o.dot(n)) / u.dot(n)))
    ob.data.update()
    return ob


def screen(name, panel, outline_local, z, mat, coll=None, des_section=None, des_rect=None):
    """Flat screen n-gon with 0..1 UVs, origin at its centre, +Z towards the pilot."""
    P = np.array(outline_local)
    if np.dot(P[:, 0], np.roll(P[:, 1], -1)) - np.dot(P[:, 1], np.roll(P[:, 0], -1)) < 0:
        P = P[::-1]  # counter-clockwise, so the face normal (+Z) points at the pilot
    c = (P.min(0) + P.max(0)) / 2
    V = np.column_stack([P - c, np.zeros(len(P))])
    uv = (P - P.min(0)) / (P.max(0) - P.min(0))
    ob = mesh_obj(name, V, [tuple(range(len(P)))], mat, coll or COLL["Instruments"], smooth=False,
                  uv=uv)
    ob.matrix_world = panel.m @ Matrix.Translation((c[0], c[1], z))
    if des_section:
        ob["des_section"] = des_section
        ob["des_rect"] = list(des_rect)
    ob["role"] = "screen"
    return ob


def des_rect(section):
    s = DES[section.lower()]
    if "radius" in s:
        return (int(s["x1"]), int(s["y1"]), int(s["radius"]))
    return (int(s["x1"]), int(s["y1"]), int(s["width"]), int(s["height"]))


def rect_monitor(section, dist, bezel_px, screen_px=None, facing=0.8, pitch=0.0, center_px=None,
                 bezel_z=(-0.04, 0.0), screen_z=-0.025, body_px=None, body_z=(-0.25, -0.035),
                 mat_bezel=None, mat_body=None, prefix=None):
    """Rectangular monitor for a DES section (MasterMonitor, SlaveMonitor0, ...): bezel with the
    screen hole, a body behind it and the screen. bezel_px: outer outline in pixels (list of points
    or (x0, y0, x1, y1, r) rounded rectangle). screen_px: screen outline (default: DES rect grown by
    3 px, corner radius 6). Returns the Panel for adding details."""
    x, y, w, h = des_rect(section)
    cx, cy = center_px or (x + w / 2, y + h / 2)
    p = Panel(cx, cy, dist, facing, pitch)
    if screen_px is None:
        screen_px = rrect_px(x - 3, y - 3, x + w + 3, y + h + 3, 6)
    if len(bezel_px) == 5 and not isinstance(bezel_px[0], (tuple, list)):
        bezel_px = rrect_px(*bezel_px)
    prefix = prefix or section
    prism(f"{prefix}_Bezel", p, [bezel_px, screen_px], *bezel_z, mat_bezel or M["bronze"],
          bevel=0.005)
    body = body_px or bezel_px
    if len(body) == 5 and not isinstance(body[0], (tuple, list)):
        body = rrect_px(*body)
    prism(f"{prefix}_Body", p, [body], *body_z, mat_body or M["dark"])
    screen(f"Screen_{section}", p, p.poly(screen_px), screen_z, M["screen"],
           des_section=section, des_rect=des_rect(section))
    return p


def round_monitor(section, dist, screen_r_px, housing_px=None, housing_r_px=None, center_px=None,
                  housing_center_px=None, teeth=0, tooth_px=6, facing=0.4, depth=0.05,
                  back_depth=0.15, mat_housing=None, stalk_to_floor=False, prefix=None):
    """Round monitor for a DES section (RoundMonitor0/1): ring housing (plain or with gear teeth),
    lens ring, round screen and a can behind it. Returns the Panel."""
    x1, y1, _ = des_rect(section)
    sx, sy = center_px or (x1, y1)
    hx, hy = housing_center_px or (sx, sy)
    p = Panel(sx, sy, dist, facing=facing)
    r_scr = p.radius(sx, sy, screen_r_px)
    hc = p.proj(hx, hy)
    prefix = prefix or section
    if housing_px is not None:
        housing = p.poly(housing_px)
    else:
        r_out = p.radius(hx, hy, housing_r_px)
        if teeth:
            r_in = p.radius(hx, hy, housing_r_px - tooth_px)
            housing = [(hc[0] + a, hc[1] + b) for a, b in gear(0, 0, r_in, r_out, teeth, 0.42, 5)]
        else:
            housing = [(hc[0] + math.cos(a) * r_out, hc[1] + math.sin(a) * r_out)
                       for a in np.linspace(0, 2 * math.pi, 72, endpoint=False)]
    hole = [(math.cos(a) * r_scr * 1.02, math.sin(a) * r_scr * 1.02)
            for a in np.linspace(0, 2 * math.pi, 64, endpoint=False)]
    prism(f"{prefix}_Housing", p, [housing, hole], -depth, 0.0, mat_housing or M["bronze"],
          local=True, bevel=0.005)
    can = [(math.cos(a) * r_scr * 1.3, math.sin(a) * r_scr * 1.3)
           for a in np.linspace(0, 2 * math.pi, 48, endpoint=False)]
    prism(f"{prefix}_Back", p, [can], -back_depth, -depth + 0.005, M["dark"], local=True)
    ring = np.array([p.world(math.cos(a) * r_scr, math.sin(a) * r_scr, -0.004)
                     for a in np.linspace(0, 2 * math.pi, 96, endpoint=False)])
    tube(f"{prefix}_Lens", ring, 0.006, M["bronze_dk"], COLL["Instruments"], sides=8, closed=True,
         normals=np.tile(np.array(p.n.to_tuple()), (96, 1)))
    disc = [(math.cos(a) * r_scr, math.sin(a) * r_scr)
            for a in np.linspace(0, 2 * math.pi, 64, endpoint=False)]
    screen(f"Screen_{section}", p, disc, -0.012, M["screen"], des_section=section,
           des_rect=des_rect(section))
    if stalk_to_floor:
        back = p.world(0, 0, -back_depth)
        cylinder_between(f"{prefix}_Stalk", back, (back[0], back[1] + 0.05, FLOOR_Z), 0.025,
                         M["dark"])
    return p


def mount_up(name, p, r=0.03, mat=None):
    """Vertical rod from point p up into the hull."""
    p = np.asarray(p, float)
    return cylinder_between(name, p, p + np.array([0, 0, hull_dist(np.array([0, 0, 1.0]), p) + 0.02]),
                            r, mat or M["dark"])


def lamp(name, panel, px, py, r=0.009, z=0.008):
    return ellipsoid(name, panel.world_px(px, py, z), (r,) * 3, M["lamp"])


def lamp_disc(name, panel, px, py, r_px=6, z=0.002):
    """Flat lamp lens on a panel, for DES lamps the art shows switched off: the grazing rim of a
    lamp() sphere gets the lit fallback in the texture bake and shows up as a bright ring."""
    ring = [panel.world_px(x, y, z) for x, y in circle(px, py, r_px, 24)]
    return mesh_obj(name, ring, [tuple(range(24))], M["lamp"], COLL["Instruments"], smooth=False,
                    inward=True)


# ------------------------------------------------------------------------------------------------
# Interior: floor, seat, pilot dummy (reference only, not exported)
# ------------------------------------------------------------------------------------------------
def standard_interior():
    k = math.sqrt(max(1 - ((FLOOR_Z - HC[2]) / HA[2]) ** 2, 0.05)) * 0.985
    floor_pts = [(HA[0] * k * math.cos(a), HC[1] + HA[1] * k * math.sin(a))
                 for a in np.linspace(0, 2 * math.pi, 64, endpoint=False)]
    prism("Floor", WorldPlane(FLOOR_Z - 0.015), [floor_pts], -0.015, 0.015, M["floor"],
          COLL["Hull"], local=True)
    S = COLL["Seat"]
    box("Seat_Pan", (0, 0.0, -0.80), (0.50, 0.50, 0.08), M["seat"], S)
    box("Seat_Back", (0, -0.29, -0.45), (0.50, 0.09, 0.66), M["seat"], S,
        rot=(math.radians(-10), 0, 0))
    box("Seat_Headrest", (0, -0.20, 0.0), (0.28, 0.08, 0.22), M["seat"], S, bevel=0.025)
    box("Seat_Base", (0, -0.05, (FLOOR_Z - 0.84) / 2), (0.30, 0.30, abs(FLOOR_Z + 0.84)), M["dark"], S)
    R = COLL["Reference"]
    ellipsoid("Dummy_Head", (0, -0.085, 0.02), (0.085, 0.105, 0.115), M["dummy"], R)
    tube("Dummy_Torso", np.array([(0, -0.17, -0.22), (0, -0.16, -0.70)]), 0.16, M["dummy"], R)
    for sx in (-1, 1):
        tube(f"Dummy_Leg_{sx}", np.array([(0.1 * sx, -0.05, -0.72), (0.12 * sx, 0.36, -0.66),
                                          (0.12 * sx, 0.52, -0.95)]), 0.065, M["dummy"], R)
        tube(f"Dummy_Arm_{sx}", np.array([(0.21 * sx, -0.14, -0.24), (0.26 * sx, 0.06, -0.48),
                                          (0.19 * sx, 0.33, -0.45)]), 0.045, M["dummy"], R)


# ------------------------------------------------------------------------------------------------
# Finish: markers, camera, lights, save, export, alignment check, renders
# ------------------------------------------------------------------------------------------------
def camera(name, loc, rot_deg, angle=105.0, ortho=None):
    cd = bpy.data.cameras.new(name)
    cd.sensor_fit = 'HORIZONTAL'
    if ortho:
        cd.type, cd.ortho_scale = 'ORTHO', ortho
    else:
        cd.angle = math.radians(angle)
    cd.clip_start = 0.01
    ob = bpy.data.objects.new(name, cd)
    ob.location = loc
    ob.rotation_euler = [math.radians(a) for a in rot_deg]
    COLL["RenderRig"].objects.link(ob)
    return ob


def eye_camera():
    """Camera at the eye matching the original cockpit camera, incl. its off-centre optical axis."""
    cam = camera("EyeCamera", (0, 0, 0), (90, 0, 0), angle=2 * math.degrees(math.atan(320 / F)))
    cam.data.shift_x = (320 - CX) / 640      # optical axis lands on pixel (CX, CY)
    cam.data.shift_y = (CY - 240) / 640
    return cam


def light(name, kind, loc, power, color, size=None, rot=(0, 0, 0)):
    ld = bpy.data.lights.new(name, kind)
    ld.energy = power
    ld.color = color
    if size and kind == 'AREA':
        ld.shape, ld.size, ld.size_y = 'RECTANGLE', size[0], size[1]
    elif size:
        ld.shadow_soft_size = size
    ob = bpy.data.objects.new(name, ld)
    ob.location = loc
    ob.rotation_euler = [math.radians(a) for a in rot]
    COLL["RenderRig"].objects.link(ob)
    return ob


def default_lights():
    light("WindowLight", 'AREA', (0, 1.25, 0.05), 45, (0.35, 0.75, 0.85), (2.0, 1.0), (-90, 0, 0))
    light("ConsoleKey", 'POINT', (0.05, 0.25, 0.05), 6, (1.0, 0.75, 0.5), 0.15)
    light("CeilingGreen", 'POINT', (0.0, 0.45, 0.62), 12, (0.3, 1.0, 0.6), 0.2)
    light("SideFillL", 'POINT', (-0.75, 0.35, 0.0), 10, (0.25, 1.0, 0.7), 0.2)
    light("SideFillR", 'POINT', (0.75, 0.35, 0.0), 10, (0.25, 1.0, 0.7), 0.2)
    light("RearFill", 'POINT', (0.0, -0.5, 0.35), 6, (0.4, 0.8, 0.8), 0.3)
    sc = bpy.context.scene
    world = bpy.data.worlds.get("World") or bpy.data.worlds.new("World")
    sc.world = world
    try:
        world.use_nodes = True
    except Exception:
        pass
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.0, 0.012, 0.045, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.0
    world.color = (0.0, 0.012, 0.045)


def markers(hud_dist=0.98):
    eye = bpy.data.objects.new("Eye", None)
    eye.empty_display_type = 'ARROWS'
    eye.empty_display_size = 0.08
    eye["role"] = "pilot_eye"
    COLL["Markers"].objects.link(eye)
    x, y, w, h = des_rect("MasterHUD")
    ph = Panel(x + w / 2, y + h / 2, hud_dist)
    hud = screen("HUD_MasterHUD", ph, ph.poly([(x, y), (x + w, y), (x + w, y + h), (x, y + h)]), 0.0,
                 M["hud"], COLL["Markers"], "MasterHUD", (x, y, w, h))
    hud["role"] = "hud"
    return hud


def save_rgba(arr, path):
    h, w = arr.shape[:2]
    if arr.shape[2] == 3:
        arr = np.concatenate([arr, np.ones((h, w, 1), np.float32)], -1)
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=True)
    img.pixels.foreach_set(np.ascontiguousarray(arr[::-1], np.float32).ravel())
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()
    bpy.data.images.remove(img)


def load_rgba(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    a = np.array(img.pixels[:], np.float32).reshape(h, w, 4)[::-1]
    bpy.data.images.remove(img)
    return a


def use_metal():
    try:
        prefs = bpy.context.preferences.addons['cycles'].preferences
        prefs.compute_device_type = 'METAL'
        prefs.get_devices()
        for d in prefs.devices:
            d.use = d.type == 'METAL'
        bpy.context.scene.cycles.device = 'GPU'
    except Exception as e:
        log("no Metal:", e)


def render(path, cam, res, engine='CYCLES', samples=64, transparent=False, hide=()):
    sc = bpy.context.scene
    sc.camera = cam
    sc.render.engine = engine
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = transparent
    sc.render.filepath = path
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGBA'
    hidden = [ob for ob in hide if not ob.hide_render]
    for ob in hidden:
        ob.hide_render = True
    if engine == 'CYCLES':
        sc.cycles.samples = samples
        sc.cycles.use_denoising = True
        sc.cycles.device = 'GPU'
    bpy.ops.render.render(write_still=True)
    for ob in hidden:
        ob.hide_render = False


def alignment_check(cam, hide):
    """Flat render from the eye vs. the original: IoU of the see-through area and diff images."""
    sc = bpy.context.scene
    rend = os.path.join(OUT, "renders")
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'MATERIAL'
    sc.display.shading.show_shadows = False
    sc.display.shading.show_cavity = False
    path = os.path.join(rend, "eye_flat.png")
    render(path, cam, (640, 480), engine='BLENDER_WORKBENCH', transparent=True, hide=hide)
    ours = load_rgba(path)
    ours_win = ours[..., 3] < 0.5
    iou = (SEE & ours_win).sum() / max((SEE | ours_win).sum(), 1)
    log(f"window IoU vs original: {iou:.3f}")
    mag = np.array([0.35, 0.0, 0.35], np.float32)
    o_rgb = np.where(SEE[..., None], mag, np.clip(ART * 1.6, 0, 1))
    r_rgb = np.where(ours_win[..., None], mag, ours[..., :3])
    diff = np.zeros_like(o_rgb)  # red = only the original is opaque, cyan = only ours
    diff[~SEE & ours_win] = (1, 0.15, 0.15)
    diff[SEE & ~ours_win] = (0.1, 0.9, 1)
    both = ~SEE & ~ours_win
    diff[both] = np.clip(ART * 1.2, 0, 1)[both] * 0.6
    blend = 0.5 * o_rgb + 0.5 * r_rgb
    sheet = np.concatenate([np.concatenate([o_rgb, r_rgb], 1), np.concatenate([blend, diff], 1)], 0)
    save_rgba(sheet, os.path.join(rend, "alignment_check.png"))
    save_rgba(np.repeat(np.repeat(diff, 2, 0), 2, 1), os.path.join(rend, "alignment_diff_2x.png"))
    return iou


def finish():
    """Markers, camera, lights; save .blend + .glb; alignment check; renders.
    CLI: --no-render (save/export only), --quick (save/export + alignment check only)."""
    sc = bpy.context.scene
    hud = markers()
    cam_eye = eye_camera()
    sc.camera = cam_eye
    default_lights()
    glass = bpy.data.objects.get("Window_Glass")

    for ob in sc.objects:
        ob.select_set(False)
    export = [ob for n in ("Hull", "Instruments", "Cables", "Seat", "Markers")
              for ob in COLL[n].objects]
    for ob in export:
        ob.select_set(True)
    dg = bpy.context.evaluated_depsgraph_get()
    tris = sum(sum(len(p.vertices) - 2 for p in ob.evaluated_get(dg).data.polygons)
               for ob in export if ob.type == 'MESH')
    log(f"objects={len(export)} triangles~{tris}")
    blend = os.path.join(OUT, f"cockpit_{CID}.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend, compress=True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, f"cockpit_{CID}.glb"), export_format='GLB',
                              use_selection=True, export_apply=True, export_extras=True,
                              export_cameras=False, export_lights=False, export_yup=True)
    if "--no-render" in ARGV:
        return
    use_metal()
    hide = [o for o in (glass, hud) if o]
    alignment_check(cam_eye, hide)
    if "--quick" in ARGV:
        return
    rend = os.path.join(OUT, "renders")
    sc.view_settings.view_transform = 'AgX'
    render(os.path.join(rend, "eye_front.png"), cam_eye, (1280, 960), hide=[hud])
    render(os.path.join(rend, "eye_look_left_down.png"),
           camera("CamLookLeft", (0.04, 0.0, 0.0), (68, 0, 38), angle=100), (1280, 960), hide=[hud])
    render(os.path.join(rend, "eye_look_right_up.png"),
           camera("CamLookRight", (-0.04, 0.0, 0.0), (104, 0, -40), angle=100), (1280, 960),
           hide=[hud])
    pano = camera("CamPanorama", (0, 0, 0), (90, 0, 0))
    pano.data.type = 'PANO'
    pano.data.panorama_type = 'EQUIRECTANGULAR'
    render(os.path.join(rend, "eye_360_equirect.png"), pano, (2048, 1024), samples=48,
           hide=[hud] + list(COLL["Seat"].objects))
    # section view: everything cut at x = 0, seen from the right, pilot dummy for scale
    COLL["Reference"].hide_render = False
    dg = bpy.context.evaluated_depsgraph_get()
    for ob in [o for o in export if o.type == 'MESH' and o is not hud]:
        me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
        me.transform(ob.matrix_world)
        bm = bmesh.new()
        bm.from_mesh(me)
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.x > 0.0], context='VERTS')
        bm.to_mesh(me)
        bm.free()
        if len(me.vertices):
            COLL["RenderRig"].objects.link(bpy.data.objects.new(ob.name + "_cut", me))
        ob.hide_render = True
    hud.hide_render = True
    for d in COLL["Reference"].objects:
        if d.name.endswith(("Leg_1", "Arm_1")):
            d.hide_render = True
    sec = camera("CamSection", (3.0, 0.05, -0.22), (90, 0, 90), ortho=2.9)
    render(os.path.join(rend, "section_side.png"), sec, (1280, 960), samples=48)
    log("done")
