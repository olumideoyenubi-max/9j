"""Makes the two rooms NHEstate puts inside a home, in Blender: a modern loft (flats) and a classic drawing room (houses).

  blender -b --factory-startup --python build_estate_interiors.py -- <out folder>   (default <repo>/project-files/downloads/nh-estate)

Writes SM_Home_Loft.fbx and SM_Home_Classic.fbx and a picture of each. Bring them in with import_estate_homes.py.

Each is one room 18 m by 12 and 3.8 m high with its floor, walls, ceiling and furniture in one mesh; the origin is the
middle of the floor. In Unreal the front door is in the middle of the +Y wall and the windows fill the -Y wall (Blender's
Y is turned over on the way), which is what ANHEstate::BuildHome expects; it adds the lights.

  Loft      plaster walls, a boarded ceiling on dark beams, a polished floor; a wall of steel-framed window; two niches
            with plants either side of the television; a deep linen sofa, an armchair, a slab coffee table and an
            ottoman on a rug under two woven pendant lamps; a kitchen with an island; a bed behind a steel-framed screen
  Classic   strip-wood floor, panelled dado, a coffered dark ceiling with two chandeliers; a wall of bookcases; a brick
            fireplace; three arched windows over radiators; two carved sofas and a round table on a bordered carpet; a
            dining table for six on another; paintings in gilt frames; a four-poster bed behind a folding screen
Both are original rooms in those two styles: boxes, cylinders and a few spheres with flat-coloured materials.
"""
import bpy, bmesh, math, os, random, sys
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv and len(sys.argv) > sys.argv.index("--") + 1 else os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "..", "project-files", "downloads", "nh-estate"))
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)

MATS = {  # name: colour, roughness, metallic, emission
    "Home_Plaster": ((0.62, 0.55, 0.46), 0.9, 0, 0), "Home_White": ((0.84, 0.83, 0.8), 0.85, 0, 0), "Home_Concrete": ((0.09, 0.085, 0.08), 0.12, 0, 0),
    "Home_Oak": ((0.42, 0.26, 0.13), 0.5, 0, 0), "Home_DarkWood": ((0.06, 0.035, 0.025), 0.4, 0, 0), "Home_Walnut": ((0.2, 0.1, 0.05), 0.45, 0, 0),
    "Home_Steel": ((0.03, 0.03, 0.035), 0.45, 0.8, 0), "Home_Linen": ((0.55, 0.5, 0.42), 0.95, 0, 0), "Home_Charcoal": ((0.07, 0.07, 0.08), 0.9, 0, 0),
    "Home_Grey": ((0.3, 0.31, 0.32), 0.9, 0, 0), "Home_Rug": ((0.66, 0.56, 0.4), 0.98, 0, 0), "Home_RugBlue": ((0.2, 0.26, 0.36), 0.98, 0, 0),
    "Home_Cream": ((0.8, 0.76, 0.66), 0.95, 0, 0), "Home_Stone": ((0.13, 0.12, 0.11), 0.7, 0, 0), "Home_Marble": ((0.78, 0.77, 0.74), 0.15, 0, 0),
    "Home_Brick": ((0.36, 0.13, 0.09), 0.9, 0, 0), "Home_Mortar": ((0.5, 0.45, 0.4), 0.95, 0, 0), "Home_Brass": ((0.55, 0.4, 0.14), 0.3, 1, 0),
    "Home_Gilt": ((0.6, 0.45, 0.12), 0.35, 1, 0), "Home_Leaf": ((0.08, 0.26, 0.07), 0.8, 0, 0), "Home_LeafPale": ((0.3, 0.42, 0.12), 0.8, 0, 0),
    "Home_Clay": ((0.45, 0.3, 0.2), 0.8, 0, 0), "Home_Rattan": ((0.85, 0.68, 0.42), 0.7, 0, 2.5), "Home_Lamp": ((1.0, 0.82, 0.55), 0.4, 0, 9.0),
    "Home_Sky": ((0.55, 0.75, 1.0), 0.3, 0, 5.0), "Home_Screen": ((0.15, 0.4, 0.8), 0.2, 0, 3.0), "Home_Fire": ((1.0, 0.4, 0.08), 0.5, 0, 8.0),
    "Home_Iron": ((0.75, 0.75, 0.72), 0.5, 0.6, 0), "Home_Art1": ((0.2, 0.33, 0.3), 0.7, 0, 0), "Home_Art2": ((0.5, 0.36, 0.16), 0.7, 0, 0),
    "Home_Book1": ((0.3, 0.07, 0.06), 0.8, 0, 0), "Home_Book2": ((0.08, 0.16, 0.1), 0.8, 0, 0), "Home_Book3": ((0.45, 0.36, 0.2), 0.8, 0, 0)}
