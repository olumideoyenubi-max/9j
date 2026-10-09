"""Models the modular Lagos building kit from nothing and writes each piece as a glTF file.

  python3 build_kit.py [folder]        (default ~/Downloads/nh-kit)

Plain Python, no libraries, run outside Unreal. Scripts/import_kit.py then brings the folder into Unreal, and the
game's ANHKitBlock actor puts buildings together from the pieces.

The grid: a piece is 300 cm wide and one storey, 300 cm, tall. It is a facade panel, not a room: its origin is the
bottom left corner on the wall line, X runs along the wall, Y points out of the building, Z is up. Buildings are any
number of panels long (stretched a little to fit the wall) and any number of storeys high.

Every piece uses a handful of shared materials, named by slot:
  Wall            the painted wall (building master; the generator gives each building its colour)
  TrimFrames, TrimFramesCutout, TrimShop, TrimShopCutout     strips of the two trim sheets (Data/trim_sheets.json)
  Glass, Dark     window glass; the unlit inside of an open shop
  RoofZinc, RoofFlat, Concrete, Door, Tank, Appliance
so no piece has a texture of its own.
"""
import json
import math
import os
import struct
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.expanduser("~"), "Downloads", "nh-kit")
HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "..", "Data", "trim_sheets.json"), encoding="utf-8") as fh:
    TRIMS = json.load(fh)
STRIPS = {name: (sheet, s) for sheet, strips in TRIMS["sheets"].items() for name, s in strips.items()}
MODULE, STOREY = 300.0, 300.0


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def mul(a, k):
    return (a[0] * k, a[1] * k, a[2] * k)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit(a):
    n = math.sqrt(a[0] ** 2 + a[1] ** 2 + a[2] ** 2) or 1.0
    return (a[0] / n, a[1] / n, a[2] / n)


