"""How far a result strays from its original: the shift between them and how much they differ."""
import numpy as np
from PIL import Image, ImageFilter


def gray(image):
    return np.asarray(image.convert('L'), dtype=np.float64)


def shift(a, b):
    """The subpixel shift of b against a (grey images of one size), by phase correlation."""
    window = np.outer(np.hanning(a.shape[0]), np.hanning(a.shape[1]))
    fa, fb = np.fft.fft2((a - a.mean())*window), np.fft.fft2((b - b.mean())*window)
    r = fa*np.conj(fb)
    r /= np.abs(r) + 1e-9
    c = np.fft.ifft2(r).real
    y, x = np.unravel_index(np.argmax(c), c.shape)

    def fit(m, z, p):
        d = m - 2*z + p
        return 0 if d == 0 else 0.5*(m - p)/d
    dy = fit(c[(y - 1) % c.shape[0], x], c[y, x], c[(y + 1) % c.shape[0], x])
    dx = fit(c[y, (x - 1) % c.shape[1]], c[y, x], c[y, (x + 1) % c.shape[1]])
    y = y + dy if y + dy < c.shape[0]/2 else y + dy - c.shape[0]
    x = x + dx if x + dx < c.shape[1]/2 else x + dx - c.shape[1]
    return x, y


def measure(source, image):
    """The image shrunk to the source's size against it: the shift (in source pixels), the mean
    difference of the slightly blurred images (0-255) and the mean colour change per channel."""
    small = image.convert('RGB').resize(source.size, Image.BOX)
    sx, sy = shift(gray(source), gray(small))
    blur = ImageFilter.GaussianBlur(1.5)
    a = np.asarray(source.convert('RGB').filter(blur), dtype=np.float64)
    b = np.asarray(small.filter(blur), dtype=np.float64)
    return sx, sy, np.abs(a - b).mean(), (b - a).reshape(-1, 3).mean(axis=0)
