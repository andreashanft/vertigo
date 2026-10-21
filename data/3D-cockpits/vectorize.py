"""
Vector outlines for the cockpit textures: iso-lines of sampled fields (marching squares),
simplified, as SVG paths. Plain Python with numpy.
"""

import numpy as np


def contours(f, level=0.5):
    """Closed iso-lines of the field f (h, w) at level, as a list of (n, 2) arrays of (x, y) in
    sample coordinates (sample [i, j] is at x = j, y = i). Saddles follow the cell's mean."""
    lo = min(float(f.min()), level) - 1
    f = np.pad(np.asarray(f, np.float64), 1, constant_values=lo)  # every line closes
    h, w = f.shape
    b = f >= level
    nh = h * (w - 1)                                    # horizontal edges first, then vertical
    hcross = (b[:, :-1] != b[:, 1:]).ravel()
    vcross = (b[:-1, :] != b[1:, :]).ravel()
    with np.errstate(divide='ignore', invalid='ignore'):
        th = (level - f[:, :-1]) / (f[:, 1:] - f[:, :-1])
        tv = (level - f[:-1, :]) / (f[1:, :] - f[:-1, :])
    hy, hx = np.mgrid[0:h, 0:w - 1]
    vy, vx = np.mgrid[0:h - 1, 0:w]
    pts = np.concatenate([np.stack([hx + np.nan_to_num(th), hy], -1).reshape(-1, 2),
                          np.stack([vx, vy + np.nan_to_num(tv)], -1).reshape(-1, 2)]) - 1
    # the edges of every cell: top, right, bottom, left
    cy, cx = np.mgrid[0:h - 1, 0:w - 1]
    top = cy * (w - 1) + cx
    bottom = (cy + 1) * (w - 1) + cx
    left = nh + cy * w + cx
    right = nh + cy * w + cx + 1
    edges = np.stack([top, right, bottom, left], -1).reshape(-1, 4)
    cross = np.concatenate([hcross, vcross])[edges]
    n = cross.sum(1)
    pairs = []
    two = n == 2
    e2 = edges[two][cross[two]].reshape(-1, 2)
    pairs.append(e2)
    four = np.nonzero(n == 4)[0]
    if len(four):
        corners = np.stack([b[:-1, :-1], b[:-1, 1:], b[1:, 1:], b[1:, :-1]], -1).reshape(-1, 4)[four]
        centre = np.stack([f[:-1, :-1], f[:-1, 1:], f[1:, 1:], f[1:, :-1]], -1).reshape(-1, 4)[four].mean(1) >= level
        e = edges[four]
        # corner k sits between edge k - 1 and edge k (top-left: left and top, ...); the
        # contour cuts off the corners that are on the other side than the centre
        cut_tl = corners[:, 0] != centre
        a = np.where(cut_tl[:, None], np.stack([e[:, 3], e[:, 0]], -1), np.stack([e[:, 0], e[:, 1]], -1))
        c = np.where(cut_tl[:, None], np.stack([e[:, 1], e[:, 2]], -1), np.stack([e[:, 2], e[:, 3]], -1))
        pairs += [a, c]
    pairs = np.concatenate(pairs)
    if not len(pairs):
        return []
    # every crossing edge is shared by exactly two segments: walk the cycles
    both = np.concatenate([pairs, pairs[:, ::-1]])
    both = both[np.argsort(both[:, 0], kind='stable')]
    nodes = both[0::2, 0]
    nbr = np.stack([both[0::2, 1], both[1::2, 1]], -1)
    nbr_i = np.searchsorted(nodes, nbr)
    seen = np.zeros(len(nodes), bool)
    loops = []
    for start in range(len(nodes)):
        if seen[start]:
            continue
        loop, prev, cur = [], -1, start
        while not seen[cur]:
            seen[cur] = True
            loop.append(cur)
            a, c = nbr_i[cur]
            prev, cur = cur, (c if a == prev else a)
        if len(loop) > 2:
            loops.append(pts[nodes[loop]])
    return loops


def simplify(poly, tol):
    """Douglas-Peucker for a closed polyline (n, 2)."""
    if len(poly) < 4:
        return poly
    far = int(np.argmax(((poly - poly[0]) ** 2).sum(1)))
    out = []
    for part in (poly[:far + 1], np.vstack([poly[far:], poly[:1]])):
        keep = np.zeros(len(part), bool)
        keep[0] = keep[-1] = True
        stack = [(0, len(part) - 1)]
        while stack:
            i, j = stack.pop()
            if j <= i + 1:
                continue
            a, b = part[i], part[j]
            d = b - a
            seg = part[i + 1:j] - a
            ln = np.hypot(*d)
            dist = np.abs(d[0] * seg[:, 1] - d[1] * seg[:, 0]) / ln if ln > 1e-12 else np.hypot(seg[:, 0], seg[:, 1])
            k = int(np.argmax(dist))
            if dist[k] > tol:
                keep[i + 1 + k] = True
                stack += [(i, i + 1 + k), (i + 1 + k, j)]
        out.append(part[keep][:-1])
    return np.vstack(out)