class Piece:
    """Triangles by material slot, in Unreal's axes and cm"""

    def __init__(self, name):
        self.name, self.slots = name, {}

    def quad(self, slot, origin, across, up, uv=(0.0, 0.0, 1.0, 1.0), normal=None):
        """A rectangle from `origin` along `across` and `up`. uv is (u at origin, v at the top, u at the far side, v at
        the bottom): pictures have v running down. The face looks along up x across (a wall drawn left to right faces out, +Y) unless `normal` says otherwise."""
        n = unit(normal if normal else cross(up, across))
        p = [origin, add(origin, across), add(add(origin, across), up), add(origin, up)]
        u0, v_top, u1, v_bottom = uv
        t = [(u0, v_bottom), (u1, v_bottom), (u1, v_top), (u0, v_top)]
        self.poly(slot, p, t, n)

    def poly(self, slot, points, uvs, normal):
        data = self.slots.setdefault(slot, {"p": [], "n": [], "t": [], "i": []})
        base = len(data["p"])
        data["p"] += points
        data["n"] += [normal] * len(points)
        data["t"] += uvs
        for k in range(1, len(points) - 1):
            data["i"] += [base, base + k, base + k + 1]

    def strip(self, name, origin, along, up, part=(0.0, 1.0), cutout=False, normal=None, u0=0.0):
        """A rectangle mapped onto a trim strip: `up` crosses the strip (or the `part` of it), `along` repeats it"""
        sheet, s = STRIPS[name]
        length, height = math.sqrt(sum(c * c for c in along)), math.sqrt(sum(c * c for c in up))
        v0 = s["v0"] + (s["v1"] - s["v0"]) * part[0]
        v1 = s["v0"] + (s["v1"] - s["v0"]) * part[1]
        u1 = u0 + length / (height / (v1 - v0))          # the sheet is square: keep the strip's proportions
        self.quad(f"Trim{sheet}{'Cutout' if cutout else ''}", origin, along, up, (u0, v0, u1, v1), normal)

    def box(self, slot, lo, hi, faces="xXyYzZ", tile=300.0):
        """An axis-aligned box. faces: x/X = the low/high X side and so on. UVs repeat every `tile` cm"""
        (x0, y0, z0), (x1, y1, z1) = lo, hi
        dx, dy, dz = x1 - x0, y1 - y0, z1 - z0
        if "y" in faces:
            self.quad(slot, (x1, y0, z0), (-dx, 0, 0), (0, 0, dz), (0, 0, dx / tile, dz / tile), (0, -1, 0))
        if "Y" in faces:
            self.quad(slot, (x0, y1, z0), (dx, 0, 0), (0, 0, dz), (0, 0, dx / tile, dz / tile), (0, 1, 0))
        if "x" in faces:
            self.quad(slot, (x0, y0, z0), (0, dy, 0), (0, 0, dz), (0, 0, dy / tile, dz / tile), (-1, 0, 0))
        if "X" in faces:
            self.quad(slot, (x1, y1, z0), (0, -dy, 0), (0, 0, dz), (0, 0, dy / tile, dz / tile), (1, 0, 0))
        if "z" in faces:
            self.quad(slot, (x0, y0, z0), (dx, 0, 0), (0, dy, 0), (0, 0, dx / tile, dy / tile), (0, 0, -1))
        if "Z" in faces:
            self.quad(slot, (x0, y0, z1), (dx, 0, 0), (0, dy, 0), (0, 0, dx / tile, dy / tile), (0, 0, 1))

    def triangles(self):
        return sum(len(d["i"]) for d in self.slots.values()) // 3

    def write(self, folder):
        """glTF is Y up, in metres: Unreal's (x, y, z) cm is glTF's (x, z, y) / 100. Triangles are turned to face their normal."""
        blob, views, accessors, primitives, materials = bytearray(), [], [], [], []

        def push(values, fmt, kind, target, bounds=False):
            start = len(blob)
            for v in values:
                blob.extend(struct.pack(fmt, *v) if isinstance(v, tuple) else struct.pack(fmt, v))
            while len(blob) % 4:
                blob.append(0)
            views.append({"buffer": 0, "byteOffset": start, "byteLength": len(blob) - start, "target": target})
            accessor = {"bufferView": len(views) - 1, "componentType": 5125 if fmt == "<I" else 5126, "count": len(values), "type": kind}
            if bounds:
                accessor["min"] = [min(v[k] for v in values) for k in range(3)]
                accessor["max"] = [max(v[k] for v in values) for k in range(3)]
            accessors.append(accessor)
            return len(accessors) - 1

        for slot, d in self.slots.items():
            pos = [(p[0] / 100.0, p[2] / 100.0, p[1] / 100.0) for p in d["p"]]
            nor = [(n[0], n[2], n[1]) for n in d["n"]]
            index = []
            for k in range(0, len(d["i"]), 3):
                a, b, c = d["i"][k:k + 3]
                e1, e2 = add(pos[b], mul(pos[a], -1)), add(pos[c], mul(pos[a], -1))
                facing = cross(e1, e2)
                turned = sum(facing[j] * nor[a][j] for j in range(3)) < 0
                index += [a, c, b] if turned else [a, b, c]
            materials.append({"name": slot, "pbrMetallicRoughness": {"baseColorFactor": [0.7, 0.7, 0.7, 1.0]}, "doubleSided": slot.endswith("Cutout")})
            primitives.append({"attributes": {"POSITION": push(pos, "<3f", "VEC3", 34962, True), "NORMAL": push(nor, "<3f", "VEC3", 34962),
                                              "TEXCOORD_0": push(d["t"], "<2f", "VEC2", 34962)},
                               "indices": push(index, "<I", "SCALAR", 34963), "material": len(materials) - 1})
        gltf = {"asset": {"version": "2.0", "generator": "NaijaHustle build_kit.py"}, "scene": 0, "scenes": [{"nodes": [0]}],
                "nodes": [{"mesh": 0, "name": self.name}], "meshes": [{"name": self.name, "primitives": primitives}], "materials": materials,
                "buffers": [{"byteLength": len(blob)}], "bufferViews": views, "accessors": accessors}
        text = json.dumps(gltf, separators=(",", ":")).encode()
        text += b" " * (-len(text) % 4)
        with open(os.path.join(folder, self.name + ".glb"), "wb") as fh:
            fh.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(text) + 8 + len(blob)))
            fh.write(struct.pack("<I4s", len(text), b"JSON") + text)
            fh.write(struct.pack("<I4s", len(blob), b"BIN\0") + bytes(blob))


# ---- the pieces ------------------------------------------------------------------------------------------------------------------

REVEAL = 14.0       # how far windows and doors sit back in the wall


