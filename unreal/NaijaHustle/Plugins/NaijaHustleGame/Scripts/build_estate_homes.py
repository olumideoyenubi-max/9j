"""Makes the luxury house and the apartment tower that NHEstate stands on house and flat plots, in Blender.

  blender -b --factory-startup --python build_estate_homes.py -- <out folder>     (default <repo>/project-files/downloads/nh-estate)

Writes SM_Mansion.fbx and SM_Tower.fbx (metres in Blender, centimetres in Unreal; the front faces +X, the origin is the
middle of the footprint on the ground) and a picture of each beside them. Bring them in with import_estate_homes.py.

  Mansion   a two-storey modern house: a white ground floor with a garage wing, a longer upper floor cantilevered over
            the door, glazing with dark mullions, timber panels, a glass balustrade, flat roofs, a pool beside it
  Tower     a 22-storey block: a stone podium with a glazed lobby, a glass shaft banded by white floor slabs, balconies
            in alternate bays, corner fins, and a set-back double-height penthouse under a floating roof
Everything is boxes and a few cylinders with flat-coloured materials; both are original designs, not copies of buildings.
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv and len(sys.argv) > sys.argv.index("--") + 1 else os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "..", "project-files", "downloads", "nh-estate"))
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)

MATS = {  # name: colour, roughness, metallic, emission
    "Est_Render": ((0.9, 0.89, 0.86), 0.75, 0.0, 0.0), "Est_Glass": ((0.05, 0.11, 0.16), 0.08, 0.6, 0.0), "Est_Stone": ((0.42, 0.4, 0.37), 0.85, 0.0, 0.0),
    "Est_Dark": ((0.035, 0.035, 0.04), 0.5, 0.2, 0.0), "Est_Wood": ((0.33, 0.17, 0.07), 0.6, 0.0, 0.0), "Est_Water": ((0.02, 0.32, 0.45), 0.05, 0.0, 0.0),
    "Est_Light": ((1.0, 0.86, 0.6), 0.4, 0.0, 6.0), "Est_Metal": ((0.55, 0.56, 0.58), 0.3, 1.0, 0.0), "Est_Green": ((0.07, 0.2, 0.05), 0.9, 0.0, 0.0)}


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
ORDER = list(MATS)


class Building:
    def __init__(self, name):
        self.name, self.bm = name, bmesh.new()

    def box(self, mat, centre, size, yaw=0.0):
        made = bmesh.ops.create_cube(self.bm, size=1.0, matrix=Matrix.Translation(centre) @ Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Diagonal((*size, 1)))
        for f in {f for v in made["verts"] for f in v.link_faces}:
            f.material_index = ORDER.index(mat)

    def post(self, mat, centre, radius, height):
        made = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=12, radius1=radius, radius2=radius, depth=height, matrix=Matrix.Translation(centre))
        for f in {f for v in made["verts"] for f in v.link_faces}:
            f.material_index = ORDER.index(mat)

    def glazing(self, x, y0, y1, z0, z1, every=2.0, facing="x"):
        """A wall of glass with dark frames: at x across y0..y1 (or, facing y, at y=x across x0..x1), from z0 up to z1."""
        mid, span, tall = (y0 + y1) / 2, abs(y1 - y0), z1 - z0
        at = (lambda u, z, d=0.0: (x + d, u, z)) if facing == "x" else (lambda u, z, d=0.0: (u, x + d, z))
        size = (lambda a, u, z: (a, u, z)) if facing == "x" else (lambda a, u, z: (u, a, z))
        self.box("Est_Glass", at(mid, z0 + tall / 2), size(0.08, span, tall))
        for k in range(int(span // every) + 1):
            u = min(y0, y1) + k * span / max(int(span // every), 1)
            self.box("Est_Dark", at(u, z0 + tall / 2), size(0.16, 0.09, tall))
        for z in (z0 + 0.05, z1 - 0.05):
            self.box("Est_Dark", at(mid, z), size(0.16, span, 0.1))

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


def mansion():
    b = Building("SM_Mansion")
    b.box("Est_Stone", (0, 0, 0.2), (21, 31, 0.4))                                  # the plinth and the drive
    b.box("Est_Stone", (12.5, -2, 0.05), (4, 9, 0.1))
    # ground floor: the main block, and the garage wing to the left
    b.box("Est_Render", (-1, 3, 2.3), (13, 17, 3.8))
    b.box("Est_Render", (0.5, -10.5, 2.0), (10, 9.5, 3.2))
    for k in range(3):                                                              # three garage doors
        y = -13.3 + k * 2.9
        b.box("Est_Metal", (5.55, y, 1.55), (0.12, 2.5, 2.5))
        for z in (0.7, 1.3, 1.9, 2.5):
            b.box("Est_Dark", (5.62, y, z), (0.04, 2.5, 0.04))
    b.box("Est_Dark", (0.5, -10.5, 3.75), (10.6, 10.1, 0.3))                        # the wing's flat roof
    b.glazing(5.55, 5.2, 11.2, 0.6, 3.8)                                            # the living room's window wall
    b.box("Est_Wood", (5.56, 1.5, 1.9), (0.14, 2.2, 3.0))                           # the front door, in its timber surround
    b.box("Est_Dark", (5.66, 1.5, 1.75), (0.06, 1.7, 2.7))
    b.box("Est_Metal", (5.72, 2.1, 1.7), (0.04, 0.06, 0.9))
    for y in (-0.6, 3.6):                                                           # the columns of the porch
        b.box("Est_Render", (8.2, y, 3.9), (0.5, 0.5, 7.0))
    # first floor: longer than the ground floor and pushed forward over the door
    b.box("Est_Render", (1.2, 1.5, 5.9), (15, 24, 3.4))
    b.glazing(8.75, -8.5, 0.5, 4.5, 7.4)
    b.glazing(8.75, 4.5, 12.5, 4.5, 7.4)
    b.box("Est_Wood", (8.72, 2.5, 5.9), (0.12, 3.6, 3.2))                           # timber between the two window walls
    for k in range(9):
        b.box("Est_Dark", (8.8, 0.85 + k * 0.41, 5.9), (0.05, 0.05, 3.2))
    b.box("Est_Glass", (9.9, 1.5, 4.75), (0.05, 23.5, 1.05))                        # the balcony's glass balustrade
    b.box("Est_Stone", (9.3, 1.5, 4.2), (1.5, 24, 0.22))
    b.box("Est_Metal", (9.9, 1.5, 5.3), (0.07, 23.6, 0.05))
    b.box("Est_Dark", (1.4, 1.5, 7.75), (17, 25.6, 0.3))                            # the roof, oversailing all round
    b.box("Est_Render", (-2, 6, 8.6), (6, 7, 1.4))                                  # a roof-top room, and the tank screen
    b.glazing(1.05, 3, 9, 8.0, 9.2)
    b.box("Est_Dark", (-2, 6, 9.4), (6.8, 7.8, 0.2))
    b.glazing(11.55, -6, 4, 0.6, 3.6, facing="y")                                   # the side of the living room, to the pool
    # the pool beside the house, in its deck, with loungers and lights
    b.box("Est_Stone", (2, 15.5, 0.45), (15, 7.5, 0.12))
    b.box("Est_Water", (2, 15.5, 0.5), (11, 4.2, 0.1))
    for k in range(3):
        b.box("Est_Render", (-2 + k * 3.2, 12.6, 0.75), (1.9, 0.7, 0.2))
        b.box("Est_Render", (-2.75 + k * 3.2, 12.6, 0.95), (0.5, 0.7, 0.45), yaw=0)
    for x in (-5, 2, 9):
        b.post("Est_Dark", (x, 19.0, 1.0), 0.05, 1.1)
        b.box("Est_Light", (x, 19.0, 1.6), (0.22, 0.22, 0.22))
    # down-lights under the roof and by the door, a hedge along the front, two palms
    for y in (-8, -3, 2, 7, 12):
        b.box("Est_Light", (9.2, y, 7.55), (0.3, 0.3, 0.06))
    for y in (0.2, 2.8):
        b.box("Est_Light", (5.7, y, 3.3), (0.1, 0.25, 0.4))
    b.box("Est_Green", (10.2, 9, 0.75), (0.9, 9, 0.7))
    b.box("Est_Green", (10.2, -6.5, 0.75), (0.9, 5, 0.7))
    for y in (-2.6, 13.8):
        b.post("Est_Wood", (9.6, y, 3.0), 0.16, 5.2)
        for k in range(7):
            a = math.radians(k * 360 / 7)
            b.box("Est_Green", (9.6 + math.cos(a) * 1.3, y + math.sin(a) * 1.3, 5.5), (2.6, 0.45, 0.08), yaw=math.degrees(a))
    return b.finish()


def tower():
    b = Building("SM_Tower")
    floors, floor_h, half = 22, 3.3, 12.0
    b.box("Est_Stone", (0, 0, 0.25), (34, 34, 0.5))
    b.box("Est_Stone", (0, 0, 4.25), (30, 30, 7.5))                                 # the podium
    b.glazing(15.02, -9, 9, 0.6, 6.4, every=3.0)                                    # the lobby's glass front
    b.box("Est_Dark", (16.5, 0, 6.9), (4.5, 12, 0.3))                               # the canopy over the door, on two posts
    for y in (-5, 5):
        b.post("Est_Metal", (18.2, y, 3.6), 0.14, 6.3)
    for y in (-12.5, 12.5):
        b.box("Est_Light", (15.06, y, 4.0), (0.06, 0.5, 5.0))
    base = 8.0
    b.box("Est_Glass", (0, 0, base + floors * floor_h / 2), (2 * half, 2 * half, floors * floor_h))   # the shaft
    for f in range(floors + 1):                                                     # a white slab edge at every floor
        b.box("Est_Render", (0, 0, base + f * floor_h), (2 * half + 0.9, 2 * half + 0.9, 0.32))
    for sx in (-1, 1):                                                              # fins up the four corners
        for sy in (-1, 1):
            b.box("Est_Render", (sx * (half + 0.2), sy * (half + 0.2), base + floors * floor_h / 2), (1.3, 1.3, floors * floor_h))
    for f in range(floors):                                                         # balconies in alternate bays, the pattern stepping floor by floor
        z = base + f * floor_h
        for bay in range(4):
            if (bay + f) % 2:
                continue
            u = -8.4 + bay * 5.6
            for facing, sign in (("x", 1), ("x", -1), ("y", 1), ("y", -1)):
                out = sign * (half + 1.25)
                centre = (out, u, z + 0.18) if facing == "x" else (u, out, z + 0.18)
                size = (2.5, 4.6, 0.2) if facing == "x" else (4.6, 2.5, 0.2)
                b.box("Est_Render", centre, size)
                rail = (out + sign * 1.2, u, z + 0.85) if facing == "x" else (u, out + sign * 1.2, z + 0.85)
                b.box("Est_Glass", rail, (0.05, 4.6, 1.1) if facing == "x" else (4.6, 0.05, 1.1))
        for u in (-11.2, -5.6, 0, 5.6, 11.2):                                       # mullions on the front and back
            for sign in (1, -1):
                b.box("Est_Dark", (sign * (half + 0.03), u, z + floor_h / 2), (0.08, 0.12, floor_h))
                b.box("Est_Dark", (u, sign * (half + 0.03), z + floor_h / 2), (0.12, 0.08, floor_h))
    top = base + floors * floor_h
    b.box("Est_Glass", (0, 0, top + 3.6), (17, 17, 7.0))                            # the penthouse, set back, on its terrace
    for u in (-8.5, -4.25, 0, 4.25, 8.5):
        for sign in (1, -1):
            b.box("Est_Dark", (sign * 8.53, u, top + 3.6), (0.1, 0.14, 7.0))
            b.box("Est_Dark", (u, sign * 8.53, top + 3.6), (0.14, 0.1, 7.0))
    b.box("Est_Glass", (0, 0, top + 0.75), (2 * half + 0.4, 2 * half + 0.4, 1.1))   # the terrace's balustrade (a glass band)
    b.box("Est_Water", (8.5, -6, top + 0.35), (5, 9, 0.12))                         # a pool on the terrace
    b.box("Est_Render", (0, 0, top + 7.4), (22, 22, 0.45))                          # the roof, floating wider than the penthouse
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.post("Est_Metal", (sx * 10, sy * 10, top + 3.7), 0.18, 7.2)
    b.box("Est_Dark", (-3, 3, top + 8.7), (5, 5, 2.2))                              # plant, and the mast
    b.post("Est_Metal", (-3, 3, top + 14.5), 0.12, 9.5)
    b.box("Est_Light", (-3, 3, top + 19.3), (0.3, 0.3, 0.3))
    return b.finish()


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
        sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
        sun.rotation_euler, sun.data.energy = (math.radians(50), 0, math.radians(120)), 4
        scene.collection.objects.link(sun)
    scene.camera, cam.data.lens, cam.data.clip_end = cam, 28, 2000
    cam.location = eye
    cam.rotation_euler = (Vector(look) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
    for o in bpy.data.objects:
        if o.type == "MESH":
            o.hide_render = o is not obj
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light, scene.display.shading.color_type = "STUDIO", "MATERIAL"
    scene.display.shading.show_shadows = True
    scene.render.resolution_x, scene.render.resolution_y = 1100, 700
    scene.render.filepath = os.path.join(OUT, name)
    bpy.ops.render.render(write_still=True)


house, block = mansion(), tower()
for obj, eye, look in ((house, (34, -22, 9), (2, 1, 4)), (block, (75, -60, 30), (0, 0, 40))):
    print("ESTATE:", obj.name, len(obj.data.polygons), "faces,", [round(v, 1) for v in obj.dimensions], "m,", os.path.getsize(export(obj)) // 1000, "KB")
    picture(obj, eye, look, obj.name + ".png")