def svg_path(loops, scale=1.0, offset=(0.0, 0.0), digits=2):
    """SVG path data of closed loops, with points mapped to p * scale + offset."""
    out = []
    for loop in loops:
        p = loop * scale + np.asarray(offset)
        out.append("M" + " ".join(f"{x:.{digits}f},{y:.{digits}f}" for x, y in p) + "Z")
    return "".join(out)


# ---------------------------------------------------------------------------------------------
# Sharp edges inside a part: the strong, long edges of the art (panel seams, outlines, stripes)
# drawn as crisp lines at any resolution instead of being upsampled softly
# ---------------------------------------------------------------------------------------------
TILE, MARGIN = 16, 3          # detail is computed in tiles of TILE x TILE art pixels (+ margin)
EDGE_CONTRAST = 0.10          # minimum contrast (sRGB luminance) across an edge
EDGE_COHERENCE = 0.75         # how clearly the edge has one direction (structure tensor)
EDGE_LENGTH = 10              # minimum length in pixels


def upsample_matrix(n, s):
    """Catmull-Rom upsampling of n samples by s, pixel centres aligned: (n * s, n)."""
    x = (np.arange(n * s) + 0.5) / s - 0.5
    i0 = np.floor(x).astype(int)
    f = x - i0
    m = np.zeros((n * s, n))
    for k, w in zip(range(-1, 3), (-0.5 * f ** 3 + f ** 2 - 0.5 * f, 1.5 * f ** 3 - 2.5 * f ** 2 + 1,
                                   -1.5 * f ** 3 + 2 * f ** 2 + 0.5 * f, 0.5 * f ** 3 - 0.5 * f ** 2)):
        np.add.at(m, (np.arange(n * s), np.clip(i0 + k, 0, n - 1)), w)
    return m


def _box(a, r):
    p = np.pad(a, [(r, r), (r, r)] + [(0, 0)] * (a.ndim - 2), mode='edge')
    h, w = a.shape[:2]
    return sum(p[dy:dy + h, dx:dx + w] for dy in range(2 * r + 1) for dx in range(2 * r + 1)) / (2 * r + 1) ** 2


def _neighbours(a):
    """The 3x3 neighbourhood of every pixel, stacked on a new last axis (before the channels)."""
    h, w = a.shape[:2]
    p = np.pad(a, [(1, 1), (1, 1)] + [(0, 0)] * (a.ndim - 2), mode='edge')
    return np.stack([p[dy:dy + h, dx:dx + w] for dy in range(3) for dx in range(3)], 2)


def _components(mask):
    """Connected components (4-neighbourhood): a label per pixel (mask.size where not set)."""
    lab = np.where(mask, np.arange(mask.size).reshape(mask.shape), mask.size)
    while True:
        m = lab.copy()
        m[1:] = np.minimum(m[1:], lab[:-1]); m[:-1] = np.minimum(m[:-1], lab[1:])
        m[:, 1:] = np.minimum(m[:, 1:], lab[:, :-1]); m[:, :-1] = np.minimum(m[:, :-1], lab[:, 1:])
        m = np.where(mask, m, mask.size)
        if np.array_equal(m, lab):
            return lab
        lab = m


