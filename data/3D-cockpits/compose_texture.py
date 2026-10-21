"""
Make a 3D cockpit's texture from the original art (COCKPIT.R16) and the maps of bake_maps.py,
and the textured model. Needs numpy and OpenImageIO only, so it runs in Blender's Python:

  /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
      --python compose_texture.py -- --cockpit NNN [--art FILE] [--soft] [--suffix S]
      [--quality 95] [--data DIR] [--out DIR]

  --art     take the colours from this image instead of COCKPIT.R16: the art upscaled by an
            integer factor that divides 8 (2x, 4x, 8x), e.g. by Real-ESRGAN; the edges between
            the parts still come from the model
  --soft    don't sharpen the art's strong edges (without --art they are drawn as crisp lines)
  --suffix  appended to the output names, to compare variants

Writes out/NNN/cockpit_NNN_textured.glb, its texture textures/cockpit_NNN.jpg, the vector
version of the art vector/cockpit_NNN.svg and renders/eye_8x.png (the art as the texture shows
it from the eye, at 8x).

How it works
  The art is split into the model's parts: the maps say which part the eye sees at every
  sub-pixel (eye.exr). Every part gets a colour field of its own, made from the art pixels it
  covers and continued smoothly beyond them (push-pull). A texel takes its colour only from its
  own part's field, at the pixel where the eye sees it (view.exr). So the edges between parts
  are the model's edges, sharp at any resolution, and no part gets the colours of the part in
  front of or behind it, also where the head moves. Far from the art's pixels of its part, and
  where the surface turns away from the eye, a texel blends into the fallback: the model lit
  by the scene lights (shade.exr), tinted with the part's colour.
"""

import base64
import json
import os
import struct
import sys
import time

import numpy as np
import OpenImageIO as oiio

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cockpitdata as CD  # noqa: E402
import vectorize as VZ  # noqa: E402

ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]


def arg(name, default=None):
    return ARGV[ARGV.index(name) + 1] if name in ARGV else default


CID = arg("--cockpit", "000")
OUT = arg("--out") or os.path.join(HERE, "out", CID)
MAPS = os.path.join(OUT, "maps")
QUALITY = int(arg("--quality", 95))
SUFFIX = arg("--suffix", "")
SHARPEN = "--soft" not in ARGV and not arg("--art")   # strong edges as crisp lines (vectorize)
PAD = 64           # the colour fields reach this far beyond the 640x480 frame (pixels)
DECAY = 0.7        # confidence per halving of the resolution away from the part's pixels
T0 = time.time()


def log(*a):
    print(f"[compose {CID} {time.time() - T0:6.1f}s]", *a, flush=True)


def read(name):
    return oiio.ImageBuf(os.path.join(MAPS, name)).get_pixels(oiio.FLOAT)