ORDER = list(MATS)


def material(name):
    colour, rough, metal, glow = MATS[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*colour, 1)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if glow:
        bsdf.inputs["Emission Color"].default_value = (*colour, 1)
        bsdf.inputs["Emission Strength"].default_value = glow
    mat.diffuse_color = (*colour, 1)
    return mat


MATERIALS = {n: material(n) for n in MATS}
L, W, H = 18.0, 12.0, 3.8          # the room: X along it, the door wall at y = -W/2, the window wall at y = +W/2


class Room:
    def __init__(self, name):
        self.name, self.bm = name, bmesh.new()

    def _paint(self, made, mat):
        for f in {f for v in made["verts"] for f in v.link_faces}:
            f.material_index = ORDER.index(mat)

    def box(self, mat, centre, size, yaw=0.0, tilt=0.0):
        m = Matrix.Translation(centre) @ Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Rotation(math.radians(tilt), 4, "X") @ Matrix.Diagonal((*size, 1))
        self._paint(bmesh.ops.create_cube(self.bm, size=1.0, matrix=m), mat)

    def tube(self, mat, centre, radius, height, sides=14, top=None):
        self._paint(bmesh.ops.create_cone(self.bm, cap_ends=True, segments=sides, radius1=radius, radius2=radius if top is None else top, depth=height, matrix=Matrix.Translation(centre)), mat)

    def ball(self, mat, centre, radius, squash=1.0):
        self._paint(bmesh.ops.create_icosphere(self.bm, subdivisions=2, radius=radius, matrix=Matrix.Translation(centre) @ Matrix.Diagonal((1, 1, squash, 1))), mat)

    def shell(self, floor, wall, ceiling):
        self.box(floor, (0, 0, -0.1), (L, W, 0.2))
        self.box(ceiling, (0, 0, H + 0.1), (L, W, 0.2))
        for x in (-L / 2 - 0.1, L / 2 + 0.1):
            self.box(wall, (x, 0, H / 2), (0.2, W, H))
        for x in (-(L / 2 + 0.8) / 2 - 0.4 + 0.4, (L / 2 + 0.8) / 2):       # the door wall, with the doorway left in it
            pass
        self.box(wall, (-(L / 4 + 0.35), -W / 2 - 0.1, H / 2), (L / 2 - 0.7, 0.2, H))
        self.box(wall, ((L / 4 + 0.35), -W / 2 - 0.1, H / 2), (L / 2 - 0.7, 0.2, H))
        self.box(wall, (0, -W / 2 - 0.1, (H + 2.4) / 2), (1.4, 0.2, H - 2.4))

    def door(self, wood, metal):
        self.box(wood, (0, -W / 2 - 0.02, 1.2), (1.4, 0.08, 2.4))
        self.box(metal, (0.5, -W / 2 + 0.05, 1.1), (0.04, 0.06, 0.22))
        for x in (-0.76, 0.76):
            self.box(wood, (x, -W / 2 + 0.03, 1.25), (0.12, 0.1, 2.5))
        self.box(wood, (0, -W / 2 + 0.03, 2.47), (1.64, 0.1, 0.12))

    def plant(self, at, height=1.6, kind="tree"):
        x, y = at
        self.tube("Home_Clay", (x, y, 0.25), 0.3, 0.5, top=0.36)
        if kind == "tree":
            self.tube("Home_Walnut", (x, y, 0.5 + height / 2), 0.035, height, sides=6)
            rnd = random.Random(int(x * 10 + y * 7))
            for _ in range(11):
                a, r = rnd.uniform(0, math.tau), rnd.uniform(0.1, 0.55)
                self.ball("Home_LeafPale" if rnd.random() < 0.5 else "Home_Leaf", (x + math.cos(a) * r, y + math.sin(a) * r, 0.5 + height * rnd.uniform(0.6, 1.05)), rnd.uniform(0.18, 0.32), 0.7)
        else:
            for k in range(9):
                a = math.radians(k * 40)
                self.box("Home_Leaf", (x + math.cos(a) * 0.3, y + math.sin(a) * 0.3, 0.5 + height * 0.45), (0.12, 0.03, height), yaw=math.degrees(a) + 90, tilt=28)

    def sofa(self, at, width, yaw, body, cushion, legs=None, back=0.85):
        cx, cy = at
        m = Matrix.Rotation(math.radians(yaw), 3, "Z")
        put = lambda mat, off, size: self.box(mat, Vector((cx, cy, 0)) + m @ Vector(off), size, yaw=yaw)
        put(body, (0, 0, 0.22 + (0.12 if legs else 0)), (width, 1.0, 0.44))
        put(body, (0, 0.42, back / 2 + (0.12 if legs else 0)), (width, 0.2, back))
        for s in (-1, 1):
            put(body, (s * (width / 2 - 0.1), 0, 0.33 + (0.12 if legs else 0)), (0.2, 1.0, 0.66))
        seats = max(2, round(width / 0.9))
        for k in range(seats):
            u = -width / 2 + 0.2 + (k + 0.5) * (width - 0.4) / seats
            put(cushion, (u, -0.06, 0.5 + (0.12 if legs else 0)), ((width - 0.4) / seats - 0.04, 0.78, 0.14))
            put(cushion, (u, 0.27, 0.72 + (0.12 if legs else 0)), ((width - 0.4) / seats - 0.08, 0.16, 0.4))
        if legs:
            for sx in (-1, 1):
                for sy in (-1, 1):
                    put(legs, (sx * (width / 2 - 0.08), sy * 0.42, 0.07), (0.07, 0.07, 0.14))
            put(legs, (0, 0.5, back + 0.2), (width * 0.9, 0.06, 0.16))          # a carved top rail
            put(legs, (0, 0.5, back + 0.32), (width * 0.4, 0.06, 0.1))

    def finish(self):
        mesh = bpy.data.meshes.new(self.name)
        self.bm.normal_update()
        self.bm.to_mesh(mesh)
        self.bm.free()
        for n in ORDER:
            mesh.materials.append(MATERIALS[n])
        obj = bpy.data.objects.new(self.name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def loft():
    r = Room("SM_Home_Loft")
    r.shell("Home_Concrete", "Home_Plaster", "Home_Oak")
    r.door("Home_Walnut", "Home_Steel")
    for k in range(17):                                                           # the boards of the ceiling, and the beams under them
        r.box("Home_DarkWood", (-8.5 + k * 1.0625 + 0.5, 0, H - 0.005), (0.03, W, 0.02))
    for x in (-7.5, -4.5, -1.5, 1.5, 4.5, 7.5):
        r.box("Home_DarkWood", (x, 0, H - 0.17), (0.24, W, 0.34))
    # the window wall: sky behind a steel grid
    r.box("Home_Plaster", (0, W / 2 + 0.1, 0.2), (L, 0.2, 0.4))
    r.box("Home_Sky", (0, W / 2 + 0.25, H / 2 + 0.2), (L, 0.05, H - 0.4))
    for k in range(13):
        r.box("Home_Steel", (-9 + k * 1.5, W / 2, H / 2 + 0.2), (0.07, 0.1, H - 0.4))
    for z in (0.42, 1.5, 2.6, H - 0.04):
        r.box("Home_Steel", (0, W / 2, z), (L, 0.1, 0.07))
    # the end wall: the television between two niches lined in dark stone, one square and one arched
    for y, arched in ((-3.4, False), (3.4, True)):
        r.box("Home_Stone", (-L / 2 + 0.03, y, 1.9), (0.06, 1.8, 2.2))
        r.box("Home_Plaster", (-L / 2 + 0.16, y - 1.05, 1.9), (0.32, 0.3, 2.5))
        r.box("Home_Plaster", (-L / 2 + 0.16, y + 1.05, 1.9), (0.32, 0.3, 2.5))
        r.box("Home_Plaster", (-L / 2 + 0.16, y, 0.4), (0.32, 2.4, 0.8))
        r.box("Home_Oak", (-L / 2 + 0.2, y, 0.82), (0.4, 1.8, 0.05))
        if arched:
            for k in range(9):                                                    # the arch, in steps
                a = math.radians(k * 180 / 8)
                r.box("Home_Plaster", (-L / 2 + 0.16, y + math.cos(a) * 1.02, 2.75 + math.sin(a) * 0.5), (0.32, 0.36, 0.5), tilt=0)
            r.box("Home_Plaster", (-L / 2 + 0.16, y, 3.45), (0.32, 2.4, 0.7))
        else:
            r.box("Home_Plaster", (-L / 2 + 0.16, y, 3.4), (0.32, 2.4, 0.8))
        r.plant((-L / 2 + 0.25, y - 0.3), 0.9, "fern")
        for k, mat in enumerate(("Home_Clay", "Home_Cream", "Home_Charcoal")):
            r.tube(mat, (-L / 2 + 0.22, y + 0.3 + k * 0.22, 0.85 + 0.1 + k * 0.03), 0.07, 0.2 + k * 0.06, sides=10)
    r.box("Home_Walnut", (-L / 2 + 0.3, 0, 0.3), (0.5, 2.6, 0.5))
    r.box("Home_Steel", (-L / 2 + 0.08, 0, 1.6), (0.06, 2.2, 1.25))
    r.box("Home_Screen", (-L / 2 + 0.115, 0, 1.6), (0.01, 2.1, 1.15))
    for y in (-1.6, 1.6):                                                         # wall lights either side of it
        r.box("Home_Steel", (-L / 2 + 0.06, y, 2.3), (0.1, 0.16, 0.3))
        r.box("Home_Lamp", (-L / 2 + 0.07, y, 2.47), (0.08, 0.12, 0.03))
    # sitting: the rug, a deep sofa and an armchair, a slab table, an ottoman, a tree, two pendants
    r.box("Home_Rug", (-5.3, 0.2, 0.012), (5.4, 4.4, 0.024))
    r.sofa((-4.6, 0.2), 3.3, 90, "Home_Linen", "Home_Cream")
    r.sofa((-5.6, 2.9), 1.3, 0, "Home_Charcoal", "Home_Grey")
    r.box("Home_Oak", (-6.3, 0.2, 0.36), (1.1, 2.0, 0.14))
    for y in (-0.5, 0.9):
        r.box("Home_Walnut", (-6.3, y, 0.15), (0.9, 0.35, 0.3))
    for k, mat in enumerate(("Home_Charcoal", "Home_Clay", "Home_Stone")):
        r.tube(mat, (-6.3 + k * 0.12, 0.5 + k * 0.25, 0.5 + k * 0.02), 0.06 + k * 0.015, 0.14 + k * 0.04, sides=10)
    r.box("Home_Charcoal", (-5.7, -2.2, 0.22), (1.2, 1.2, 0.44))
    r.plant((-3.0, 3.6), 2.0, "tree")
    r.plant((-8.3, -5.2), 1.3, "fern")
    for x, y, z in ((-6.1, 0.0, 2.6), (-5.2, 0.7, 2.9)):
        r.tube("Home_Steel", (x, y, (H + z) / 2 + 0.2), 0.008, H - z - 0.4, sides=5)
        r.tube("Home_Rattan", (x, y, z), 0.3, 0.55, sides=16)
        r.ball("Home_Lamp", (x, y, z), 0.07)
    # eating and cooking: a table for six, a kitchen along the door wall with an island
    r.box("Home_Oak", (2.2, 1.6, 0.74), (2.6, 1.1, 0.07))
    for x in (1.2, 3.2):
        r.box("Home_Steel", (x, 1.6, 0.36), (0.08, 0.9, 0.72))
    for k in range(6):
        cx, cy = 1.4 + (k % 3) * 0.8, 0.75 if k < 3 else 2.45
        r.box("Home_Linen", (cx, cy, 0.24), (0.48, 0.48, 0.48))
        r.box("Home_Linen", (cx, cy + (-0.2 if k < 3 else 0.2), 0.62), (0.48, 0.08, 0.5))
    r.tube("Home_Steel", (2.2, 1.6, (H + 2.0) / 2), 0.008, H - 2.0, sides=5)
    r.box("Home_Rattan", (2.2, 1.6, 1.95), (1.6, 0.3, 0.22))
    r.box("Home_Charcoal", (2.6, -W / 2 + 0.35, 0.45), (5.6, 0.7, 0.9))
    r.box("Home_Marble", (2.6, -W / 2 + 0.36, 0.92), (5.7, 0.74, 0.05))
    r.box("Home_Charcoal", (2.6, -W / 2 + 0.2, 2.5), (5.6, 0.4, 0.8))
    r.box("Home_Oak", (2.6, -W / 2 + 0.16, 1.7), (5.6, 0.3, 0.04))
    r.box("Home_Iron", (5.9, -W / 2 + 0.4, 1.05), (0.9, 0.8, 2.1))
    r.box("Home_Steel", (1.2, -W / 2 + 0.36, 0.955), (0.8, 0.5, 0.02))
    r.box("Home_Charcoal", (2.6, -3.4, 0.45), (3.2, 1.0, 0.9))
    r.box("Home_Marble", (2.6, -3.4, 0.92), (3.4, 1.1, 0.05))
    for x in (1.6, 2.6, 3.6):
        r.tube("Home_Steel", (x, -2.65, 0.36), 0.03, 0.72, sides=8)
        r.tube("Home_Oak", (x, -2.65, 0.74), 0.19, 0.05, sides=14)
    # sleeping: behind a steel-framed screen at the far end, a low bed on a platform
    for k in range(7):
        r.box("Home_Steel", (5.4, -1.0 + k * 1.15, H / 2), (0.06, 0.06, H))
    for z in (0.03, 1.2, 2.4, H - 0.03):
        r.box("Home_Steel", (5.4, 2.45, z), (0.06, 6.9, 0.06))
    r.box("Home_Walnut", (7.4, 2.6, 0.15), (2.6, 2.6, 0.3))
    r.box("Home_Cream", (7.5, 2.6, 0.42), (2.2, 2.0, 0.26))
    r.box("Home_Linen", (7.2, 2.6, 0.57), (1.4, 2.02, 0.06))
    for y in (2.1, 3.1):
        r.box("Home_White", (8.3, y, 0.62), (0.42, 0.7, 0.14))
    r.box("Home_Walnut", (8.75, 2.6, 0.9), (0.1, 3.0, 1.8))
    for y in (0.95, 4.25):
        r.box("Home_Walnut", (8.4, y, 0.25), (0.5, 0.5, 0.5))
        r.tube("Home_Lamp", (8.4, y, 0.68), 0.11, 0.26, sides=10)
    r.box("Home_Charcoal", (7.0, -4.3, 1.2), (3.2, 0.7, 2.4))
    return r.finish()


def classic():
    r = Room("SM_Home_Classic")
    r.shell("Home_Oak", "Home_White", "Home_DarkWood")
    r.door("Home_DarkWood", "Home_Brass")
    for k in range(40):                                                           # the floor in strips of two woods
        if k % 2:
            r.box("Home_Walnut", (0, -W / 2 + (k + 0.5) * W / 40, 0.004), (L, W / 40 - 0.01, 0.008))
    # the coffered ceiling: a grid of beams with a boss where they cross
    for x in [-9 + k * 1.5 for k in range(13)]:
        r.box("Home_Walnut", (x, 0, H - 0.12), (0.16, W, 0.24))
    for y in [-6 + k * 1.5 for k in range(9)]:
        r.box("Home_Walnut", (0, y, H - 0.12), (L, 0.16, 0.24))
    for x in [-8.25 + k * 1.5 for k in range(12)]:
        for y in [-5.25 + k * 1.5 for k in range(8)]:
            r.box("Home_Walnut", (x, y, H - 0.03), (0.7, 0.7, 0.06), yaw=45)
    # the dado round the room, and the cornice
    for y in (-W / 2 + 0.03,):
        for x in (-(L / 4 + 0.35), (L / 4 + 0.35)):
            r.box("Home_DarkWood", (x, y, 0.5), (L / 2 - 0.7, 0.06, 1.0))
    r.box("Home_DarkWood", (L / 2 - 0.03, 0, 0.5), (0.06, W, 1.0))
    for y in (-W / 2 + 0.06, W / 2 - 0.06):
        r.box("Home_DarkWood", (0, y, H - 0.3), (L, 0.12, 0.12))
    # the window wall: three arched windows with the day in them, a radiator under each, dado between
    r.box("Home_White", (0, W / 2 + 0.1, H / 2), (L, 0.2, H))
    r.box("Home_DarkWood", (0, W / 2 - 0.03, 0.5), (L, 0.06, 1.0))
    for x in (-5.0, 0.0, 5.0):
        r.box("Home_Sky", (x, W / 2 - 0.02, 1.9), (1.6, 0.06, 1.6))
        for k in range(9):
            a = math.radians(k * 180 / 8)
            r.box("Home_Sky", (x + math.cos(a) * 0.4, W / 2 - 0.02, 2.7 + math.sin(a) * 0.38), (0.82, 0.06, 0.2))
            r.box("Home_DarkWood", (x + math.cos(a) * 0.86, W / 2 - 0.05, 2.7 + math.sin(a) * 0.62), (0.1, 0.1, 0.3), tilt=0)
        for dx in (-0.85, 0.0, 0.85):
            r.box("Home_DarkWood", (x + dx, W / 2 - 0.06, 1.9), (0.08 if dx else 0.05, 0.1, 1.7))
        for z in (1.08, 1.9, 2.7):
            r.box("Home_DarkWood", (x, W / 2 - 0.06, z), (1.7, 0.1, 0.06))
        for k in range(10):                                                       # the radiator
            r.box("Home_Iron", (x - 0.54 + k * 0.12, W / 2 - 0.2, 0.5), (0.08, 0.14, 0.7))
    # the bookcase wall
    r.box("Home_DarkWood", (-L / 2 + 0.3, 0, 1.6), (0.6, 9.6, 3.2))
    r.box("Home_DarkWood", (-L / 2 + 0.38, 0, 0.5), (0.76, 9.8, 1.0))
    rnd = random.Random(7)
    for bay in range(6):
        y0 = -4.6 + bay * 1.56
        r.box("Home_Walnut", (-L / 2 + 0.62, y0 + 0.7, 2.2), (0.04, 1.36, 2.0))
        for shelf in range(4):
            z = 1.26 + shelf * 0.5
            r.box("Home_DarkWood", (-L / 2 + 0.64, y0 + 0.7, z - 0.03), (0.1, 1.36, 0.04))
            y = y0 + 0.06
            while y < y0 + 1.3:
                w = rnd.uniform(0.04, 0.09)
                r.box(rnd.choice(("Home_Book1", "Home_Book2", "Home_Book3", "Home_Walnut")), (-L / 2 + 0.68, y + w / 2, z + rnd.uniform(0.14, 0.2)), (0.14, w - 0.008, rnd.uniform(0.28, 0.4)))
                y += w
        r.box("Home_DarkWood", (-L / 2 + 0.64, y0 + 0.7, 3.32), (0.16, 1.0, 0.2))             # a pediment over each bay
        r.box("Home_DarkWood", (-L / 2 + 0.64, y0 + 0.7, 3.47), (0.16, 0.5, 0.12))
        r.box("Home_Brass", (-L / 2 + 0.78, y0 + 0.7, 0.55), (0.03, 0.05, 0.05))
    # the fireplace at the other end: a brick breast with courses showing, the fire, a mantel, a hearth
    r.box("Home_Brick", (L / 2 - 0.45, -1.5, H / 2), (0.9, 3.0, H))
    for k in range(19):
        r.box("Home_Mortar", (L / 2 - 0.905, -1.5, 0.2 * k + 0.1), (0.01, 3.0, 0.02))
    r.box("Home_Stone", (L / 2 - 0.86, -1.5, 0.6), (0.2, 1.4, 1.0))
    r.box("Home_Fire", (L / 2 - 0.9, -1.5, 0.32), (0.14, 0.9, 0.3))
    for k in range(3):
        r.tube("Home_Walnut", (L / 2 - 0.95, -1.8 + k * 0.3, 0.16), 0.07, 0.6, sides=8)
    r.box("Home_DarkWood", (L / 2 - 1.0, -1.5, 1.2), (0.4, 2.4, 0.1))
    r.box("Home_Marble", (L / 2 - 1.25, -1.5, 0.03), (0.9, 2.6, 0.06))
    for y, mat in ((-2.3, "Home_Brass"), (-0.7, "Home_Brass")):
        r.tube(mat, (L / 2 - 1.0, y, 1.4), 0.04, 0.3, sides=8)
        r.ball("Home_Lamp", (L / 2 - 1.0, y, 1.6), 0.05)
    r.box("Home_Gilt", (L / 2 - 0.92, -1.5, 2.3), (0.06, 1.5, 1.1))
    r.box("Home_Art1", (L / 2 - 0.955, -1.5, 2.3), (0.02, 1.3, 0.9))
    # paintings, and wall lights between them
    for x, mat in ((-3.0, "Home_Art2"), (3.2, "Home_Art1")):
        r.box("Home_Gilt", (x, -W / 2 + 0.05, 2.2), (1.5, 0.06, 1.1))
        r.box(mat, (x, -W / 2 + 0.085, 2.2), (1.3, 0.02, 0.9))
    for x in (-5.2, -1.2, 1.2, 5.6):
        r.box("Home_Brass", (x, -W / 2 + 0.08, 2.1), (0.06, 0.12, 0.2))
        r.ball("Home_Lamp", (x, -W / 2 + 0.16, 2.28), 0.07)
    # two chandeliers
    for x in (-3.5, 3.0):
        r.tube("Home_Brass", (x, 0.5, H - 0.55), 0.02, 0.9, sides=6)
        r.ball("Home_Brass", (x, 0.5, H - 1.05), 0.12)
        for k in range(6):
            a = math.radians(k * 60)
            r.box("Home_Brass", (x + math.cos(a) * 0.3, 0.5 + math.sin(a) * 0.3, H - 1.05), (0.6, 0.03, 0.03), yaw=math.degrees(a))
            r.ball("Home_Lamp", (x + math.cos(a) * 0.6, 0.5 + math.sin(a) * 0.6, H - 0.93), 0.09)
    # sitting: a bordered carpet, two carved sofas facing over a round table, an armchair by the fire
    for size, mat in (((6.4, 4.6), "Home_Cream"), ((5.9, 4.1), "Home_RugBlue"), ((5.2, 3.4), "Home_Cream"), ((3.4, 1.8), "Home_Grey"), ((3.0, 1.4), "Home_Cream")):
        r.box(mat, (-3.6, 0.6, 0.006 + 0.004 * (6.4 - size[0])), (size[0], size[1], 0.012))
    r.sofa((-3.6, 2.2), 2.6, 0, "Home_Grey", "Home_Grey", legs="Home_DarkWood", back=0.8)
    r.sofa((-3.6, -1.0), 2.6, 180, "Home_Grey", "Home_Grey", legs="Home_DarkWood", back=0.8)
    r.tube("Home_Walnut", (-3.6, 0.6, 0.62), 0.6, 0.05, sides=20)
    r.tube("Home_Walnut", (-3.6, 0.6, 0.3), 0.07, 0.6, sides=10)
    r.tube("Home_Walnut", (-3.6, 0.6, 0.03), 0.34, 0.06, sides=14)
    r.tube("Home_Cream", (-3.6, 0.6, 0.76), 0.08, 0.22, sides=10, top=0.12)
    r.sofa((6.2, -3.6), 1.1, 110, "Home_Grey", "Home_Grey", legs="Home_DarkWood", back=0.9)
    # eating: a second carpet, a long table and six chairs
    for size, mat in (((4.6, 3.4), "Home_RugBlue"), ((4.1, 2.9), "Home_Cream")):
        r.box(mat, (3.2, 2.2, 0.006 + 0.004 * (4.6 - size[0])), (size[0], size[1], 0.012))
    r.box("Home_DarkWood", (3.2, 2.2, 0.76), (2.8, 1.1, 0.06))
    for sx in (-1, 1):
        for sy in (-1, 1):
            r.tube("Home_DarkWood", (3.2 + sx * 1.25, 2.2 + sy * 0.42, 0.37), 0.05, 0.74, sides=8)
    for k in range(6):
        cx, cy, s = 2.3 + (k % 3) * 0.9, (1.35 if k < 3 else 3.05), (-1 if k < 3 else 1)
        r.box("Home_Grey", (cx, cy, 0.46), (0.46, 0.46, 0.08))
        r.box("Home_DarkWood", (cx, cy + s * 0.21, 0.8), (0.46, 0.05, 0.75))
        for dx in (-0.2, 0.2):
            for dy in (-0.2, 0.2):
                r.box("Home_DarkWood", (cx + dx, cy + dy, 0.21), (0.05, 0.05, 0.42))
    # the television in a cabinet, between the fire and the window
    r.box("Home_DarkWood", (L / 2 - 0.4, 3.2, 0.45), (0.7, 1.8, 0.9))
    r.box("Home_Steel", (L / 2 - 0.3, 3.2, 1.45), (0.08, 1.5, 0.9))
    r.box("Home_Screen", (L / 2 - 0.345, 3.2, 1.45), (0.01, 1.4, 0.8))
    # sleeping: a four-poster behind a folding screen, by the door wall
    for k in range(4):
        r.box("Home_Walnut", (0.4 + k * 0.72, -2.4 + (0.18 if k % 2 else -0.18), 1.0), (0.76, 0.04, 2.0), yaw=(28 if k % 2 else -28))
    r.box("Home_DarkWood", (7.0, -4.4, 0.3), (2.3, 2.2, 0.6)) if False else None
    r.box("Home_DarkWood", (2.0, -4.6, 0.3), (2.3, 2.2, 0.6))
    r.box("Home_Cream", (2.0, -4.55, 0.68), (2.1, 2.0, 0.2))
    r.box("Home_RugBlue", (2.0, -4.1, 0.79), (2.12, 1.2, 0.04))
    for dx in (-0.6, 0.6):
        r.box("Home_White", (2.0 + dx, -5.3, 0.85), (0.7, 0.4, 0.14))
    r.box("Home_DarkWood", (2.0, -5.72, 1.1), (2.3, 0.1, 1.6))
    for sx in (-1, 1):
        for sy in (-1, 1):
            r.tube("Home_DarkWood", (2.0 + sx * 1.1, -4.6 + sy * 1.05, 1.25), 0.05, 2.5, sides=8)
    for sy in (-1, 1):
        r.box("Home_DarkWood", (2.0, -4.6 + sy * 1.05, 2.5), (2.3, 0.07, 0.07))
    for sx in (-1, 1):
        r.box("Home_DarkWood", (2.0 + sx * 1.1, -4.6, 2.5), (0.07, 2.17, 0.07))
    r.box("Home_DarkWood", (4.6, -5.5, 1.1), (1.8, 0.7, 2.2))                    # a wardrobe
    r.box("Home_Brass", (4.6, -5.13, 1.1), (0.03, 0.03, 0.3))
    r.plant((-8.2, 5.2), 1.2, "fern")
    r.plant((8.2, 5.0), 1.7, "tree")
    return r.finish()


def export(obj):
    for o in bpy.data.objects:
        o.select_set(o is obj)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False, axis_forward="-Y", axis_up="Z")
    return path


