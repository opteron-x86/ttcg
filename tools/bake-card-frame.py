#!/usr/bin/env python3
"""Bake the card frame's metal: a bezel and a value medallion, lit from one height field.

    python3 tools/bake-card-frame.py            # writes TesseraUI/assets/card/frame-{1,2,4,8}.png

Needs numpy, scipy and Pillow; nothing else in the build does, so the baked atlases are committed.

Each atlas holds the same pieces at a different pixel density (pixels per card unit, where a card is
180 units wide), so the interface can pick the one nearest the size it draws at. Every piece comes
twice: a grey `base` that the interface multiplies by the metal's colour, and a `light` layer of
white highlights and black shadows drawn over it untinted. The layout below, in card units, is
mirrored in native/src/CardFace.cpp.
"""
from pathlib import Path
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter

ROOT = Path(__file__).resolve().parents[1]
ATLAS = (102, 114)  # the layout below, inside a one-unit margin
MARGIN = 1
COIN = 44          # square sprite; the medallion sits at its centre
COIN_RADIUS = 14.6
COIN_RIM = 2.7
BEZEL = 4.2
PATCH = 14         # corner square, and the depth of each edge strip
EDGE = 32          # length of each edge strip
MASTER = 8         # the densest level; the others are box-filtered from it


class Grid:
    """A padded canvas of `size` units at MASTER pixels per unit."""
    pad = 10

    def __init__(self, width, height):
        self.w, self.h = width, height
        yy, xx = np.mgrid[0:(height + 2 * self.pad) * MASTER, 0:(width + 2 * self.pad) * MASTER]
        self.X = (xx + .5) / MASTER - self.pad
        self.Y = (yy + .5) / MASTER - self.pad

    def box(self, cx, cy, hw, hh, r=0.):
        dx, dy = np.abs(self.X - cx) - hw + r, np.abs(self.Y - cy) - hh + r
        return np.hypot(np.maximum(dx, 0), np.maximum(dy, 0)) + np.minimum(np.maximum(dx, dy), 0) - r

    def circle(self, cx, cy, r):
        return np.hypot(self.X - cx, self.Y - cy) - r

    def crop(self, image, x, y, w, h):
        x0, y0 = (x + self.pad) * MASTER, (y + self.pad) * MASTER
        return image[y0:y0 + h * MASTER, x0:x0 + w * MASTER]


def step(d, soft=.35):
    return np.clip(.5 - d / soft, 0, 1)


def shift(image, down, right):
    return np.roll(np.roll(image, int(down * MASTER), 0), int(right * MASTER), 1)


def blur(image, units):
    return gaussian_filter(image, units * MASTER)


def lighting(height):
    gy, gx = np.gradient(height * 1.9 * MASTER)
    nz = 1 / np.sqrt(gx * gx + gy * gy + 1)
    nx, ny = -gx * nz, -gy * nz
    L = np.array([-.42, -.66, .62])
    L /= np.linalg.norm(L)
    diffuse = np.clip(nx * L[0] + ny * L[1] + nz * L[2], 0, 1)
    half = np.array([L[0], L[1], L[2] + 1])
    half /= np.linalg.norm(half)
    spec = np.clip(nx * half[0] + ny * half[1] + nz * half[2], 0, 1)
    # A pale sky above and dark ground below give the metal something to reflect.
    sky = np.clip(.5 - ny * .9 - nx * .35, 0, 1)
    return diffuse, spec, sky


class Layer:
    """Straight-alpha accumulation of black and white passes."""

    def __init__(self, shape):
        self.rgb = np.zeros(shape, np.float32)
        self.a = np.zeros(shape, np.float32)

    def over(self, value, alpha):
        alpha = np.clip(alpha, 0, 1)
        self.rgb = self.rgb * (1 - alpha) + value * alpha
        self.a = self.a * (1 - alpha) + alpha

    def image(self):
        return np.dstack([self.rgb] * 3 + [self.a])  # premultiplied