def write(path, a, quality=None):
    """Write float sRGB (h, w, c) in [0, 1] as an 8-bit image."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    spec = oiio.ImageSpec(a.shape[1], a.shape[0], a.shape[2], oiio.UINT8)
    if quality:
        spec.attribute("CompressionQuality", quality)
        spec.attribute("jpeg:subsampling", "4:4:4")
    o = oiio.ImageOutput.create(path)
    o.open(path, spec)
    o.write_image(np.ascontiguousarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8)))
    o.close()


def to_linear(x):
    return np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)


def to_srgb(x):
    x = np.clip(x, 0, 1)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - 0.055)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def lum(x):
    return x @ np.array([0.2126, 0.7152, 0.0722], np.float32)


# ---------------------------------------------------------------------------------------------
# Image helpers
# ---------------------------------------------------------------------------------------------
def down(a):
    """2x2 sum, odd sizes padded with zeros."""
    h, w = a.shape[:2]
    a = np.pad(a, [(0, h % 2), (0, w % 2)] + [(0, 0)] * (a.ndim - 2))
    return a[0::2, 0::2] + a[1::2, 0::2] + a[0::2, 1::2] + a[1::2, 1::2]


def up(a, shape):
    """Bilinear upsampling by 2 (pixel centres aligned) and crop to shape."""
    h, w = shape[:2]
    y = np.clip((np.arange(h) + 0.5) / 2 - 0.5, 0, a.shape[0] - 1)
    x = np.clip((np.arange(w) + 0.5) / 2 - 0.5, 0, a.shape[1] - 1)
    y0, x0 = np.floor(y).astype(int), np.floor(x).astype(int)
    y1, x1 = np.minimum(y0 + 1, a.shape[0] - 1), np.minimum(x0 + 1, a.shape[1] - 1)
    fy, fx = (y - y0)[:, None], (x - x0)[None, :]
    if a.ndim == 3:
        fy, fx = fy[..., None], fx[..., None]
    top = a[y0][:, x0] * (1 - fx) + a[y0][:, x1] * fx
    bot = a[y1][:, x0] * (1 - fx) + a[y1][:, x1] * fx
    return top * (1 - fy) + bot * fy


def push_pull(w, c):
    """Fill colour c where the weight w (0..1) is low from the surrounding weighted colours
    (Gortler et al.'s push-pull). Returns the filled colours and a confidence that is w at the
    pixels themselves and decays by DECAY per halving of the resolution away from them."""
    ws, ns = [np.clip(w, 0, 1)], [np.clip(w, 0, 1)[..., None] * c]
    while min(ws[-1].shape) > 2:
        wd = down(ws[-1])
        ns.append(down(ns[-1]) * (np.minimum(wd, 1) / np.maximum(wd, 1e-12))[..., None])
        ws.append(np.minimum(wd, 1))
    col = ns[-1] / np.maximum(ws[-1], 1e-12)[..., None]
    conf = ws[-1]
    for wl, nl in zip(ws[-2::-1], ns[-2::-1]):
        col = nl + (1 - wl)[..., None] * up(col, wl.shape)
        conf = wl + (1 - wl) * DECAY * up(conf, wl.shape)
    return col, conf


def catmull_rom(img, x, y):
    """Sample img (h, w, ...) at float pixel positions (centres at integers), clamped."""
    h, w = img.shape[:2]
    x0, y0 = np.floor(x).astype(np.int32), np.floor(y).astype(np.int32)
    fx, fy = (x - x0).astype(np.float32), (y - y0).astype(np.float32)

    def weights(f):
        f2, f3 = f * f, f * f * f
        return (-0.5 * f3 + f2 - 0.5 * f, 1.5 * f3 - 2.5 * f2 + 1,
                -1.5 * f3 + 2 * f2 + 0.5 * f, 0.5 * f3 - 0.5 * f2)

    wx, wy = weights(fx), weights(fy)
    out = 0
    for j in range(4):
        yy = np.clip(y0 + j - 1, 0, h - 1)
        row = 0
        for k in range(4):
            xx = np.clip(x0 + k - 1, 0, w - 1)
            v = img[yy, xx]
            row = row + (wx[k][:, None] * v if v.ndim == 2 else wx[k] * v)
        out = out + (wy[j][:, None] * row if row.ndim == 2 else wy[j] * row)
    return out


def bilinear(img, x, y):
    h, w = img.shape[:2]
    x = np.clip(x, 0, w - 1); y = np.clip(y, 0, h - 1)
    x0, y0 = np.minimum(np.floor(x).astype(np.int32), w - 2), np.minimum(np.floor(y).astype(np.int32), h - 2)
    fx, fy = x - x0, y - y0
    return ((img[y0, x0] * (1 - fx) + img[y0, x0 + 1] * fx) * (1 - fy)
            + (img[y0 + 1, x0] * (1 - fx) + img[y0 + 1, x0 + 1] * fx) * fy)


# ---------------------------------------------------------------------------------------------
# 1. Inputs
# ---------------------------------------------------------------------------------------------
info = json.load(open(os.path.join(MAPS, "parts.json")))
cp = CD.Cockpit(CID, arg("--data"))
art = to_linear(cp.art.astype(np.float32))
opaque = ~cp.see
NP = len(info["parts"]) + 1
S = info["eye_scale"]
# the colours: the art, or an upscaled version of it (U x)
if arg("--art"):
    source = to_linear(oiio.ImageBuf(arg("--art")).get_pixels(oiio.FLOAT)[..., :3])
    U = source.shape[1] // 640
    if source.shape[:2] != (480 * U, 640 * U) or S % U:
        sys.exit(f"--art: {source.shape[1]}x{source.shape[0]} isn't 640x480 times 1, 2, 4 or 8")
else:
    source, U = art, 1
eye = read("eye.exr")
eye_part = eye[..., 0].astype(np.int32)
del eye
# coverage of every part in every art pixel, from the S x S sub-pixels of the eye render
key = (np.arange(480 * 640, dtype=np.int64).reshape(480, 1, 640, 1) * NP
       + eye_part.reshape(480, S, 640, S)).ravel()
cover = (np.bincount(key, minlength=480 * 640 * NP).reshape(480, 640, NP) / (S * S)).astype(np.float32)
log(f"inputs: {NP - 1} parts, eye render {S}x, colours {U}x")

view = read("view.exr")
part_mat = read("part.exr")
RES = view.shape[0]
tex_part = part_mat[..., 0].astype(np.int32).ravel()
tex_mat = part_mat[..., 1].astype(np.int32).ravel()
del part_mat
order = np.argsort(tex_part, kind='stable')
bounds = np.searchsorted(tex_part[order], np.arange(NP + 1))
view = view.reshape(-1, 3)
shade = read("shade.exr")
shade_l = lum(np.repeat(np.repeat(shade, 2, 0), 2, 1)).ravel()
del shade

# ---------------------------------------------------------------------------------------------
# 2. Colour field per part, sampled at its texels
# ---------------------------------------------------------------------------------------------
field_rgb = np.zeros((RES * RES, 3), np.float32)
field_a = np.zeros(RES * RES, np.float32)
fields = {}
canvas = (480 + 2 * PAD, 640 + 2 * PAD)
canvas_u = (canvas[0] * U, canvas[1] * U)
opaque_u = np.repeat(np.repeat(opaque, U, 0), U, 1)


def box3(a):
    p = np.pad(a, 1, mode='edge')
    return sum(p[dy:dy + a.shape[0], dx:dx + a.shape[1]] for dy in range(3) for dx in range(3)) / 9


for p in range(1, NP):
    texels = order[bounds[p]:bounds[p + 1]]
    cov = cover[..., p]
    if not len(texels) or cov.max() < 0.5:
        continue
    # trust pixels the part covers fully, and less so at its rim (they mix in what's behind it)
    w = smoothstep(0.55, 0.95, cov) * (0.15 + 0.85 * smoothstep(0.6, 0.95, box3(cov))) * opaque
    if w.sum() < 0.5:
        continue
    wc = np.zeros(canvas, np.float32)
    wc[PAD:PAD + 480, PAD:PAD + 640] = w
    if U == 1:
        cc = np.zeros(canvas + (3,), np.float32)
        cc[PAD:PAD + 480, PAD:PAD + 640] = art
        col, conf = push_pull(wc, cc)
    else:   # the same at U x, with the part's coverage of the finer pixels
        conf = push_pull(wc, np.zeros(canvas + (1,), np.float32))[1]
        cov_u = (eye_part == p).reshape(480 * U, S // U, 640 * U, S // U).mean((1, 3))
        w_u = np.repeat(np.repeat(w, U, 0), U, 1) * smoothstep(0.55, 0.95, cov_u) * opaque_u
        wc = np.zeros(canvas_u, np.float32)
        cc = np.zeros(canvas_u + (3,), np.float32)
        wc[PAD * U:(PAD + 480) * U, PAD * U:(PAD + 640) * U] = w_u
        cc[PAD * U:(PAD + 480) * U, PAD * U:(PAD + 640) * U] = source
        col = push_pull(wc, cc)[0]
    ys, xs = np.nonzero(cov > 0)
    y0, x0 = max((ys.min() + PAD - 4) * U, 0), max((xs.min() + PAD - 4) * U, 0)
    crop = col[y0:(ys.max() + PAD + 5) * U, x0:(xs.max() + PAD + 5) * U]
    tiles, lo, hi = VZ.edge_detail(crop, to_srgb(crop), S) if SHARPEN else ({}, None, None)
    fields[p] = dict(x0=x0, y0=y0, col=crop, tiles=tiles, lo=lo, hi=hi)
    for i in range(0, len(texels), 1 << 22):
        t = texels[i:i + (1 << 22)]
        x, y = view[t, 0] + PAD - 0.5, view[t, 1] + PAD - 0.5
        inside = (x > -1) & (x < canvas[1]) & (y > -1) & (y < canvas[0]) & (view[t, 2] > 0)
        xu, yu = (view[t, 0] + PAD) * U - 0.5, (view[t, 1] + PAD) * U - 0.5
        rgb = catmull_rom(col, xu, yu)
        if tiles:
            rgb += VZ.sample_tiles(tiles, "d", x + 0.5 - x0, y + 0.5 - y0, S)
        field_rgb[t] = np.maximum(rgb, 0)
        field_a[t] = bilinear(conf, x, y) * inside * smoothstep(0.12, 0.35, view[t, 2])
log(f"fields: {len(fields)} parts with art, "
    f"{sum(len(f['tiles']) for f in fields.values())} tiles with sharp edges")

# ---------------------------------------------------------------------------------------------
# 3. Fallback: the lit model, tinted per part (else per material, else overall) with the colour
#    the art gives it
# ---------------------------------------------------------------------------------------------
core = field_a > 0.9
glob = field_rgb[core].mean(0) / max(shade_l[core].mean(), 1e-5)
tint_mat = {}
for m in np.unique(tex_mat):
    sel = core & (tex_mat == m)
    if sel.sum() > 2000:
        tint_mat[m] = field_rgb[sel].mean(0) / max(shade_l[sel].mean(), 1e-5)
final = np.zeros((RES * RES, 3), np.float32)
for p in range(1, NP):
    t = order[bounds[p]:bounds[p + 1]]
    if not len(t):
        continue
    sel = t[core[t]]
    if len(sel) > 2000:
        tint = field_rgb[sel].mean(0) / max(shade_l[sel].mean(), 1e-5)
    else:
        mats, counts = np.unique(tex_mat[t], return_counts=True)
        tint = tint_mat.get(mats[counts.argmax()], glob)
    a = field_a[t][:, None]
    final[t] = a * field_rgb[t] + (1 - a) * shade_l[t][:, None] * tint
log(f"art covers {(field_a > 0.5).sum() / max((tex_part > 0).sum(), 1):.1%} of the used atlas")
del field_rgb, shade_l
# fill the texels between the islands with their colours, so that mipmaps and filtering at the
# islands' edges don't pull in black
final, _ = push_pull((tex_part > 0).reshape(RES, RES).astype(np.float32), final.reshape(RES, RES, 3))
texture = to_srgb(final)
del final
tex_path = os.path.join(OUT, "textures", f"cockpit_{CID}{SUFFIX}.jpg")
write(tex_path, texture, QUALITY)
del texture
log(f"texture: {tex_path} ({os.path.getsize(tex_path) / 1e6:.1f} MB)")

# ---------------------------------------------------------------------------------------------
# 4. The textured model: the atlas model with the placeholder image replaced
# ---------------------------------------------------------------------------------------------


def replace_image(glb_in, glb_out, image_bytes, mime="image/jpeg"):
    data = open(glb_in, "rb").read()
    jlen = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20 + jlen])
    blen = struct.unpack_from("<I", data, 20 + jlen)[0]
    binary = data[28 + jlen:28 + jlen + blen]
    image = doc["images"][0]
    views = doc["bufferViews"]
    chunks, offset = [], 0
    for i, v in enumerate(views):   # rebuild the binary chunk with the new image
        b = image_bytes if i == image["bufferView"] else \
            binary[v.get("byteOffset", 0):v.get("byteOffset", 0) + v["byteLength"]]
        v["byteOffset"], v["byteLength"] = offset, len(b)
        chunks.append(b + b"\0" * (-len(b) % 4))
        offset += len(chunks[-1])
    image["mimeType"] = mime
    doc["buffers"][0]["byteLength"] = offset
    js = json.dumps(doc, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    body = b"".join(chunks)
    with open(glb_out, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 28 + len(js) + len(body)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", len(body), 0x004E4942) + body)


glb = os.path.join(OUT, f"cockpit_{CID}_textured{SUFFIX}.glb")
replace_image(os.path.join(MAPS, f"cockpit_{CID}_atlas.glb"), glb, open(tex_path, "rb").read())
log(f"model: {glb} ({os.path.getsize(glb) / 1e6:.1f} MB)")

# ---------------------------------------------------------------------------------------------
# 5. The art as the texture shows it from the eye, at S x the original resolution
# ---------------------------------------------------------------------------------------------
eye8 = np.zeros((480 * S * 640 * S, 3), np.float32)
flat = eye_part.ravel()
px_order = np.argsort(flat, kind='stable')
px_bounds = np.searchsorted(flat[px_order], np.arange(NP + 1))
for p, f in fields.items():
    sel = px_order[px_bounds[p]:px_bounds[p + 1]]
    x = ((sel % (640 * S) + 0.5) / S + PAD) * U - 0.5 - f["x0"]
    y = ((sel // (640 * S) + 0.5) / S + PAD) * U - 0.5 - f["y0"]
    rgb = catmull_rom(f["col"], x, y)
    if f["tiles"]:
        rgb += VZ.sample_tiles(f["tiles"], "d", x + 0.5, y + 0.5, S)
    eye8[sel] = np.maximum(rgb, 0)
eye8 = eye8.reshape(480 * S, 640 * S, 3)
eye8 = to_srgb(eye8)
write(os.path.join(OUT, "renders", f"eye_8x{SUFFIX}.png"), eye8)
small = eye8.reshape(480, S, 640, S, 3).mean((1, 3))
err = np.abs(small - cp.art)[opaque].mean()
write(os.path.join(OUT, "renders", f"eye_8x_vs_original{SUFFIX}.png"),
      np.concatenate([np.where(opaque[..., None], cp.art, 0), small], 1))
log(f"eye view: mean difference to the original {err * 255:.1f}/255")

# ---------------------------------------------------------------------------------------------
# 6. The vector version of the art: the window and every part's outline as paths, each part
#    filled with its colour field. Any SVG viewer shows it at any size.
# ---------------------------------------------------------------------------------------------


def png_base64(rgb):
    tmp = os.path.join(OUT, "vector", "_field.png")
    write(tmp, rgb)
    data = base64.b64encode(open(tmp, "rb").read()).decode()
    os.remove(tmp)
    return data


Q = S // 2          # outlines from the parts' coverage at Q x the art's resolution
depth = read("eye.exr")[..., 1]
names = {q["index"]: q["name"] for q in info["parts"]}
clips, images = [], []
for p, f in fields.items():
    x0, y0, col = f["x0"], f["y0"], f["col"]
    sel = px_order[px_bounds[p]:px_bounds[p + 1]]
    if len(sel) < S * S:
        continue
    ys, xs = sel // (640 * S), sel % (640 * S)
    by0, by1 = (ys.min() // S) * S, (ys.max() // S + 1) * S
    bx0, bx1 = (xs.min() // S) * S, (xs.max() // S + 1) * S
    mask = eye_part[by0:by1, bx0:bx1] == p
    h, w = mask.shape
    cov_q = mask.reshape(h // (S // Q), S // Q, w // (S // Q), S // Q).mean((1, 3))
    loops = [VZ.simplify(l, 0.2) for l in VZ.contours(cov_q, 0.5)]
    # sample [i, j] of cov_q is at art pixel ((j + 0.5) / Q + bx0 / S, ...)
    d = VZ.svg_path(loops, 1 / Q, (bx0 / S + 0.5 / Q, by0 / S + 0.5 / Q))
    clips.append(f'<clipPath id="c{p}"><path clip-rule="evenodd" d="{d}"/></clipPath>')
    hh, ww = col.shape[:2]
    at = f'x="{x0 / U - PAD}" y="{y0 / U - PAD}" width="{ww / U}" height="{hh / U}" preserveAspectRatio="none"'
    layers = [f'<image {at} xlink:href="data:image/png;base64,{png_base64(to_srgb(col))}"/>']
    # the sharp edges: their dark and bright sides, each filled with that side's colours
    for side, colours in (("dark", f["lo"]), ("bright", f["hi"])):
        d = ""
        for (ty, tx), tile in f["tiles"].items():
            # sample [i, j] of a tile is at crop pixel (tx + (j + 0.5) / S, ty + (i + 0.5) / S)
            loops = [l for l in (VZ.simplify(l, 0.25) for l in VZ.contours(tile[side][1:-1, 1:-1], 0.5))
                     if len(l) > 2]
            d += VZ.svg_path(loops, 1 / S, (x0 - PAD + tx + 0.5 / S, y0 - PAD + ty + 0.5 / S))
        if d:
            clips.append(f'<clipPath id="{side[0]}{p}"><path clip-rule="evenodd" d="{d}"/></clipPath>')
            layers.append(f'<image {at} clip-path="url(#{side[0]}{p})" '
                          f'xlink:href="data:image/png;base64,{png_base64(to_srgb(colours))}"/>')
    images.append((float(np.median(depth.ravel()[sel])),
                   f'<g id="{names[p]}" clip-path="url(#c{p})">{"".join(layers)}</g>'))
window = [VZ.simplify(l, 0.05) for l in VZ.contours(CD.window_field(cp.see), 0.5)]
base = np.where(opaque[..., None], small, 0)
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
       f'width="{640 * S}" height="{480 * S}" viewBox="0 0 640 480" image-rendering="optimizeQuality">',
       f'<title>Cockpit {CID}</title>',
       '<desc>The cockpit art redrawn from the 3D model: every part clipped to its outline as the '
       'eye sees it, filled with its colour field (compose_texture.py).</desc>',
       '<defs>', *clips, '</defs>',
       f'<image id="base" width="640" height="480" xlink:href="data:image/png;base64,{png_base64(base)}"/>',
       '<g id="parts">', *[s for z, s in sorted(images, key=lambda t: -t[0])], '</g>',
       f'<path id="window" fill="none" d="{VZ.svg_path(window, 1, (0.5, 0.5))}"/>',
       '</svg>']
svg_path = os.path.join(OUT, "vector", f"cockpit_{CID}{SUFFIX}.svg")
open(svg_path, "w").write("\n".join(svg))
log(f"vector: {svg_path} ({len(images)} parts, {os.path.getsize(svg_path) / 1e6:.1f} MB)")
log("done")
