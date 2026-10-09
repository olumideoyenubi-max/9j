"""Models the street clutter the scatter system lays along Lagos streets, each prop a glTF file.

  python3 build_props.py [folder]        (default ~/Downloads/nh-props)

Plain Python, no libraries, run outside Unreal; it uses the modelling helpers of build_kit.py. Then
NH_KIT_SET=props with Scripts/import_kit.py brings the folder into Unreal, and the game's ANHStreetScatter lays the
props along the streets round the player.

A prop stands on the ground at its origin, facing +Y where it has a front. Material slots: Plastic and Paint take a
colour from the scatter (a red chair, a blue drum); Weed, Dark, Concrete, Wood, Zinc and Appliance are fixed.
Nothing has a texture of its own, and everything is original: no real product is copied.
"""
import json
import math
import os
import random
import sys

from build_kit import Piece, add, cross, mul, unit

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.expanduser("~"), "Downloads", "nh-props")
rnd = random.Random(234)


def tube(p, slot, x, y, r0, r1, z0, z1, sides=12, cap=True):
    """An upright round or tapering post, with a lid"""
    ring = [(math.cos(2 * math.pi * k / sides), math.sin(2 * math.pi * k / sides)) for k in range(sides + 1)]
    for (c0, s0), (c1, s1) in zip(ring, ring[1:]):
        pts = [(x + c0 * r0, y + s0 * r0, z0), (x + c1 * r0, y + s1 * r0, z0), (x + c1 * r1, y + s1 * r1, z1), (x + c0 * r1, y + s0 * r1, z1)]
        p.poly(slot, pts, [(0, 1), (1, 1), (1, 0), (0, 0)], unit(((c0 + c1) / 2, (s0 + s1) / 2, (r0 - r1) / max(1.0, z1 - z0))))
    if cap:
        p.poly(slot, [(x + c * r1, y + s * r1, z1) for c, s in ring[:-1]], [(0.5 + c / 2, 0.5 + s / 2) for c, s in ring[:-1]], (0, 0, 1))


def both(p, slot, points):
    """A flat shape seen from both sides"""
    n = unit(cross(add(points[1], mul(points[0], -1)), add(points[2], mul(points[0], -1))))
    uv = [(0, 1), (1, 1), (0.5, 0)] if len(points) == 3 else [(0, 1), (1, 1), (1, 0), (0, 0)]
    p.poly(slot, points, uv, n)
    p.poly(slot, points, uv, mul(n, -1))


def chair():
    """A stacking plastic chair"""
    p = Piece("Chair")
    for x in (-21.0, 17.0):
        for y in (-20.0, 16.0):
            p.box("Plastic", (x, y, 0.0), (x + 4, y + 4, 42.0), "xXyY")
    p.box("Plastic", (-22.0, -21.0, 42.0), (22.0, 21.0, 45.0), "xXyYzZ")
    p.box("Plastic", (-22.0, -21.0, 45.0), (22.0, -18.0, 82.0), "xXyYZ")
    for x in (-22.0, 19.0):
        p.box("Plastic", (x, -18.0, 62.0), (x + 3, 18.0, 65.0), "xXYzZ")
        p.box("Plastic", (x, 15.0, 45.0), (x + 3, 18.0, 62.0), "xXyY")
    return p


def drum():
    """A 200-litre steel drum"""
    p = Piece("Drum")
    tube(p, "Paint", 0, 0, 29.0, 29.0, 0.0, 88.0, 14)
    for z in (28.0, 58.0):
        tube(p, "Paint", 0, 0, 30.5, 30.5, z, z + 3.0, 14, cap=False)
    tube(p, "Dark", -14.0, 0.0, 3.5, 3.5, 88.0, 90.0, 8)       # the bung
    return p


def generator():
    """A small petrol generator in its carrying frame"""
    p = Piece("Generator")
    p.box("Dark", (-29.0, -21.0, 4.0), (29.0, 21.0, 10.0), "xXyYzZ")
    p.box("Dark", (-26.0, -17.0, 10.0), (4.0, 17.0, 34.0), "xXyYZ")            # the engine
    p.box("Appliance", (4.0, -15.0, 10.0), (26.0, 15.0, 30.0), "xXyYZ")        # the alternator
    p.box("Paint", (-27.0, -19.0, 34.0), (27.0, 19.0, 47.0), "xXyYzZ")         # the tank
    tube(p, "Dark", -12.0, 0.0, 4.0, 4.0, 47.0, 50.0, 8)
    for x in (-30.0, 27.0):
        for y in (-22.0, 19.0):
            p.box("Paint", (x, y, 0.0), (x + 3, y + 3, 50.0), "xXyY")
    for y in (-22.0, 19.0):
        p.box("Paint", (-30.0, y, 50.0), (30.0, y + 3, 53.0), "xXyYzZ")
    return p