def wall(p, opening=None, width=MODULE, height=STOREY):
    """The wall face of a panel, with a hole (x, z, wide, high) and the hole's sides"""
    if not opening:
        p.quad("Wall", (0, 0, 0), (width, 0, 0), (0, 0, height), (0, 0, width / MODULE, height / STOREY))
        return
    x, z, w, h = opening

    def face(x0, z0, x1, z1):
        if x1 > x0 and z1 > z0:
            p.quad("Wall", (x0, 0, z0), (x1 - x0, 0, 0), (0, 0, z1 - z0), (x0 / MODULE, 1 - z1 / STOREY, x1 / MODULE, 1 - z0 / STOREY))

    face(0, 0, x, height); face(x + w, 0, width, height); face(x, 0, x + w, z); face(x, z + h, x + w, height)
    k = REVEAL / MODULE
    p.quad("Wall", (x, -REVEAL, z), (0, REVEAL, 0), (0, 0, h), (0, 0, k, h / STOREY), (1, 0, 0))
    p.quad("Wall", (x + w, 0, z), (0, -REVEAL, 0), (0, 0, h), (0, 0, k, h / STOREY), (-1, 0, 0))
    p.quad("Wall", (x, -REVEAL, z + h), (w, 0, 0), (0, REVEAL, 0), (0, 0, w / MODULE, k), (0, 0, -1))
    if z > 0:
        p.quad("Wall", (x, 0, z), (w, 0, 0), (0, -REVEAL, 0), (0, 0, w / MODULE, k), (0, 0, 1))


def frame(p, strip, x, z, w, h, y=-REVEAL + 1.0, bar=6.0, bottom=True):
    """A frame round an opening, drawn just proud of whatever fills it"""
    p.strip(strip, (x, y, z + h - bar), (w, 0, 0), (0, 0, bar))
    if bottom:
        p.strip(strip, (x, y, z), (w, 0, 0), (0, 0, bar))
    p.strip(strip, (x + bar, y, z), (0, 0, h), (-bar, 0, 0), normal=(0, 1, 0))
    p.strip(strip, (x + w, y, z), (0, 0, h), (-bar, 0, 0), normal=(0, 1, 0))


def sill(p, x, z, w):
    """A concrete sill under a window, throwing rain clear of the wall"""
    p.strip("Ledge", (x - 8, 7, z - 9), (w + 16, 0, 0), (0, 0, 9))
    p.box("Wall", (x - 8, 0, z - 9), (x + w + 8, 7, z), "xXzZ")


WINDOW = (80.0, 95.0, 140.0, 125.0)


def wall_plain():
    p = Piece("Wall_Plain")
    wall(p)
    return p


def wall_window(name="Wall_Window", bars=False):
    p = Piece(name)
    x, z, w, h = WINDOW
    wall(p, WINDOW)
    p.quad("Glass", (x, -REVEAL, z), (w, 0, 0), (0, 0, h))
    frame(p, "FrameAluminium", x, z, w, h)
    p.strip("FrameAluminium", (x + w / 2 - 2.5, -REVEAL + 1.0, z), (0, 0, h), (5, 0, 0), normal=(0, 1, 0))    # a sliding window's middle rail
    sill(p, x, z, w)
    if bars:        # the strip is three times repeated up the window, so the bars come out the right thickness
        for k in range(3):
            p.strip("BurglarBars", (x, -3.0, z + h / 3 * k), (w, 0, 0), (0, 0, h / 3), cutout=True)
    return p


def wall_louvre():
    p = Piece("Wall_Louvre")
    x, z, w, h = 95.0, 95.0, 110.0, 125.0
    wall(p, (x, z, w, h))
    p.quad("Dark", (x, -REVEAL - 6, z), (w, 0, 0), (0, 0, h))
    for k in range(2):      # eight glass blades
        p.strip("Louvres", (x, -REVEAL, z + h / 2 * k), (w, 0, 0), (0, 0, h / 2), u0=0.0127)
    frame(p, "FrameWood", x, z, w, h)
    sill(p, x, z, w)
    for k in range(3):
        p.strip("BurglarBars", (x, -3.0, z + h / 3 * k), (w, 0, 0), (0, 0, h / 3), cutout=True)
    return p


