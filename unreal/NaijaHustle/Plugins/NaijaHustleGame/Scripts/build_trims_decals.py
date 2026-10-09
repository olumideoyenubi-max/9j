"""Paints the game's trim sheets and decal atlas from nothing: no photographs, no downloads, all original.

  python3 build_trims_decals.py [folder]        (default ~/Downloads/nh-trims)

Plain Python with numpy and Pillow (pip install numpy pillow), run outside Unreal. It writes:

  T_NH_Trim_Frames_BaseColor|Normal|ORM.png   window frames, burglar bars, louvres, ledges, gutters, pipes
  T_NH_Trim_Shop_BaseColor|Normal|ORM.png     roller shutters, signboard frame, awning, folding gate, kerb, tiles
  T_NH_Decals.png                             16 decals in a 4 x 4 grid, colour with transparency
  trims.json                                  where every strip and decal sits, for the building kit and the importer
  (the plugin's Data/trim_sheets.json is the same list, kept in the repo so the kit can read it without the pictures)

A trim sheet is 2048 x 2048 and made of strips that run its full width and repeat sideways. A model maps a long thin
face (a window frame, a ledge) onto one strip, so every building shares these three textures. Strips are painted in
light, neutral colours where the material is expected to tint them. BaseColor's alpha is the cut-out for bars and gates.
Normal maps are OpenGL-style, like the surface library's, and ORM is occlusion, roughness, metallic.

Every word on the decals is made up. Phone numbers start 0555, which is not a Nigerian mobile prefix as far as I know.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.expanduser("~"), "Downloads", "nh-trims")
HERE = os.path.dirname(os.path.abspath(__file__))
W = 2048
FONTS = "/System/Library/Fonts/Supplemental/"
rng = np.random.default_rng(419)


def noise(h, w, cell, octaves=3):
    """Soft noise, 0..1, that repeats sideways"""
    total, amp, norm = np.zeros((h, w), np.float32), 1.0, 0.0
    for _ in range(octaves):
        small = rng.random((max(2, h // cell + 2), max(2, w // cell))).astype(np.float32)
        img = Image.fromarray(np.tile(small, (1, 3)))
        big = np.asarray(img.resize((w * 3, h), Image.BICUBIC))[:, w:w * 2]
        total += big * amp
        norm += amp
        amp *= 0.5
        cell = max(2, cell // 2)
    return np.clip(total / norm, 0, 1)


def blur(a, radius):
    """Three box blurs, near enough a Gaussian; done here because Pillow will not blur decimal images"""
    r = max(1, int(round(radius)))
    for _ in range(3):
        for axis in (0, 1):
            pad = [(r + 1, r) if i == axis else (0, 0) for i in (0, 1)]
            c = np.cumsum(np.pad(a, pad, mode="wrap" if axis == 1 else "edge"), axis=axis, dtype=np.float64)
            n = a.shape[axis]
            a = ((np.take(c, range(2 * r + 1, 2 * r + 1 + n), axis) - np.take(c, range(0, n), axis)) / (2 * r + 1)).astype(np.float32)
    return a


def grid(h):
    y = (np.arange(h, dtype=np.float32) + 0.5) / h
    x = np.arange(W, dtype=np.float32)
    return np.repeat(y[:, None], W, 1), np.repeat(x[None, :], h, 0)


def mix(a, b, t):
    t = np.clip(t, 0, 1)[..., None]
    return np.asarray(a, np.float32) * (1 - t) + np.asarray(b, np.float32) * t


def arch(t):
    """0 at both ends, 1 in the middle"""
    return np.clip(np.sin(np.clip(t, 0, 1) * math.pi), 0, 1)


def step(v, lo, hi):
    return np.clip((v - lo) / (hi - lo), 0, 1)


def worn(h, colour, rust=(0.33, 0.16, 0.07), amount=0.3, dirt=0.3):
    """A painted surface: the colour, with rust breaking through and dirt in patches"""
    n1, n2 = noise(h, W, 48), noise(h, W, 160)
    c = mix(np.broadcast_to(np.asarray(colour, np.float32), (h, W, 3)), rust, step(n1, 1 - amount * 0.6, 1 - amount * 0.6 + 0.08))
    c = c * (1 - dirt * 0.5 * n2[..., None])
    return c, step(n1, 1 - amount * 0.6, 1 - amount * 0.6 + 0.08)


def bars(y, x, pitch, radius):
    """Round upright bars every `pitch` pixels: height 0..1 across each bar, and where the bar is"""
    d = np.abs((x % pitch) - pitch / 2) / radius
    return np.sqrt(np.clip(1 - d * d, 0, 1)), d < 1


# ---- strips: each returns colour (h, W, 3), height, roughness, metallic, alpha -----------------------------------------------------

def s_frame_alu(h):
    y, x = grid(h)
    height = 0.35 + 0.4 * step(y, 0.08, 0.2) * step(-y, -0.92, -0.8) + 0.25 * step(y, 0.35, 0.42) * step(-y, -0.65, -0.58)
    n = noise(h, W, 24)
    colour = np.broadcast_to(np.float32([0.62, 0.63, 0.64]), (h, W, 3)) * (0.9 + 0.2 * n[..., None])
    return colour, height, 0.3 + 0.25 * n, np.ones_like(y), None


def s_frame_wood(h):
    y, x = grid(h)
    height = 0.3 + 0.5 * step(y, 0.1, 0.25) * step(-y, -0.9, -0.75)
    grain = np.asarray(Image.fromarray(noise(max(8, h // 6), W, 64)).resize((W, h), Image.BICUBIC))     # stretched sideways: wood grain
    colour, chip = worn(h, (0.82, 0.78, 0.66), rust=(0.36, 0.24, 0.14), amount=0.45, dirt=0.4)
    return colour * (0.92 + 0.16 * grain[..., None]), height - 0.06 * chip + 0.04 * grain, 0.65 + 0.2 * chip, np.zeros_like(y), None


def s_bars(h):
    y, x = grid(h)
    bar, on = bars(y, x, 128, 14)
    rail = (np.abs(y - 0.08) < 0.035) | (np.abs(y - 0.92) < 0.035) | (np.abs(y - 0.5) < 0.02)
    height = np.maximum(bar * on, rail * 0.8)
    colour, rust = worn(h, (0.07, 0.07, 0.08), amount=0.5, dirt=0.2)
    return colour, height, 0.45 + 0.4 * rust, 1 - rust, (on | rail).astype(np.float32)


def s_bars_diamond(h):
    y, x = grid(h)
    u, v = (x + y * h) % 128, (x - y * h) % 128
    line = (np.minimum(u, 128 - u) < 7) | (np.minimum(v, 128 - v) < 7)
    rail = (y < 0.06) | (y > 0.94)
    centre = (np.hypot((x % 128) - 64, (y * h % 128) - 64) < 15)     # a small boss where the bars cross
    colour, rust = worn(h, (0.55, 0.56, 0.58), amount=0.4, dirt=0.3)
    on = line | rail | centre
    return colour, on * 0.7 + centre * 0.3, 0.5 + 0.35 * rust, 1 - rust, on.astype(np.float32)


def s_louvres(h):
    y, x = grid(h)
    blade = (y * 4) % 1.0                                  # four glass blades, each tilted: bright at the top edge
    upright = (x % 512 < 26) | (x % 512 > 486)
    glass = mix(np.float32([0.2, 0.3, 0.33]), np.float32([0.55, 0.68, 0.7]), 1 - blade)
    dust = noise(h, W, 90)
    colour = mix(glass * (0.8 + 0.3 * dust[..., None]), np.float32([0.6, 0.61, 0.62]), upright.astype(np.float32))
    edge = step(blade, 0.0, 0.08) * step(-blade, -1.0, -0.9)
    height = np.where(upright, 0.9, 0.2 + 0.5 * (1 - blade) * edge)
    return colour, height, np.where(upright, 0.4, 0.08 + 0.3 * dust), upright.astype(np.float32), None


def s_ledge(h):
    y, x = grid(h)
    height = 0.2 + 0.25 * step(y, 0.1, 0.2) + 0.35 * step(y, 0.35, 0.55) + 0.2 * np.sin(np.clip((y - 0.55) / 0.3, 0, 1) * math.pi) - 0.5 * step(y, 0.9, 0.97)
    n, streak = noise(h, W, 40), np.asarray(Image.fromarray(noise(4, W, 3, 1)).resize((W, h), Image.BICUBIC))
    stain = step(streak, 0.55, 0.8) * step(y, 0.5, 1.0)          # rain running off the lip
    colour = mix(np.broadcast_to(np.float32([0.66, 0.64, 0.6]), (h, W, 3)) * (0.85 + 0.25 * n[..., None]), np.float32([0.16, 0.17, 0.13]), stain * 0.75)
    return colour, height + 0.05 * n, 0.8 + 0.1 * n, np.zeros_like(y), None


def s_gutter(h):
    y, x = grid(h)
    height = 0.15 + 0.8 * arch((y - 0.1) / 0.8) ** 0.6
    seam = (x % 1024 < 10)
    colour, rust = worn(h, (0.5, 0.52, 0.53), amount=0.55, dirt=0.4)
    return colour * np.where(seam, 0.6, 1.0)[..., None], height - 0.1 * seam, 0.4 + 0.4 * rust, 1 - 0.8 * rust, None


def s_parapet(h):
    y, x = grid(h)
    height = 0.3 + 0.6 * step(y, 0.05, 0.2) * step(-y, -0.95, -0.6)
    n = noise(h, W, 30)
    joint = (x % 512 < 6)
    black = step(noise(h, W, 120), 0.45, 0.7) * step(-y, -0.7, 0.0)   # the soot-black mould Lagos parapets carry
    colour = mix(np.broadcast_to(np.float32([0.6, 0.58, 0.54]), (h, W, 3)) * (0.85 + 0.25 * n[..., None]), np.float32([0.07, 0.08, 0.07]), black * 0.8)
    return colour, height - 0.2 * joint, 0.85 + 0.1 * n, np.zeros_like(y), None


def s_pipe(h):
    y, x = grid(h)
    height = arch(y) ** 0.5
    collar = (x % 768 < 40)
    n = noise(h, W, 60)
    colour = np.broadcast_to(np.float32([0.74, 0.75, 0.72]), (h, W, 3)) * (0.8 + 0.25 * n[..., None]) * np.where(collar, 0.85, 1.0)[..., None]
    return colour, height + 0.08 * collar, 0.45 + 0.2 * n, np.zeros_like(y), None


def s_sill_tile(h):
    y, x = grid(h)
    joint = (x % 128 < 5) | (y < 0.08)
    n = noise(h, W, 50)
    colour = mix(np.broadcast_to(np.float32([0.58, 0.3, 0.2]), (h, W, 3)) * (0.85 + 0.3 * n[..., None]), np.float32([0.4, 0.39, 0.36]), joint.astype(np.float32))
    return colour, 0.7 - 0.4 * joint, np.where(joint, 0.9, 0.35 + 0.2 * n), np.zeros_like(y), None


def s_shutter(h):
    y, x = grid(h)
    slat = (y * 8) % 1.0                                   # eight curved slats
    height = 0.3 + 0.6 * arch(slat) ** 0.7
    guide = (x % 1024 < 30)
    colour, rust = worn(h, (0.72, 0.73, 0.74), amount=0.25, dirt=0.35)
    low = step(y, 0.7, 1.0) * step(noise(h, W, 70), 0.35, 0.6)       # shutters rust from the ground up
    colour = mix(colour, np.float32([0.3, 0.14, 0.06]), low * 0.8) * (0.75 + 0.25 * np.sin(slat * math.pi))[..., None]
    rust = np.maximum(rust, low)
    return colour * np.where(guide, 0.7, 1.0)[..., None], np.where(guide, 0.95, height), 0.4 + 0.45 * rust, 1 - 0.9 * rust, None


def s_shutter_rail(h):
    y, x = grid(h)
    height = 0.4 + 0.5 * step(y, 0.15, 0.3) * step(-y, -0.85, -0.7)
    handle = (np.abs((x % 1024) - 512) < 70) & (np.abs(y - 0.5) < 0.16)
    lock = (np.hypot((x % 1024) - 200, (y - 0.5) * h) < h * 0.22)
    colour, rust = worn(h, (0.3, 0.31, 0.33), amount=0.5, dirt=0.3)
    colour = mix(colour, np.float32([0.62, 0.52, 0.2]), lock.astype(np.float32))        # a brass padlock plate
    return colour, height + 0.1 * handle + 0.12 * lock, 0.45 + 0.4 * rust, 1 - 0.8 * rust, None


def s_sign_frame(h):
    y, x = grid(h)
    height = 0.25 + 0.65 * (step(y, 0.0, 0.08) * step(-y, -0.3, -0.22) + step(y, 0.7, 0.78) * step(-y, -1.0, -0.92))
    rivet = (np.hypot((x % 256) - 128, (y - 0.15) * h) < 7) | (np.hypot((x % 256) - 128, (y - 0.85) * h) < 7)
    colour, rust = worn(h, (0.64, 0.65, 0.66), amount=0.35, dirt=0.3)
    return colour, height + 0.1 * rivet, 0.4 + 0.4 * rust, 1 - 0.8 * rust, None


def s_awning(h):
    y, x = grid(h)
    stripe = ((x // 128) % 2).astype(np.float32)
    weave = 0.5 + 0.5 * np.sin(x * 1.6) * np.sin(y * h * 1.6)
    sag = 0.5 + 0.2 * np.sin(x / 256 * math.pi * 2)
    n = noise(h, W, 80)
    colour = mix(np.broadcast_to(np.float32([0.86, 0.85, 0.8]), (h, W, 3)), np.float32([0.5, 0.5, 0.5]), stripe) * (0.75 + 0.3 * n[..., None])
    grime = step(y, 0.75, 1.0) * 0.5                               # the bottom edge collects dust and exhaust
    return colour * (1 - grime[..., None]), sag + 0.05 * weave, 0.85 + 0.1 * weave, np.zeros_like(y), None


def s_gate(h):
    y, x = grid(h)
    u, v = (x * 1.0 + y * h * 0.5) % 96, (x * 1.0 - y * h * 0.5) % 96
    lattice = (np.minimum(u, 96 - u) < 6) | (np.minimum(v, 96 - v) < 6)
    post = (x % 192 < 14)
    rail = (y < 0.05) | (y > 0.95)
    on = lattice | post | rail
    colour, rust = worn(h, (0.1, 0.1, 0.11), amount=0.55, dirt=0.2)
    return colour, on * 0.6 + post * 0.3, 0.5 + 0.35 * rust, 1 - rust, on.astype(np.float32)


def s_door_frame(h):
    y, x = grid(h)
    height = 0.3 + 0.3 * step(y, 0.08, 0.16) + 0.3 * step(y, 0.4, 0.5) * step(-y, -0.92, -0.84)
    colour, rust = worn(h, (0.42, 0.44, 0.46), amount=0.4, dirt=0.35)
    return colour, height, 0.45 + 0.4 * rust, 1 - 0.8 * rust, None


def s_terrazzo(h):
    y, x = grid(h)
    chips = step(noise(h, W, 6, 1), 0.62, 0.7)
    n = noise(h, W, 120)
    nosing = step(y, 0.0, 0.12) * step(-y, -0.2, -0.1)
    colour = mix(np.broadcast_to(np.float32([0.52, 0.5, 0.46]), (h, W, 3)), np.float32([0.78, 0.74, 0.66]), chips * 0.8) * (0.7 + 0.35 * n[..., None])
    return colour, 0.5 + 0.3 * nosing, 0.3 + 0.35 * n, np.zeros_like(y), None


def s_kerb(h):
    y, x = grid(h)
    block = ((x // 256) % 2).astype(np.float32)                    # the black and white kerb of every Lagos main road
    joint = (x % 256 < 6)
    n, scuff = noise(h, W, 40), step(noise(h, W, 30), 0.55, 0.75)
    colour = mix(np.broadcast_to(np.float32([0.07, 0.07, 0.07]), (h, W, 3)), np.float32([0.82, 0.82, 0.78]), block)
    colour = mix(colour, np.float32([0.45, 0.43, 0.4]), scuff * 0.7) * (0.8 + 0.25 * n[..., None]) * (1 - 0.4 * step(y, 0.75, 1.0))[..., None]
    height = 0.4 + 0.4 * step(y, 0.0, 0.15) - 0.25 * joint - 0.08 * scuff
    return colour, height, 0.75 + 0.15 * n, np.zeros_like(y), None


def s_wall_tiles(h):
    y, x = grid(h)
    joint = (x % 128 < 5) | ((y * h) % 128 < 5)
    shade = np.asarray(Image.fromarray(rng.random((max(1, h // 128), W // 128)).astype(np.float32)).resize((W, h), Image.NEAREST))
    n = noise(h, W, 100)
    colour = mix(np.broadcast_to(np.float32([0.8, 0.8, 0.77]), (h, W, 3)) * (0.88 + 0.12 * shade[..., None]), np.float32([0.3, 0.29, 0.26]), joint.astype(np.float32))
    return colour * (0.8 + 0.25 * n[..., None]), 0.7 - 0.4 * joint, np.where(joint, 0.9, 0.15 + 0.3 * n), np.zeros_like(y), None


SHEETS = {
    "Frames": [("FrameAluminium", 128, s_frame_alu), ("FrameWood", 128, s_frame_wood), ("BurglarBars", 384, s_bars), ("BurglarBarsDiamond", 384, s_bars_diamond),
               ("Louvres", 384, s_louvres), ("Ledge", 192, s_ledge), ("Gutter", 160, s_gutter), ("ParapetCap", 128, s_parapet), ("Pipe", 96, s_pipe), ("SillTile", 64, s_sill_tile)],
    "Shop": [("RollerShutter", 512, s_shutter), ("ShutterRail", 96, s_shutter_rail), ("SignFrame", 128, s_sign_frame), ("Awning", 256, s_awning), ("FoldingGate", 384, s_gate),
             ("DoorFrame", 128, s_door_frame), ("Terrazzo", 160, s_terrazzo), ("Kerb", 128, s_kerb), ("WallTiles", 256, s_wall_tiles)],
}


def save(array, name, mode):
    Image.fromarray((np.clip(array, 0, 1) * 255 + 0.5).astype(np.uint8)).save(os.path.join(OUT, name), optimize=True)


def build_sheet(sheet, strips):
    colour, height, rough, metal, alpha, layout, top = [], [], [], [], [], {}, 0
    for name, h, paint in strips:
        c, hgt, r, m, a = paint(h)
        colour.append(c); height.append(hgt.astype(np.float32)); rough.append(np.broadcast_to(r, (h, W))); metal.append(np.broadcast_to(m, (h, W)))
        alpha.append(np.ones((h, W), np.float32) if a is None else a)
        layout[name] = {"v0": top / W, "v1": (top + h) / W, "pixels": h, "cutout": a is not None}
        top += h
    assert top == W, f"{sheet}: strips add up to {top}, not {W}"
    colour, height, rough, metal, alpha = (np.concatenate(v) for v in (colour, height, rough, metal, alpha))
    soft = blur(height, 1)
    dx = (np.roll(soft, -1, 1) - np.roll(soft, 1, 1)) * 6.0          # sideways it wraps; up and down it must not cross strips
    dy = np.gradient(soft, axis=0) * 6.0
    length = np.sqrt(dx * dx + dy * dy + 1)
    normal = np.stack([-dx / length * 0.5 + 0.5, dy / length * 0.5 + 0.5, 1 / length * 0.5 + 0.5], -1)   # green up: OpenGL
    wide = blur(height, 8)
    occlusion = np.clip(1 - (wide - height) * 2.2, 0.35, 1)
    save(np.concatenate([colour, alpha[..., None]], -1), f"T_NH_Trim_{sheet}_BaseColor.png", "RGBA")
    save(normal, f"T_NH_Trim_{sheet}_Normal.png", "RGB")
    save(np.stack([occlusion, rough, metal], -1), f"T_NH_Trim_{sheet}_ORM.png", "RGB")
    return layout


# ---- decals: 512 x 512 each, colour with transparency ------------------------------------------------------------------------------

CELL = 512


def font(name, size):
    return ImageFont.truetype(FONTS + name, size)


def blank():
    return Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))


def weather(img, amount=0.35, cell=14):
    """Wear holes into a decal so it sits in the wall instead of on it"""
    n = noise(CELL, CELL, cell)
    a = np.asarray(img).astype(np.float32)
    a[..., 3] *= 1 - amount * step(n, 0.45, 0.62)
    return Image.fromarray(a.astype(np.uint8))


def text_block(lines, colour, sizes, face="Arial Black.ttf", top=40, gap=8, tilt=0.0, jitter=0):
    img = blank()
    draw = ImageDraw.Draw(img)
    y = top
    for line, size in zip(lines, sizes):
        f = font(face, size)
        box = draw.textbbox((0, 0), line, font=f)
        draw.text(((CELL - box[2]) / 2 + (rng.integers(-jitter, jitter + 1) if jitter else 0), y), line, font=f, fill=colour)
        y += box[3] + gap
    return img.rotate(tilt, Image.BICUBIC) if tilt else img


def poster(paper, ink, accent, lines, sizes, band=None):
    img = blank()
    draw = ImageDraw.Draw(img)
    draw.rectangle([70, 20, 442, 492], fill=paper)
    if band:
        draw.rectangle([70, 20, 442, 130], fill=accent)
    draw.ellipse([196, 250, 316, 370], outline=accent, width=10)          # where a face would be: a plain ring, nobody's portrait
    y = 34
    for i, (line, size) in enumerate(zip(lines, sizes)):
        f = font("Impact.ttf" if i == 0 else "Arial Bold.ttf", size)
        box = draw.textbbox((0, 0), line, font=f)
        draw.text(((CELL - box[2]) / 2, y), line, font=f, fill=paper if (band and i == 0) else ink)
        y += box[3] + 12
        if i == 0:
            y = 150
        if i == 1:
            y = 384
    a = np.asarray(img).astype(np.float32)
    a[..., :3] *= (0.86 + 0.14 * noise(CELL, CELL, 90))[..., None]       # sun-faded and creased
    return weather(Image.fromarray(a.astype(np.uint8)), 0.22, 30)


def torn_poster():
    img = blank()
    draw = ImageDraw.Draw(img)
    for colour, box in (((196, 188, 160, 255), [60, 40, 300, 330]), ((150, 60, 50, 255), [200, 150, 460, 470]), ((205, 200, 185, 255), [110, 260, 330, 480])):
        draw.rectangle(box, fill=colour)
    a = np.asarray(img).astype(np.float32)
    a[..., 3] *= step(noise(CELL, CELL, 40), 0.42, 0.47)                  # most of it has been ripped away
    a[..., :3] *= (0.7 + 0.3 * noise(CELL, CELL, 30))[..., None]
    return Image.fromarray(a.astype(np.uint8))


def stencil(lines, sizes, colour=(170, 20, 16, 255), tilt=0.0):
    return weather(text_block(lines, colour, sizes, "Arial Black.ttf", top=120 - 30 * (len(lines) - 2), tilt=tilt), 0.55, 9)


def scrawl(lines, sizes, colour, tilt):
    return weather(text_block(lines, colour, sizes, "Chalkduster.ttf", top=110, tilt=tilt, jitter=14), 0.3, 8)


def spray(lines, sizes, colour, tilt):
    img = text_block(lines, colour, sizes, "Brush Script.ttf", top=120, tilt=tilt)
    glow = img.filter(ImageFilter.GaussianBlur(5))                         # the overspray round the letters
    return weather(Image.alpha_composite(glow, img), 0.2, 30)


def from_mask(mask, colour, strength=1.0):
    a = np.zeros((CELL, CELL, 4), np.float32)
    a[..., :3] = np.asarray(colour, np.float32)
    a[..., 3] = np.clip(mask * strength, 0, 1) * 255
    return Image.fromarray(a.astype(np.uint8))


def edge_fade():
    yy, xx = np.mgrid[0:CELL, 0:CELL].astype(np.float32)
    return step(-np.hypot(xx - CELL / 2, yy - CELL / 2), -CELL * 0.5, -CELL * 0.3)


def drip_stain():
    yy, xx = np.mgrid[0:CELL, 0:CELL].astype(np.float32)
    columns = np.asarray(Image.fromarray(noise(2, CELL, 6, 2)).resize((CELL, CELL), Image.BICUBIC))
    reach = np.asarray(Image.fromarray(noise(2, CELL, 20, 1)).resize((CELL, CELL), Image.BICUBIC))
    mask = step(columns, 0.5, 0.75) * step(-yy / CELL, -(0.35 + 0.6 * reach), -(0.1 + 0.3 * reach)) * step(-np.abs(xx - CELL / 2), -CELL * 0.48, -CELL * 0.3)
    return from_mask(mask, (38, 36, 28), 0.85)


def damp_stain():
    yy, _ = np.mgrid[0:CELL, 0:CELL].astype(np.float32)
    mask = step(noise(CELL, CELL, 90) + yy / CELL * 0.5, 0.7, 0.95) * edge_fade()      # rising damp: heavier at the bottom
    img = np.asarray(from_mask(mask, (52, 60, 38), 0.8)).astype(np.float32)
    img[..., :3] *= (0.7 + 0.5 * noise(CELL, CELL, 12))[..., None]
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))


def crack(branches):
    img = Image.new("L", (CELL, CELL), 0)
    draw = ImageDraw.Draw(img)

    def walk(x, y, angle, length, width):
        for _ in range(length):
            angle += rng.normal(0, 0.35)
            nx, ny = x + math.cos(angle) * 12, y + math.sin(angle) * 12
            if not (30 < nx < CELL - 30 and 30 < ny < CELL - 30):
                return
            draw.line([x, y, nx, ny], fill=255, width=max(1, int(width)))
            x, y, width = nx, ny, width * 0.97
            if width > 1.5 and rng.random() < 0.06 * branches:
                walk(x, y, angle + rng.choice([-1, 1]) * rng.uniform(0.5, 1.1), length // 2, width * 0.6)

    for _ in range(branches):
        walk(CELL / 2 + rng.normal(0, 30), CELL / 2 + rng.normal(0, 30), rng.uniform(0, 2 * math.pi), 34, 5)
    mask = np.asarray(img.filter(ImageFilter.GaussianBlur(0.8))).astype(np.float32) / 255
    return from_mask(mask, (14, 13, 12), 1.0)


def oil_spill():
    yy, xx = np.mgrid[0:CELL, 0:CELL].astype(np.float32)
    blob = step(noise(CELL, CELL, 110) - np.hypot(xx - CELL / 2, yy - CELL / 2) / CELL * 1.5, -0.12, 0.02)
    img = np.asarray(from_mask(blob, (10, 9, 10), 0.9)).astype(np.float32)
    sheen = noise(CELL, CELL, 40)[..., None]                                # the rainbow film on old engine oil, kept faint
    img[..., :3] += sheen * np.float32([8, 7, 10]) * blob[..., None]
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))


def tyre_marks():
    yy, xx = np.mgrid[0:CELL, 0:CELL].astype(np.float32)
    bend = 40 * np.sin(yy / CELL * math.pi * 0.9)
    tread = 0.75 + 0.25 * np.sin(yy * 0.9)
    mask = sum(step(-np.abs(xx - bend - centre), -34, -20) for centre in (150, 330)) * tread
    mask = mask * step(noise(CELL, CELL, 26), 0.25, 0.6) * step(yy, 10, 110) * step(-yy, -500, -380)   # a skid starts hard and fades
    return from_mask(mask, (12, 12, 12), 0.8)


def build_decals():
    cream, red, blue, black, green = (232, 224, 196, 255), (176, 28, 24, 255), (28, 54, 140, 255), (24, 22, 22, 255), (18, 110, 60, 255)
    decals = [
        ("Poster_Revival", poster(cream, black, red, ["NIGHT OF FAVOUR", "5 DAYS REVIVAL", "OPEN GROUND · 6PM", "ALL ARE WELCOME"], [50, 34, 26, 24], band=True), "wall", 110),
        ("Poster_Owambe", poster((240, 210, 60, 255), black, blue, ["OWAMBE LIVE!", "DJ KPOMO ON DECK", "SATURDAY · GATE N500", "COME AND FLEX"], [58, 30, 26, 26], band=True), "wall", 110),
        ("Poster_Lesson", poster((226, 232, 236, 255), blue, green, ["HOME LESSON", "MATHS · ENGLISH", "CALL 0555 010 0142", "PRIMARY TO SS3"], [60, 32, 28, 26]), "wall", 100),
        ("Poster_Torn", torn_poster(), "wall", 120),
        ("Stencil_PostNoBill", stencil(["POST", "NO BILL"], [110, 90]), "wall", 120),
        ("Stencil_NotForSale", stencil(["THIS HOUSE", "IS NOT", "FOR SALE"], [64, 84, 84], tilt=-2), "wall", 180),
        ("Stencil_NoUrinate", stencil(["DO NOT", "URINATE HERE", "FINE N5000"], [80, 58, 62], colour=(20, 20, 24, 255), tilt=2), "wall", 160),
        ("Scrawl_Plumber", scrawl(["PLUMBER", "0555 010 0177", "CALL ANYTIME"], [56, 50, 40], (236, 236, 228, 255), 4), "wall", 130),
        ("Graffiti_Area", spray(["Area!"], [220], (236, 196, 30, 255), 8), "wall", 200),
        ("Graffiti_NoShaking", spray(["No", "Shaking"], [150, 130], (30, 120, 200, 255), -6), "wall", 200),
        ("Stain_Drips", drip_stain(), "wall", 250),
        ("Stain_Damp", damp_stain(), "wall", 250),
        ("Crack_A", crack(2), "any", 250),
        ("Crack_B", crack(4), "any", 300),
        ("OilSpill", oil_spill(), "ground", 220),
        ("TyreMarks", tyre_marks(), "ground", 600),
    ]
    atlas, layout = Image.new("RGBA", (W, W), (0, 0, 0, 0)), {}
    for i, (name, img, where, size) in enumerate(decals):
        col, row = i % 4, i // 4
        # keep a clear border in each cell, so a neighbour never bleeds in when the texture is shrunk
        a = np.asarray(img).astype(np.float32)
        border = np.ones((CELL, CELL), np.float32)
        border[:6], border[-6:], border[:, :6], border[:, -6:] = 0, 0, 0, 0
        a[..., 3] *= border
        atlas.paste(Image.fromarray(a.astype(np.uint8)), (col * CELL, row * CELL))
        layout[name] = {"column": col, "row": row, "on": where, "size_cm": size}
    # colour under the transparent parts is filled from the nearest paint, or the edges would darken when filtered
    a = np.asarray(atlas).astype(np.float32)
    rgb, alpha = Image.fromarray(a[..., :3].astype(np.uint8)), Image.fromarray(a[..., 3].astype(np.uint8))
    for _ in range(6):
        grown = rgb.filter(ImageFilter.MaxFilter(5))
        rgb = Image.composite(rgb, grown, alpha.point(lambda v: 255 if v > 8 else 0))
        alpha_grown = alpha.filter(ImageFilter.MaxFilter(5))
        alpha = ImageChops.lighter(alpha, alpha_grown.point(lambda v: 9 if v > 8 else 0))
    final = np.concatenate([np.asarray(rgb), a[..., 3:].astype(np.uint8)], -1)
    Image.fromarray(final).save(os.path.join(OUT, "T_NH_Decals.png"), optimize=True)
    return layout


def main():
    os.makedirs(OUT, exist_ok=True)
    data = {"format": "naija-hustle-trims", "version": 1, "size": W, "note": "v0 and v1 are a strip's top and bottom as a fraction of the sheet's height; strips repeat along U",
            "sheets": {sheet: build_sheet(sheet, strips) for sheet, strips in SHEETS.items()}, "decals": {"grid": 4, "cells": build_decals()}}
    for path in (os.path.join(OUT, "trims.json"), os.path.join(HERE, "..", "Data", "trim_sheets.json")):
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(data, fh, indent=1)
    total = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT) if f.endswith(".png"))
    print(f"{len(SHEETS)} trim sheets ({', '.join(f'{k}: {len(v)} strips' for k, v in SHEETS.items())}), {len(data['decals']['cells'])} decals, in {OUT}: {total / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
