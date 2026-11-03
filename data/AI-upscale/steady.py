"""Steadies a movie's AI pictures. Drawn one by one, each gets its own fine detail, which flickers when
they play. Real-ESRGAN's copies don't flicker, so the detail the AI adds over them is averaged over
the neighbouring pictures, each moved onto this one by the motion measured between Real-ESRGAN's
copies, and weighted by how well they fit there. What the AI draws alike in every picture stays, what
it draws differently each time fades. A loop wraps round at its ends."""
import numpy as np

SCALE = 4
SEARCH = 12  # the farthest a block is looked for between neighbouring pictures (original pixels)


def box_mean(a, r):
    """The mean over a square of 2r+1 pixels round each (its edges repeated)."""
    p = np.pad(a, ((r + 1, r), (r + 1, r)), mode='edge')
    c = p.cumsum(0).cumsum(1)
    return (c[2*r + 1:, 2*r + 1:] - c[:-2*r - 1, 2*r + 1:] - c[2*r + 1:, :-2*r - 1] + c[:-2*r - 1, :-2*r - 1])/(2*r + 1)**2


def gray(a):
    return a @ np.array([0.299, 0.587, 0.114])


def flow(a, b, block=2):
    """Where each pixel of picture a is in picture b, as (dy, dx) at 4x: blocks matched at the original's
    size, the motion smoothed and enlarged."""
    h, w = a.shape[0]//SCALE, a.shape[1]//SCALE
    ga = gray(a).reshape(h, SCALE, w, SCALE).mean((1, 3))
    gb = np.pad(gray(b).reshape(h, SCALE, w, SCALE).mean((1, 3)), SEARCH, mode='edge')
    best = np.full((h, w), np.inf)
    fy, fx = np.zeros((h, w)), np.zeros((h, w))
    for dy in range(-SEARCH, SEARCH + 1):
        for dx in range(-SEARCH, SEARCH + 1):
            moved = gb[SEARCH + dy:SEARCH + dy + h, SEARCH + dx:SEARCH + dx + w]
            # a little in favour of small moves, where blocks are alike
            cost = box_mean((ga - moved)**2, block) + 0.05*(dy*dy + dx*dx)
            better = cost < best
            best[better], fy[better], fx[better] = cost[better], dy, dx
    fy, fx = box_mean(fy, 1), box_mean(fx, 1)
    enlarge = lambda f: np.repeat(np.repeat(f, SCALE, 0), SCALE, 1)*SCALE
    return np.stack([enlarge(fy), enlarge(fx)], -1)


def warp(image, f):
    """The image sampled where the flow points (bilinear, at its edges held)."""
    h, w = image.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    y, x = np.clip(yy + f[..., 0], 0, h - 1.001), np.clip(xx + f[..., 1], 0, w - 1.001)
    y0, x0 = np.floor(y).astype(int), np.floor(x).astype(int)
    wy, wx = y - y0, x - x0
    if image.ndim == 3:
        wy, wx = wy[..., None], wx[..., None]
    return (image[y0, x0]*(1 - wy)*(1 - wx) + image[y0 + 1, x0]*wy*(1 - wx)
            + image[y0, x0 + 1]*(1 - wy)*wx + image[y0 + 1, x0 + 1]*wy*wx)


def steady(ai, esrgan, loop, radius=8, sigma=45):
    """The AI pictures (arrays of one size, on black) steadied against Real-ESRGAN's: each its
    Real-ESRGAN copy plus the AI's detail averaged over up to radius pictures before and after."""
    n = len(ai)
    ai, esrgan = [np.asarray(a, dtype=np.float64) for a in ai], [np.asarray(e, dtype=np.float64) for e in esrgan]
    neighbour = lambda t, d: (t + d) % n if loop else (t + d if 0 <= t + d < n else None)
    # the motion to the next and the previous picture, chained for those farther away
    step = {(t, d): flow(esrgan[t], esrgan[s]) for t in range(n) for d in (-1, 1) if (s := neighbour(t, d)) is not None}
    result = []
    for t in range(n):
        detail = ai[t] - esrgan[t]
        total, weight = detail.copy(), np.ones(detail.shape[:2] + (1,))
        for d in (-1, 1):
            f, s = np.zeros(detail.shape[:2] + (2,)), t
            for k in range(1, radius + 1):
                if (s, d) not in step:
                    break
                f = f + warp(step[(s, d)], f)
                s = neighbour(s, d)
                misfit = box_mean(np.abs(warp(esrgan[s], f) - esrgan[t]).mean(-1), 2)
                c = (np.exp(-(misfit/sigma)**2)*(1 - k/(radius + 1)))[..., None]
                total += warp(ai[s] - esrgan[s], f)*c
                weight += c
        result.append(np.clip(esrgan[t] + total/weight, 0, 255))
    return result