def door_leaf(p, x, w, h, z=0.0):
    p.quad("Door", (x, -REVEAL, z), (w, 0, 0), (0, 0, h), (0, 0, w / 100.0, h / 220.0))
    frame(p, "DoorFrame", x, z, w, h, bar=7.0, bottom=False)


def wall_door():
    p = Piece("Wall_Door")
    wall(p, (100.0, 0.0, 100.0, 215.0))
    door_leaf(p, 100.0, 100.0, 215.0)
    p.box("Concrete", (85.0, 0.0, -30.0), (215.0, 45.0, 4.0), "xXYZ")      # the doorstep, reaching below ground for sloping sites
    return p


def wall_balcony():
    p = Piece("Wall_Balcony")
    wall(p, (100.0, 0.0, 100.0, 215.0))
    door_leaf(p, 100.0, 100.0, 215.0)
    deep, rail = 110.0, 95.0
    p.box("Concrete", (0.0, 0.0, -14.0), (MODULE, deep, 0.0), "xXYzZ")
    p.strip("Ledge", (0, deep + 0.5, -14.0), (MODULE, 0, 0), (0, 0, 14.0))
    # the railing: diamond bars on three sides, seen from both, under a concrete hand rail
    p.strip("BurglarBarsDiamond", (0, deep - 4, 0), (MODULE, 0, 0), (0, 0, rail), cutout=True)
    p.strip("BurglarBarsDiamond", (2, 0, 0), (0, deep - 4, 0), (0, 0, rail), cutout=True, normal=(-1, 0, 0))
    p.strip("BurglarBarsDiamond", (MODULE - 2, deep - 4, 0), (0, -deep + 4, 0), (0, 0, rail), cutout=True, normal=(1, 0, 0))
    p.box("Concrete", (0.0, deep - 9.0, rail), (MODULE, deep + 1.0, rail + 7.0), "xXyYzZ")
    return p


SHOP = (30.0, 0.0, 240.0, 250.0)


def shop_shutter():
    p = Piece("Shop_Shutter")
    x, z, w, h = SHOP
    wall(p, SHOP)
    # ten-centimetre slats: the clean upper part of the strip four times, then its rusted foot once
    for k in range(5):
        p.strip("RollerShutter", (x, -REVEAL, z + 12 + (h - 12) / 5 * k), (w, 0, 0), (0, 0, (h - 12) / 5), part=(0.375, 1.0) if k == 0 else (0.0, 0.625))
    p.strip("ShutterRail", (x, -REVEAL + 1.5, z), (w, 0, 0), (0, 0, 12.0))
    p.strip("SignFrame", (x - 10, 2.0, z + h + 6), (w + 20, 0, 0), (0, 0, 36.0))      # the board over the door, blank until signs are made
    p.box("Wall", (x - 10, 0, z + h + 6), (x + w + 10, 2.0, z + h + 42), "xXzZ")
    p.strip("Terrazzo", (0, 30.0, -30.0), (MODULE, 0, 0), (0, 0, 34.0))
    p.strip("Terrazzo", (0, 0, 4.0), (MODULE, 0, 0), (0, 30.0, 0), normal=(0, 0, 1))
    return p


def shop_open():
    p = Piece("Shop_Open")
    x, z, w, h = SHOP
    wall(p, SHOP)
    deep = 160.0
    p.quad("Dark", (x, -deep, z), (w, 0, 0), (0, 0, h))
    p.quad("Dark", (x, -deep, z), (0, deep - REVEAL, 0), (0, 0, h), normal=(1, 0, 0))
    p.quad("Dark", (x + w, -REVEAL, z), (0, -deep + REVEAL, 0), (0, 0, h), normal=(-1, 0, 0))
    p.quad("Dark", (x, -deep, z + h), (w, 0, 0), (0, deep - REVEAL, 0), normal=(0, 0, -1))
    p.strip("WallTiles", (x, -deep, z + 4.0), (w, 0, 0), (0, deep, 0), normal=(0, 0, 1))
    # the folding gate, drawn back to one side, and the rolled-up shutter's box above
    p.strip("FoldingGate", (x, -REVEAL + 2, z), (w * 0.3, 0, 0), (0, 0, h - 30), cutout=True)
    p.strip("RollerShutter", (x, -REVEAL, z + h - 30), (w, 0, 0), (0, 0, 30.0), part=(0.0, 0.375))
    p.strip("SignFrame", (x - 10, 2.0, z + h + 6), (w + 20, 0, 0), (0, 0, 36.0))
    p.box("Wall", (x - 10, 0, z + h + 6), (x + w + 10, 2.0, z + h + 42), "xXzZ")
    p.strip("Terrazzo", (0, 30.0, -30.0), (MODULE, 0, 0), (0, 0, 34.0))
    p.strip("Terrazzo", (0, 0, 4.0), (MODULE, 0, 0), (0, 30.0, 0), normal=(0, 0, 1))
    return p


