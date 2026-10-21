"""
Reading a cockpit's game data: the DES file (camera), COCKPIT.R16 (the 640x480 art) and where
Vertigo keeps the data. Plain Python with numpy (the settings without), no Blender: shared by
cockpitlib (in Blender), compose_texture.py and tools/install.py.
"""

import math
import os
import plistlib
import sys

try:
    import numpy as np
except ImportError:     # tools/install.py only needs the settings
    np = None


def settings_file():
    """Vertigo's settings.ini (util/settings.cpp: next to the saves, appDataLocation())."""
    if sys.platform == "darwin":
        base = os.path.expanduser("~/Library/Application Support")
    elif sys.platform == "win32":
        base = os.environ.get("LOCALAPPDATA", "")
    else:
        base = os.environ.get("XDG_DATA_HOME", "")
        if not base.startswith("/"):
            base = os.path.expanduser("~/.local/share")
    return os.path.join(base, "Vertigo", "Vertigo", "settings.ini")


def settings():
    """Vertigo's settings as {key: value}, keys like "datadir" or "cockpit/directory". Falls back to
    the files of the Qt versions (a plist on macOS, Vertigo.conf elsewhere)."""
    path = settings_file()
    if os.path.exists(path):
        values = {}
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if "=" in line and not line.startswith("#"):
                key, value = line.split("=", 1)
                values[key.strip()] = value.strip()
        return values
    plist = os.path.expanduser("~/Library/Preferences/com.vertigo.Vertigo.plist")
    if os.path.exists(plist):
        with open(plist, "rb") as f:
            return {k.replace(".", "/"): v for k, v in plistlib.load(f).items()}
    values, section = {}, ""
    conf = os.path.expanduser("~/.config/vertigo/Vertigo.conf")
    if os.path.exists(conf):
        for line in open(conf, encoding="utf-8"):
            line = line.strip()
            if line.startswith("["):
                section = line.strip("[]")
                section = "" if section == "General" else section + "/"
            elif "=" in line:
                key, value = line.split("=", 1)
                values[section + key.strip()] = value.strip()
    return values


def data_dir(given=None):
    """The game data directory: given, $VERTIGO_DATA or Vertigo's setting "datadir"."""
    d = given or os.environ.get("VERTIGO_DATA") or settings().get("datadir")
    if not d:
        raise SystemExit("Vertigo data directory unknown: pass --data DIR or set VERTIGO_DATA")
    return d


def find_ci(base, *parts):
    """Resolve a path case-insensitively (the game data mixes upper and lower case)."""
    p = base
    for part in parts:
        hit = next((e for e in os.listdir(p) if e.lower() == part.lower()), None)
        if hit is None:
            raise FileNotFoundError(os.path.join(p, part))
        p = os.path.join(p, hit)
    return p


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


def des_int(des, section, key, default=0):
    return int(float(des.get(section.lower(), {}).get(key.lower(), default)))


def camera(des):
    """The cockpit art's camera: focal length in pixels and optical centre (CX, CY)."""
    angle = float(des.get("camera", {}).get("viewangle", 105.0))
    f = 320.0 / math.tan(math.radians(angle) / 2)
    cx = des_int(des, "front", "VFX_StartX") + des_int(des, "front", "VFX_FlightX", 320)
    cy = des_int(des, "front", "VFX_StartY") + des_int(des, "front", "VFX_FlightY", 240)
    return angle, f, cx, cy


def load_r16(path):
    """COCKPIT.R16: 640x480 RGB565. Returns the raw values and float sRGB colours (480, 640, 3)."""
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


def window_field(see, sigma=0.8):
    """The see-through mask as a smooth field (0 opaque .. 1 see-through): its 0.5 contour is the
    window outline as a smooth curve instead of the pixel staircase."""
    r = int(3 * sigma + 1)
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    k /= k.sum()
    f = np.pad(see.astype(np.float64), r, mode='edge')
    f = np.apply_along_axis(lambda a: np.convolve(a, k, 'valid'), 0, f)
    return np.apply_along_axis(lambda a: np.convolve(a, k, 'valid'), 1, f)


def sample_field(field, px, py):
    """Bilinear sample of a per-pixel field at pixel coordinates (pixel i covers [i, i + 1))."""
    h, w = field.shape
    x = np.clip(px - 0.5, 0, w - 1.001)
    y = np.clip(py - 0.5, 0, h - 1.001)
    x0, y0 = x.astype(int), y.astype(int)
    fx, fy = x - x0, y - y0
    return ((field[y0, x0] * (1 - fx) + field[y0, x0 + 1] * fx) * (1 - fy)
            + (field[y0 + 1, x0] * (1 - fx) + field[y0 + 1, x0 + 1] * fx) * fy)


class Cockpit:
    """One cockpit's data: DES, camera, art and see-through mask."""

    def __init__(self, cid, data=None):
        self.cid = cid
        self.data = data_dir(data)
        self.des = parse_des(find_ci(self.data, "VFX", "COCKPIT", f"{cid}.DES"))
        self.angle, self.f, self.cx, self.cy = camera(self.des)
        raw, self.art = load_r16(find_ci(self.data, "VFX", "COCKPIT", cid, "COCKPIT.R16"))
        self.see = see_through_mask(raw)