def kiosk():
    """A roadside kiosk: a painted box with a serving hatch and a zinc roof"""
    p = Piece("Kiosk")
    w, d, h = 100.0, 75.0, 205.0
    p.box("Paint", (-w, -d, 0.0), (w, d, h), "xXy")
    p.quad("Paint", (-w, d, 0.0), (2 * w, 0, 0), (0, 0, 92.0))                  # below the hatch
    p.quad("Paint", (-w, d, 175.0), (2 * w, 0, 0), (0, 0, h - 175.0))           # above it
    for x in (-w, w - 14.0):
        p.quad("Paint", (x, d, 92.0), (14.0, 0, 0), (0, 0, 83.0))
    p.quad("Dark", (-w + 14, d - 40.0, 92.0), (2 * w - 28, 0, 0), (0, 0, 83.0))     # the dark inside
    p.box("Wood", (-w - 4, d - 40.0, 88.0), (w + 4, d + 24.0, 93.0), "xXYzZ", tile=100.0)   # the counter
    p.box("Wood", (-w + 20, d - 38.0, 130.0), (w - 20, d - 20.0, 133.0), "YzZ", tile=100.0)   # a shelf of goods
    for k in range(7):                                                          # the goods: plain coloured packs
        x = -w + 26 + k * 24.0
        p.box("Plastic" if k % 2 else "Appliance", (x, d - 36.0, 133.0), (x + 16, d - 24.0, 133.0 + 14 + (k * 7) % 12), "xXYZ")
    for normal, z in (((0, 0.1, 1), 0.0), ((0, -0.1, -1), -2.0)):               # the roof, tipped forward, overhanging the hatch
        p.quad("Zinc", (-w - 15, -d - 10, h + 22 + z), (2 * w + 30, 0, 0), (0, 2 * d + 55, -22), (0, 0, 2.3, 2.0), normal)
    p.box("Paint", (-w, -d, h), (w, -d + 3, h + 21), "xXyY")
    return p


def umbrella_stall():
    """A trader's table under a big umbrella"""
    p = Piece("UmbrellaStall")
    p.box("Dark", (-2.0, -2.0, 0.0), (2.0, 2.0, 232.0), "xXyY")
    tube(p, "Concrete", 0, 0, 22.0, 18.0, 0.0, 14.0, 8)                          # the weight it stands in
    sides, rim, low, high = 8, 125.0, 196.0, 236.0
    for k in range(sides):
        a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
        both(p, "Plastic" if k % 2 else "Appliance", [(math.cos(a0) * rim, math.sin(a0) * rim, low), (math.cos(a1) * rim, math.sin(a1) * rim, low), (0, 0, high)])
    p.box("Wood", (30.0, -40.0, 70.0), (130.0, 40.0, 74.0), "xXyYzZ", tile=100.0)
    for x in (33.0, 123.0):
        for y in (-37.0, 33.0):
            p.box("Wood", (x, y, 0.0), (x + 4, y + 4, 70.0), "xXyY", tile=100.0)
    for k in range(5):                                                          # what is for sale: bowls and packs, plain colours
        x = 38.0 + k * 18.0
        p.box("Plastic" if k % 2 == 0 else "Appliance", (x, -20.0 + (k * 13) % 30, 74.0), (x + 13, -6.0 + (k * 13) % 30, 74.0 + 8 + (k * 5) % 9), "xXyYZ")
    return p


def pole():
    """A concrete power pole with a cross-arm. The wires hang from 800 cm up"""
    p = Piece("Pole")
    for (z0, r0), (z1, r1) in zip([(0, 13.0), (425, 11.0)], [(425, 11.0), (850, 9.0)]):
        p.poly("Concrete", [(-r0, -r0, z0), (r0, -r0, z0), (r1, -r1, z1), (-r1, -r1, z1)], [(0, 1), (1, 1), (1, 0), (0, 0)], (0, -1, 0))
        p.poly("Concrete", [(r0, r0, z0), (-r0, r0, z0), (-r1, r1, z1), (r1, r1, z1)], [(0, 1), (1, 1), (1, 0), (0, 0)], (0, 1, 0))
        p.poly("Concrete", [(-r0, r0, z0), (-r0, -r0, z0), (-r1, -r1, z1), (-r1, r1, z1)], [(0, 1), (1, 1), (1, 0), (0, 0)], (-1, 0, 0))
        p.poly("Concrete", [(r0, -r0, z0), (r0, r0, z0), (r1, r1, z1), (r1, -r1, z1)], [(0, 1), (1, 1), (1, 0), (0, 0)], (1, 0, 0))
    p.box("Wood", (-5.0, -85.0, 792.0), (5.0, 85.0, 802.0), "xXyYzZ", tile=100.0)
    for y in (-70.0, 0.0, 70.0):
        tube(p, "Appliance", 0.0, y, 4.0, 3.0, 802.0, 814.0, 6)                  # insulators
    p.box("Dark", (-16.0, -14.0, 560.0), (-9.0, 14.0, 600.0), "xXyYzZ")          # a junction box and its tangle
    return p