def bake(grid, metal, well=None, keep=None, seed=3):
    """Returns premultiplied (base, light) for a metal shape and an optional sunk well inside it."""
    rng = np.random.default_rng(seed)
    shape = grid.X.shape
    noise = lambda units, amount: (lambda n: n / n.std() * amount)(blur(rng.standard_normal(shape), units))
    in_metal = step(metal)
    in_well = step(well) if well is not None else np.zeros(shape)
    height = np.clip(-metal / 1.9, 0, 1)
    if well is not None:
        floor = np.sin(np.clip(-well / 1.5, 0, 1) * np.pi / 2)
        height = height * (1 - floor * .62)
    height = blur(height, .2) + noise(.2, .002)
    diffuse, spec, sky = lighting(height)
    tone = 1 + noise(3., .03) + noise(.5, .012)
    occlusion = np.clip(blur(height, 1.6) - height, 0, 1)
    lum = tone * (.16 + .5 * diffuse + .56 * sky) * (1 - occlusion * .9)
    shine = spec ** 60 * .85 + spec ** 8 * .1 + np.clip(lum - 1, 0, .3)
    if well is not None:
        # Enamel floor: near black, shadowed under its upper-left lip.
        under = np.clip(blur(shift(1 - in_well, 1.3, .9), 1.1), 0, 1)
        enamel = .085 * (.7 + .6 * (.5 + .5 * sky)) * (1 - under * .75)
        lum = lum * (1 - in_well) + enamel * in_well
        shine = shine * (1 - in_well)
    base = np.dstack([np.clip(lum, 0, 1) * in_metal] * 3 + [in_metal])

    light = Layer(shape)
    cast = blur(shift(in_metal, 1.6, 1.1), 1.5)
    light.over(0., cast * (1 - in_metal) * .62)
    light.over(1., shine * in_metal)
    light.over(0., np.clip(1 - np.abs(metal) / .5, 0, 1) * .5)
    if well is not None:
        gy, gx = np.gradient(well)
        lip = np.clip(1 - np.abs(well + .1) / .5, 0, 1)
        facing = np.clip((gx * .55 + gy * .83) * MASTER, -1, 1)
        light.over(1., lip * np.clip(facing, 0, 1) * .5)
        light.over(0., lip * np.clip(-facing, 0, 1) * .55)
    out = light.image()
    if keep is not None:
        out = out * keep[..., None]
        base = base * keep[..., None]
    return base, out


def main():
    atlas = np.zeros((ATLAS[1] * MASTER, ATLAS[0] * MASTER, 4), np.float32)

    def put(image, x, y):
        # Each piece repeats its border one unit outward, so stretching it never samples its neighbour.
        m = MARGIN * MASTER
        image = np.pad(image, ((m, m), (m, m), (0, 0)), mode="edge")
        h, w = image.shape[:2]
        atlas[y * MASTER:y * MASTER + h, x * MASTER:x * MASTER + w] = image

    # Medallion.
    g = Grid(COIN, COIN)
    coin = g.circle(COIN / 2, COIN / 2, COIN_RADIUS)
    base, light = bake(g, coin, coin + COIN_RIM)
    put(g.crop(base, 0, 0, COIN, COIN), 0, 0)
    put(g.crop(light, 0, 0, COIN, COIN), 46, 0)

    # Bezel, cut into nine-slice pieces from a small square card.
    side = 2 * PATCH + EDGE
    g = Grid(side, side)
    outer = g.box(side / 2, side / 2, side / 2, side / 2, 3.)
    window = g.box(side / 2, side / 2, side / 2 - BEZEL, side / 2 - BEZEL, 1.)
    base, light = bake(g, np.maximum(outer, -window), keep=step(outer), seed=5)
    far = PATCH + EDGE
    for layer, corner_y, edge_y, side_y in ((base, 46, 78, 46), (light, 62, 94, 80)):
        for i, (x, y) in enumerate(((0, 0), (far, 0), (0, far), (far, far))):
            put(g.crop(layer, x, y, PATCH, PATCH), i * 16, corner_y)
        put(g.crop(layer, PATCH, 0, EDGE, PATCH), 0, edge_y)
        put(g.crop(layer, PATCH, far, EDGE, PATCH), 34, edge_y)
        put(g.crop(layer, 0, PATCH, PATCH, EDGE), 68, side_y)
        put(g.crop(layer, far, PATCH, PATCH, EDGE), 84, side_y)

    out = ROOT / "TesseraUI/assets/card"
    out.mkdir(parents=True, exist_ok=True)
    for level in (8, 4, 2, 1):
        k = MASTER // level
        h, w = atlas.shape[0] // k, atlas.shape[1] // k
        small = atlas.reshape(h, k, w, k, 4).mean(axis=(1, 3))  # box filter, premultiplied
        alpha = small[..., 3:]
        straight = np.where(alpha > 1e-4, small[..., :3] / np.maximum(alpha, 1e-4), 0)
        rgba = np.dstack([np.clip(straight, 0, 1), alpha])
        Image.fromarray((rgba * 255 + .5).astype(np.uint8), "RGBA").save(out / f"frame-{level}.png", optimize=True)
        print(out / f"frame-{level}.png", f"{w}x{h}")


if __name__ == "__main__":
    main()