def awning():
    p = Piece("Awning")
    out, top, drop = 170.0, 262.0, 45.0
    slope = math.hypot(out, drop)
    for normal in ((0, drop / slope, out / slope), (0, -drop / slope, -out / slope)):      # cloth: seen from above and below
        p.strip("Awning", (5, 0, top), (MODULE - 10, 0, 0), (0, out, -drop), cutout=True, normal=normal)
    p.strip("Awning", (5, out, top - drop - 18), (MODULE - 10, 0, 0), (0, 0, 18), part=(0.6, 1.0), cutout=True)      # the valance
    for x in (8.0, MODULE - 12.0):      # two props back to the wall
        p.box("Concrete", (x, 0.0, top - drop - 3), (x + 4, out, top - drop + 1), "xXz")
    return p


def corner():
    """A pilaster for the building's corners: stands on the corner point, one storey tall"""
    p = Piece("Corner")
    p.box("Wall", (-14.0, -14.0, 0.0), (14.0, 6.0, STOREY), "xXyY")
    return p


def parapet():
    p = Piece("Parapet")
    high, thick = 85.0, 18.0
    p.quad("Wall", (0, 0, 0), (MODULE, 0, 0), (0, 0, high), (0, 0, 1, high / STOREY))
    p.quad("Wall", (MODULE, -thick, 0), (-MODULE, 0, 0), (0, 0, high), (0, 0, 1, high / STOREY))
    p.strip("ParapetCap", (0, -thick - 3, high), (MODULE, 0, 0), (0, thick + 6, 0), normal=(0, 0, 1))
    p.strip("ParapetCap", (0, 3, high - 8), (MODULE, 0, 0), (0, 0, 8), part=(0.0, 0.35))
    return p


def roof_tri():
    """A flat roof is laid in right-angled triangles, so it fits a building of any shape: this is the one triangle,
    100 x 100 with its square corner at the origin, turned and stretched by the generator. Drawn on both sides,
    because half of them are laid mirrored."""
    p = Piece("Roof_Tri")
    for normal in ((0, 0, 1), (0, 0, -1)):
        p.poly("RoofFlat", [(0, 0, 0), (100, 0, 0), (0, 100, 0)], [(0, 0), (1, 0), (0, 1)], normal)
    return p


RISE = 150.0


def roof_gable():
    """A zinc roof module: 300 along the ridge, 100 across (stretched to the building's depth), ridge 150 up.
    The corrugations run down the slope, so stretching the span does not show."""
    p = Piece("Roof_Gable")
    for side in (-1, 1):
        n = unit((0, side * RISE, 50.0))
        p.quad("RoofZinc", (0, side * 50.0, 0) if side < 0 else (MODULE, 50.0, 0), (MODULE * -side, 0, 0), (0, -side * 50.0, RISE), (0, 0, 3, 2.5), n)
        p.quad("RoofZinc", (0, side * 50.0, -1.5) if side > 0 else (MODULE, -50.0, -1.5), (MODULE * side, 0, 0), (0, -side * 50.0, RISE), (0, 0, 3, 2.5), mul(n, -1))   # underside of the eaves
    p.strip("Gutter", (0, -4.0, RISE - 3), (MODULE, 0, 0), (0, 8.0, 0), normal=(0, 0, 1))      # the ridge cap
    return p


def roof_gable_end():
    """The wall triangle under the end of a gable roof: 100 across, facing -X"""
    p = Piece("Roof_GableEnd")
    for normal in ((-1, 0, 0), (1, 0, 0)):
        p.poly("Wall", [(0, -50.0, 0), (0, 50.0, 0), (0, 0, RISE)], [(0, 0.5), (1, 0.5), (0.5, 0)], normal)
    return p