def edge_detail(rgb, srgb, s):
    """Sharp edges of a colour field: rgb (h, w, 3) linear colours, srgb the same in sRGB (where
    the edges are judged). Returns {(ty, tx): tile} for the tiles that have such edges, each with
    the change to the smooth s x upsampling ("d", (TILE * s + 2)^2 x 3, one pixel of margin) and
    the edge's two sides as fields whose 0.5 contours outline them ("dark", "bright", same size),
    and the darker and brighter colour beside every pixel (lo, hi: (h, w, 3) linear)."""
    h, w = rgb.shape[:2]
    luma = srgb @ np.array([0.299, 0.587, 0.114])
    smooth = np.median(_neighbours(luma), -1)
    gy, gx = np.gradient(smooth)
    jxx, jyy, jxy = _box(gx * gx, 2), _box(gy * gy, 2), _box(gx * gy, 2)
    coherence = np.sqrt((jxx - jyy) ** 2 + 4 * jxy ** 2) / np.maximum(jxx + jyy, 1e-9)
    near = _neighbours(luma)
    cols = _neighbours(rgb)
    lo = np.take_along_axis(cols, near.argmin(-1)[..., None, None], 2)[:, :, 0]
    hi = np.take_along_axis(cols, near.argmax(-1)[..., None, None], 2)[:, :, 0]
    contrast = near.max(-1) - near.min(-1)
    # edge lines: maxima of the gradient across the edge, strong, with one direction, long
    g = np.hypot(gx, gy)
    direction = (np.round(np.arctan2(gy, gx) / (np.pi / 4)) % 4).astype(int)
    gp = np.pad(g, 1)
    ridge = np.zeros((h, w), bool)
    for k, (dy, dx) in enumerate(((0, 1), (1, 1), (1, 0), (1, -1))):
        a = gp[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
        b = gp[1 - dy:1 - dy + h, 1 - dx:1 - dx + w]
        ridge |= (direction == k) & (g >= a) & (g >= b)
    strong = ridge & (contrast > EDGE_CONTRAST) & (coherence > EDGE_COHERENCE)
    lab = _components(strong | ((_box(strong.astype(float), 1) > 0.2) & ridge))
    ids, counts = np.unique(lab[strong], return_counts=True)
    line = strong & np.isin(lab, ids[counts >= EDGE_LENGTH])
    if not line.any():
        return {}, lo, hi
    edge = _box((_box(line.astype(float), 1) > 0).astype(float), 1)
    n = TILE + 2 * MARGIN
    up = upsample_matrix(n, s)

    def upsample(a):
        if a.ndim == 2:
            return up @ a @ up.T
        return np.stack([up @ a[..., c] @ up.T for c in range(a.shape[2])], -1)

    pad = [(MARGIN, MARGIN + TILE), (MARGIN, MARGIN + TILE)]
    p_rgb, p_lo, p_hi = (np.pad(a, pad + [(0, 0)], mode='edge') for a in (rgb, lo, hi))
    p_edge = np.pad(edge, pad)
    tiles = {}
    weights = np.array([0.2126, 0.7152, 0.0722])
    keep = slice(MARGIN * s - 1, (MARGIN + TILE) * s + 1)
    for ty in range(0, h, TILE):
        for tx in range(0, w, TILE):
            if not edge[ty:ty + TILE, tx:tx + TILE].any():
                continue
            sub = (slice(ty, ty + n), slice(tx, tx + n))
            smooth8, lo8, hi8 = upsample(p_rgb[sub]), upsample(p_lo[sub]), upsample(p_hi[sub])
            e8 = np.clip(upsample(p_edge[sub]), 0, 1)
            # which side of the edge: compared in (roughly) perceptual luminance
            y, ylo, yhi = (np.maximum(a @ weights, 0) ** (1 / 2.2) for a in (smooth8, lo8, hi8))
            t = (y - ylo) / np.maximum(yhi - ylo, 1e-4)
            ty_, tx_ = np.gradient(t)
            a = np.clip((t - 0.5) / np.maximum(np.hypot(ty_, tx_), 1e-3) + 0.5, 0, 1)
            sharp = lo8 + (hi8 - lo8) * a[..., None]
            zone = (e8 > 0.5).astype(np.float32)
            tiles[(ty, tx)] = dict(d=((sharp - smooth8) * e8[..., None])[keep, keep].astype(np.float32),
                                   dark=((1 - a) * zone)[keep, keep].astype(np.float32),
                                   bright=(a * zone)[keep, keep].astype(np.float32))
    return tiles, lo, hi


def sample_tiles(tiles, key, x, y, s):
    """Bilinear samples of the tiles' key at art pixel positions x, y (pixel i covers [i, i + 1),
    in the coordinates of the field edge_detail was given); 0 outside the tiles."""
    shape = tiles[next(iter(tiles))][key].shape[2:] if tiles else ()
    out = np.zeros(x.shape + shape, np.float32)
    if not tiles:
        return out
    tile_of = (np.floor(y / TILE).astype(np.int64) << 20) + np.floor(x / TILE).astype(np.int64)
    order = np.argsort(tile_of, kind='stable')
    sorted_keys = tile_of[order]
    for (oy, ox), tile in tiles.items():
        k = ((oy // TILE) << 20) + ox // TILE
        sel = order[np.searchsorted(sorted_keys, k):np.searchsorted(sorted_keys, k, 'right')]
        if not len(sel):
            continue
        img = tile[key]
        n = img.shape[0]
        fx = (x[sel] - ox) * s - 0.5 + 1           # +1: the tile's margin
        fy = (y[sel] - oy) * s - 0.5 + 1
        x0 = np.clip(np.floor(fx).astype(int), 0, n - 2)
        y0 = np.clip(np.floor(fy).astype(int), 0, n - 2)
        wx, wy = np.clip(fx - x0, 0, 1), np.clip(fy - y0, 0, 1)
        if img.ndim == 3:
            wx, wy = wx[:, None], wy[:, None]
        out[sel] = ((img[y0, x0] * (1 - wx) + img[y0, x0 + 1] * wx) * (1 - wy)
                    + (img[y0 + 1, x0] * (1 - wx) + img[y0 + 1, x0 + 1] * wx) * wy)
    return out


# ---------------------------------------------------------------------------------------------
# Filling paths: the rasteriser for the SVG textures (paths of straight segments, nonzero or
# even-odd fill, solid colours), so the texture is exactly what the SVG file says
# ---------------------------------------------------------------------------------------------
SUB = 4                       # sub-scanlines per pixel row


def coverage(loops, shape, origin=(0, 0), rule="nonzero"):
    """How much of every pixel of an image of shape (h, w) the closed loops ((n, 2) arrays of x, y
    in pixels; pixel [i, j] covers x in [j, j + 1), y in [i, i + 1)) cover, as floats 0..1.
    origin: the image's top left corner in the loops' coordinates."""
    h, w = shape
    segs = []
    for loop in loops:
        p = np.asarray(loop, np.float64) - origin
        if len(p) < 3:
            continue
        segs.append(np.concatenate([p, np.roll(p, -1, 0)], 1))
    out = np.zeros((h, w), np.float64)
    if not segs:
        return out
    s = np.concatenate(segs)
    s = s[s[:, 1] != s[:, 3]]
    if not len(s):
        return out
    up = s[:, 3] > s[:, 1]
    y0 = np.where(up, s[:, 1], s[:, 3]); y1 = np.where(up, s[:, 3], s[:, 1])
    xa = np.where(up, s[:, 0], s[:, 2]); xb = np.where(up, s[:, 2], s[:, 0])
    direction = np.where(up, 1, -1)
    slope = (xb - xa) / (y1 - y0)
    r0 = max(int(np.floor(y0.min() * SUB)), 0)
    r1 = min(int(np.ceil(y1.max() * SUB)), h * SUB)
    diff = np.zeros((h, w + 2), np.float64)
    step = max(1, int(4e6 // max(len(s), 1)))
    for c0 in range(r0, r1, step):
        ys = (np.arange(c0, min(c0 + step, r1)) + 0.5) / SUB
        hit = (y0[None, :] <= ys[:, None]) & (ys[:, None] < y1[None, :])
        rows, cols = np.nonzero(hit)
        if not len(rows):
            continue
        x = xa[cols] + (ys[rows] - y0[cols]) * slope[cols]
        # per sample row: crossings sorted by x, inside between crossing k and k + 1
        order = np.lexsort((x, rows))
        rows, x, d = rows[order], x[order], direction[cols][order]
        start = np.searchsorted(rows, rows, 'left')
        wind = np.cumsum(d)
        wind -= np.concatenate([[0], wind])[start]
        if rule == "evenodd":
            inside = (np.arange(len(rows)) - start) % 2 == 0
        else:
            inside = wind != 0
        nxt = np.arange(1, len(rows) + 1)
        ok = inside & (nxt < len(rows))
        ok[ok] &= rows[nxt[ok]] == rows[ok]
        r = ((c0 + rows[ok]) // SUB).astype(int)
        a = np.clip(x[ok], 0, w)
        b = np.clip(x[nxt[ok]], 0, w)
        keep = b > a
        r, a, b = r[keep], a[keep], b[keep]
        ia, ib = np.floor(a).astype(int), np.floor(b).astype(int)
        same = ia == ib
        np.add.at(out, (r[same], np.minimum(ia[same], w - 1)), (b[same] - a[same]) / SUB)
        r, a, b, ia, ib = r[~same], a[~same], b[~same], ia[~same], ib[~same]
        np.add.at(out, (r, ia), (ia + 1 - a) / SUB)                       # first, partial pixel
        inb = ib < w
        np.add.at(out, (r[inb], ib[inb]), (b[inb] - ib[inb]) / SUB)       # last, partial pixel
        np.add.at(diff, (r, ia + 1), 1 / SUB)                              # full pixels between
        np.add.at(diff, (r, ib), -1 / SUB)
    out += np.cumsum(diff, 1)[:, :w]
    return np.clip(out, 0, 1)