def picture(obj, eye, look, name):
    scene = bpy.context.scene
    cam = bpy.data.objects.get("Cam") or bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    if cam.name not in scene.collection.objects:
        scene.collection.objects.link(cam)
    scene.camera, cam.data.lens, cam.data.clip_start, cam.data.clip_end = cam, 16, 0.05, 200
    cam.location = eye
    cam.rotation_euler = (Vector(look) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
    for o in bpy.data.objects:
        if o.type == "MESH":
            o.hide_render = o is not obj
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light, scene.display.shading.color_type = "STUDIO", "MATERIAL"
    scene.display.shading.show_cavity = True
    scene.render.resolution_x, scene.render.resolution_y = 1280, 720
    scene.render.filepath = os.path.join(OUT, name)
    bpy.ops.render.render(write_still=True)


for make, views in ((loft, (((0.5, -5.3, 1.7), (-6.5, 1.0, 1.1)), ((-8.2, 5.0, 1.8), (4.0, -2.5, 1.0)))), (classic, (((1.0, -5.2, 1.7), (-7.0, 1.5, 1.4)), ((-7.5, 5.0, 1.8), (7.0, -2.0, 1.2))))):
    obj = make()
    print("INTERIOR:", obj.name, len(obj.data.polygons), "faces,", os.path.getsize(export(obj)) // 1000, "KB")
    for k, (eye, look) in enumerate(views):
        picture(obj, eye, look, f"{obj.name}_{k + 1}.png")