def water_tank():
    """A ribbed plastic tank on a steel stand: on half the flat roofs in Lagos"""
    p = Piece("WaterTank")
    radius, low, high, sides = 62.0, 120.0, 260.0, 14
    ring = [(math.cos(2 * math.pi * k / sides), math.sin(2 * math.pi * k / sides)) for k in range(sides + 1)]
    bands = [(low, radius), (low + 45, radius * 1.04), (low + 90, radius), (high - 14, radius * 0.97), (high + 8, radius * 0.45), (high + 16, 16.0)]
    for (z0, r0), (z1, r1) in zip(bands, bands[1:]):
        for (c0, s0), (c1, s1) in zip(ring, ring[1:]):
            pts = [(c0 * r0, s0 * r0, z0), (c1 * r0, s1 * r0, z0), (c1 * r1, s1 * r1, z1), (c0 * r1, s0 * r1, z1)]
            p.poly("Tank", pts, [(0, 1), (1, 1), (1, 0), (0, 0)], unit(cross(add(pts[1], mul(pts[0], -1)), add(pts[3], mul(pts[0], -1)))))
    p.box("Appliance", (-66.0, -66.0, low - 8), (66.0, 66.0, low), "xXyYzZ")
    for x in (-60.0, 54.0):
        for y in (-60.0, 54.0):
            p.box("Appliance", (x, y, 0.0), (x + 6, y + 6, low - 8), "xXyY")
    return p


def ac_unit():
    """The outdoor half of a split air conditioner, on its bracket under a window"""
    p = Piece("AC_Unit")
    p.box("Appliance", (0.0, 4.0, 0.0), (82.0, 34.0, 56.0), "xXyYzZ")
    p.quad("Dark", (8.0, 34.4, 8.0), (44.0, 0, 0), (0, 0, 40.0))       # the fan grille
    p.box("Dark", (6.0, 0.0, -4.0), (10.0, 34.0, 0.0), "xXYz")
    p.box("Dark", (72.0, 0.0, -4.0), (76.0, 34.0, 0.0), "xXYz")
    return p


def stairs():
    """An outside stair up one storey: runs along the wall for 420 cm, 100 wide"""
    p = Piece("Stairs")
    steps, run, wide = 16, 420.0, 100.0
    rise, going = STOREY / steps, run / steps
    for k in range(steps):
        p.box("Concrete", (k * going, 0.0, 0.0 if k < 3 else k * rise - 16), (k * going + going, wide, (k + 1) * rise), "xXYzZ", tile=200.0)
    for k in range(4):      # the railing follows the flight in four leaning lengths
        p.strip("BurglarBars", (k * run / 4, wide - 3, (k + 0.5) * STOREY / 4 + 8), (run / 4, 0, STOREY / 4), (0, 0, 80.0), cutout=True, normal=(0, 1, 0))
    p.box("Concrete", (run, 0.0, STOREY - 16), (run + 110, wide, STOREY), "xXYzZ", tile=200.0)      # the landing
    return p


PIECES = [wall_plain, wall_window, lambda: wall_window("Wall_WindowBars", True), wall_louvre, wall_door, wall_balcony, shop_shutter, shop_open, awning,
          corner, parapet, roof_tri, roof_gable, roof_gable_end, water_tank, ac_unit, stairs]


def main():
    os.makedirs(OUT, exist_ok=True)
    kit = {}
    for make in PIECES:
        p = make()
        p.write(OUT)
        kit[p.name] = {"slots": list(p.slots), "triangles": p.triangles()}
    with open(os.path.join(OUT, "kit.json"), "w", encoding="utf-8") as fh:
        json.dump({"format": "naija-hustle-kit", "version": 1, "module_cm": MODULE, "storey_cm": STOREY, "pieces": kit}, fh, indent=1)
    size = sum(os.path.getsize(os.path.join(OUT, f)) for f in os.listdir(OUT) if f.endswith(".glb"))
    print(f"{len(kit)} pieces in {OUT}: {sum(k['triangles'] for k in kit.values())} triangles in all, {size / 1000:.0f} KB")
    print(", ".join(f"{name} {k['triangles']}" for name, k in kit.items()))


if __name__ == "__main__":
    main()