def wire():
    """One span of overhead lines, 1000 cm long (stretched to the next pole), sagging: three power lines at the
    cross-arm's height and a heavier bundle of phone lines below. Thin ribbons, each seen from all round."""
    p = Piece("Wire")
    steps = 10
    for y, drop, sag, thick in ((-70.0, 12.0, 38.0, 1.3), (0.0, 12.0, 46.0, 1.3), (70.0, 12.0, 34.0, 1.3), (-12.0, -200.0, 75.0, 3.0), (-9.0, -214.0, 95.0, 2.0)):
        line = [(1000.0 * k / steps, y, drop - sag * 4 * (k / steps) * (1 - k / steps)) for k in range(steps + 1)]
        for a, b in zip(line, line[1:]):
            both(p, "Dark", [(a[0], a[1], a[2] - thick), (b[0], b[1], b[2] - thick), (b[0], b[1], b[2] + thick), (a[0], a[1], a[2] + thick)])
            both(p, "Dark", [(a[0], a[1] - thick, a[2]), (b[0], b[1] - thick, b[2]), (b[0], b[1] + thick, b[2]), (a[0], a[1] + thick, a[2])])
    return p


def weeds():
    """A tuft of weeds, the kind that comes up at the edge of the tar and through cracks: blades, not a picture of them"""
    p = Piece("Weeds")
    for _ in range(16):
        x, y, turn = rnd.uniform(-14, 14), rnd.uniform(-14, 14), rnd.uniform(0, math.pi)
        high, lean, half = rnd.uniform(14, 42), rnd.uniform(-9, 9), rnd.uniform(1.2, 2.4)
        dx, dy = math.cos(turn) * half, math.sin(turn) * half
        both(p, "Weed", [(x - dx, y - dy, 0.0), (x + dx, y + dy, 0.0), (x + lean, y + lean * 0.6, high)])
    return p


def trash_bag():
    """A tied rubbish bag, slumped"""
    p = Piece("TrashBag")
    sides = 9
    rings = [(0.0, 20.0), (9.0, 27.0), (20.0, 24.0), (29.0, 13.0), (36.0, 5.0), (43.0, 7.0)]      # the last two are the knot
    lump = [[rnd.uniform(0.85, 1.15) for _ in range(sides)] for _ in rings]
    for r, ((z0, r0), (z1, r1)) in enumerate(zip(rings, rings[1:])):
        for k in range(sides):
            k1 = (k + 1) % sides
            a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * k1 / sides
            pts = [(math.cos(a0) * r0 * lump[r][k], math.sin(a0) * r0 * lump[r][k], z0), (math.cos(a1) * r0 * lump[r][k1], math.sin(a1) * r0 * lump[r][k1], z0),
                   (math.cos(a1) * r1 * lump[r + 1][k1], math.sin(a1) * r1 * lump[r + 1][k1], z1), (math.cos(a0) * r1 * lump[r + 1][k], math.sin(a0) * r1 * lump[r + 1][k], z1)]
            p.poly("Plastic", pts, [(0, 1), (1, 1), (1, 0), (0, 0)], unit((math.cos((a0 + a1) / 2), math.sin((a0 + a1) / 2), 0.35)))
    return p


def litter():
    """Scraps on the ground: empty water sachets, wrappers, a flattened carton"""
    p = Piece("Litter")
    for k in range(11):
        x, y, turn = rnd.uniform(-75, 75), rnd.uniform(-75, 75), rnd.uniform(0, 2 * math.pi)
        w, h = rnd.uniform(7, 13), rnd.uniform(9, 19)
        ax, ay = (math.cos(turn) * w, math.sin(turn) * w, 0.0), (-math.sin(turn) * h, math.cos(turn) * h, rnd.uniform(0, 2.5))
        p.quad("Appliance" if k % 3 else "Plastic", (x, y, 0.8 + k * 0.05), ax, ay, normal=(0, 0, 1))
    return p


def rubble():
    """A heap of broken blocks and concrete"""
    p = Piece("Rubble")
    for k in range(11):
        x, y = rnd.uniform(-60, 60), rnd.uniform(-45, 45)
        w, d, h = rnd.uniform(14, 44), rnd.uniform(12, 28), rnd.uniform(8, 24)
        lift = max(0.0, 22.0 - math.hypot(x, y) * 0.4) * rnd.uniform(0.3, 1.0)       # piled higher in the middle
        p.box("Concrete", (x - w / 2, y - d / 2, 0.0), (x + w / 2, y + d / 2, lift + h), "xXyYZ", tile=120.0)
    return p


PROPS = [chair, drum, generator, kiosk, umbrella_stall, pole, wire, weeds, trash_bag, litter, rubble]


def main():
    os.makedirs(OUT, exist_ok=True)
    kit = {}
    for make in PROPS:
        p = make()
        p.write(OUT)
        kit[p.name] = {"slots": list(p.slots), "triangles": p.triangles()}
    with open(os.path.join(OUT, "kit.json"), "w", encoding="utf-8") as fh:
        json.dump({"format": "naija-hustle-props", "version": 1, "pieces": kit}, fh, indent=1)
    size = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT) if f.endswith(".glb"))
    print(f"{len(kit)} props in {OUT}: {sum(k['triangles'] for k in kit.values())} triangles in all, {size / 1000:.0f} KB")
    print(", ".join(f"{name} {k['triangles']}" for name, k in kit.items()))


if __name__ == "__main__":
    main()
