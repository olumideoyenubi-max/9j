"""Builds the vehicles the Task Force and the army come in, in Blender, and exports one FBX each for Unreal.

  blender -b --python build_response_vehicles.py -- <out folder>

  TaskForce_Van      a black mini van with a light bar and TASK FORCE down its sides
  TaskForce_Pickup   a black double-cab pickup, benches in the back, a light bar on the roll bar
  Army_Pickup        the same pickup in army green, no lights
  Army_Truck         a six-wheeled troop truck with a canvas tilt

Each faces +X, sits on the ground at the origin, and is one mesh of a few thousand triangles with plain coloured
materials. Everything is made up here from boxes and cylinders: no real make, force or unit is copied.
Writes <out>/<Name>.fbx, <out>/<Name>.png (a picture) and <out>/response_vehicles.blend.
Bring them into the game with import_response_vehicles.py.
"""
import bpy, bmesh, math, os, sys
from mathutils import Matrix, Vector

OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else os.path.expanduser("~/Downloads/nh-vehicles/response")
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

COLOURS = {
    # name: (colour, roughness, metallic, glow)
    "black": ((0.012, 0.012, 0.016), 0.32, 0.4, 0.0),
    "olive": ((0.11, 0.15, 0.07), 0.75, 0.0, 0.0),
    "canvas": ((0.2, 0.22, 0.12), 0.95, 0.0, 0.0),
    "white": ((0.85, 0.85, 0.82), 0.5, 0.0, 0.0),
    "yellow": ((0.8, 0.55, 0.03), 0.5, 0.0, 0.0),
    "glass": ((0.02, 0.03, 0.04), 0.08, 0.6, 0.0),
    "trim": ((0.05, 0.05, 0.055), 0.7, 0.0, 0.0),
    "steel": ((0.45, 0.46, 0.48), 0.35, 0.9, 0.0),
    "tyre": ((0.015, 0.015, 0.015), 0.95, 0.0, 0.0),
    "wood": ((0.28, 0.17, 0.08), 0.8, 0.0, 0.0),
    "lamp": ((1.0, 0.95, 0.8), 0.2, 0.0, 3.0),
    "tail": ((0.7, 0.02, 0.02), 0.3, 0.0, 1.5),
    "red": ((1.0, 0.03, 0.02), 0.2, 0.0, 6.0),
    "blue": ((0.03, 0.12, 1.0), 0.2, 0.0, 6.0),
}


def material(name):
    mat = bpy.data.materials.get("NHV_" + name)
    if mat:
        return mat
    colour, rough, metal, glow = COLOURS[name]
    mat = bpy.data.materials.new("NHV_" + name)
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


class Vehicle:
    """Gathers boxes, wheels and lettering into one mesh. Metres; +X is the front, +Y its left."""

    def __init__(self, name):
        self.name, self.bm, self.mats = name, bmesh.new(), []

    def slot(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def paint(self, geom, mat):
        index = self.slot(mat)
        for f in {f for v in geom for f in v.link_faces}:
            f.material_index = index

    def box(self, at, size, mat, pitch=0.0, yaw=0.0):
        """pitch leans the top back (toward -X), in degrees."""
        m = Matrix.Translation(at) @ Matrix.Rotation(math.radians(yaw), 4, "Z") @ Matrix.Rotation(math.radians(-pitch), 4, "Y") @ Matrix.Diagonal((*size, 1))
        self.paint(bmesh.ops.create_cube(self.bm, size=1.0, matrix=m)["verts"], mat)

    def both(self, at, size, mat, **kw):
        for side in (1, -1):
            self.box((at[0], at[1] * side, at[2]), size, mat, **kw)

    def wheel(self, x, half, radius, width=0.26):
        """A wheel each side, standing a little proud of a body that is `half` wide from its middle, in a dark arch."""
        y = half + width / 2 - 0.1
        for side in (1, -1):
            turn = Matrix.Translation((x, y * side, radius)) @ Matrix.Rotation(math.radians(90), 4, "X")
            self.paint(bmesh.ops.create_cone(self.bm, cap_ends=True, segments=20, radius1=radius, radius2=radius, depth=width, matrix=turn)["verts"], "tyre")
            self.paint(bmesh.ops.create_cone(self.bm, cap_ends=True, segments=12, radius1=radius * 0.55, radius2=radius * 0.5, depth=width + 0.02, matrix=turn)["verts"], "steel")
            self.box((x, half * side, radius + 0.04), (radius * 2.6, 0.03, radius * 2.0), "trim")   # the dark of the arch behind it

    def words(self, text, at, height, mat, side):
        """Lettering standing on the body's side: side +1 the left, -1 the right, read from outside."""
        curve = bpy.data.curves.new("words", "FONT")
        curve.body, curve.size, curve.extrude, curve.align_x, curve.align_y = text, height, 0.006, "CENTER", "CENTER"
        obj = bpy.data.objects.new("words", curve)
        scene.collection.objects.link(obj)
        bpy.context.view_layer.update()
        mesh = bpy.data.meshes.new_from_object(obj.evaluated_get(bpy.context.evaluated_depsgraph_get()))
        # text lies in XY facing +Z: stand it up, and turn it to face out of that side
        stand = Matrix.Rotation(math.radians(90), 4, "X")
        face = Matrix.Rotation(math.radians(180 if side > 0 else 0), 4, "Z")
        mesh.transform(Matrix.Translation(at) @ face @ stand)
        before = len(self.bm.verts)
        self.bm.from_mesh(mesh)
        self.bm.verts.ensure_lookup_table()
        self.paint(self.bm.verts[before:], mat)
        bpy.data.objects.remove(obj)
        bpy.data.curves.remove(curve)
        bpy.data.meshes.remove(mesh)

    def finish(self):
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        mesh = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for m in self.mats:
            mesh.materials.append(material(m))
        obj = bpy.data.objects.new(self.name, mesh)
        scene.collection.objects.link(obj)
        bevel = obj.modifiers.new("Bevel", "BEVEL")           # every hard edge rounded a little, so they catch the light
        bevel.width, bevel.segments, bevel.limit_method, bevel.angle_limit = 0.022, 2, "ANGLE", math.radians(50)
        return obj


def lights(v, x, z, half):
    v.both((x, half, z), (0.07, 0.34, 0.2), "lamp")
    v.box((x, 0, z), (0.05, half * 1.3, 0.2), "trim")          # grille between them


def light_bar(v, x, z):
    v.box((x, 0, z), (0.3, 1.25, 0.05), "trim")
    v.box((x, 0.36, z + 0.08), (0.26, 0.5, 0.11), "red")
    v.box((x, -0.36, z + 0.08), (0.26, 0.5, 0.11), "blue")
    v.box((x, 0, z + 0.07), (0.22, 0.2, 0.09), "white")


def van():
    v = Vehicle("TaskForce_Van")
    v.box((0, 0, 0.78), (5.1, 1.9, 0.86), "black")                       # lower body
    v.box((-0.15, 0, 1.6), (4.72, 1.84, 0.8), "black")                   # upper body
    v.box((-0.15, 0, 2.02), (4.78, 1.88, 0.07), "black")                 # roof
    v.box((2.3, 0, 1.56), (0.1, 1.62, 0.68), "glass", pitch=16)          # windscreen
    v.box((2.44, 0, 1.2), (0.35, 1.86, 0.1), "black", pitch=-20)         # the short nose under it
    v.box((-2.52, 0, 1.62), (0.04, 1.4, 0.5), "glass")                   # back window
    for x, wide in ((1.52, 0.95), (0.42, 0.9), (-0.58, 0.9), (-1.58, 0.9)):
        v.both((x, 0.925, 1.63), (wide, 0.03, 0.5), "glass")
    v.both((0, 0.955, 0.78), (5.0, 0.02, 0.07), "yellow")                # a stripe down each side
    for side in (1, -1):
        v.words("TASK FORCE", (-0.2, side * 0.962, 1.02), 0.3, "white", side)
    for x in (2.58, -2.58):
        v.box((x, 0, 0.47), (0.16, 1.94, 0.22), "trim")                  # bumpers
    lights(v, 2.56, 0.9, 0.66)
    v.both((-2.56, 0.72, 0.95), (0.06, 0.22, 0.34), "tail")
    v.both((2.05, 1.04, 1.42), (0.08, 0.16, 0.2), "trim")                # mirrors
    light_bar(v, 1.2, 2.08)
    for x in (1.62, -1.55):
        v.wheel(x, 0.95, 0.36)
    return v.finish()


def pickup(name, paint, force):
    v = Vehicle(name)
    v.box((1.05, 0, 0.8), (3.2, 1.88, 0.7), paint)                       # body under the cab and the bonnet
    v.box((2.1, 0, 1.2), (1.1, 1.8, 0.12), paint, pitch=-4)              # bonnet
    v.box((0.25, 0, 1.47), (2.15, 1.74, 0.66), paint)                    # cab
    v.box((0.25, 0, 1.82), (2.2, 1.78, 0.06), paint)                     # its roof
    v.box((1.38, 0, 1.47), (0.1, 1.56, 0.6), "glass", pitch=24)          # windscreen
    v.box((-0.84, 0, 1.5), (0.04, 1.4, 0.44), "glass")                   # back window
    for x in (0.78, -0.22):
        v.both((x, 0.875, 1.5), (0.82, 0.03, 0.44), "glass")
    v.box((-1.6, 0, 0.6), (2.05, 1.88, 0.14), paint)                     # the bed: floor, sides, tailgate
    v.both((-1.6, 0.9, 0.92), (2.05, 0.08, 0.5), paint)
    v.box((-2.6, 0, 0.92), (0.08, 1.88, 0.5), paint)
    v.both((-1.6, 0.62, 0.98), (1.75, 0.3, 0.07), "wood")                # benches for the men
    v.both((-0.78, 0.86, 1.52), (0.08, 0.08, 0.76), "trim")              # roll bar
    v.box((-0.78, 0, 1.9), (0.08, 1.8, 0.08), "trim")
    v.box((2.72, 0, 0.62), (0.1, 1.5, 0.5), "trim")                      # bull bar
    v.both((2.72, 0.45, 0.95), (0.08, 0.08, 0.5), "trim")
    v.box((2.64, 0, 0.5), (0.16, 1.92, 0.2), "trim")
    v.box((-2.66, 0, 0.5), (0.12, 1.92, 0.18), "trim")
    lights(v, 2.63, 0.95, 0.64)
    v.both((-2.65, 0.8, 0.95), (0.05, 0.16, 0.3), "tail")
    v.both((1.2, 1.0, 1.32), (0.08, 0.16, 0.18), "trim")
    if force:
        light_bar(v, 0.25, 1.87)
        v.both((1.0, 0.945, 0.82), (3.0, 0.02, 0.06), "yellow")
        for side in (1, -1):
            v.words("TASK FORCE", (0.3, side * 0.952, 1.0), 0.2, "white", side)
    else:
        for side in (1, -1):
            v.words("ARMY", (0.3, side * 0.952, 0.95), 0.24, "white", side)
    for x in (1.7, -1.62):
        v.wheel(x, 0.94, 0.4, 0.28)
    return v.finish()


def truck():
    v = Vehicle("Army_Truck")
    v.box((0, 0, 0.98), (6.9, 1.1, 0.26), "trim")                        # chassis
    v.box((2.55, 0, 1.95), (1.75, 2.4, 1.6), "olive")                    # cab
    v.box((2.55, 0, 2.78), (1.8, 2.44, 0.08), "olive")
    v.box((3.42, 0, 2.22), (0.08, 2.05, 0.72), "glass", pitch=8)
    v.both((2.6, 1.21, 2.25), (1.0, 0.03, 0.6), "glass")
    v.box((3.5, 0, 1.25), (0.2, 2.5, 0.3), "trim")                       # bumper
    lights(v, 3.45, 1.62, 0.85)
    v.box((-0.95, 0, 1.28), (5.0, 2.5, 0.16), "olive")                   # the bed and its low sides
    v.both((-0.95, 1.22, 1.62), (5.0, 0.08, 0.6), "olive")
    v.box((1.52, 0, 1.62), (0.08, 2.5, 0.6), "olive")
    v.box((-0.95, 0, 2.5), (4.95, 2.46, 1.2), "canvas")                  # the tilt over it
    v.box((-0.95, 0, 3.14), (4.95, 1.9, 0.12), "canvas")
    v.box((-3.43, 0, 2.45), (0.03, 2.0, 1.0), "trim")                    # the open back, dark inside
    v.both((-3.42, 1.0, 1.5), (0.05, 0.2, 0.3), "tail")
    v.both((3.0, 1.32, 2.2), (0.08, 0.2, 0.3), "trim")
    for side in (1, -1):
        v.words("ARMY", (2.55, side * 1.212, 1.6), 0.32, "white", side)
    for x in (2.45, -1.35, -2.6):
        v.wheel(x, 1.2, 0.55, 0.34)
    return v.finish()


def picture(obj, path):
    """Three-quarter view on a grey floor in afternoon sun."""
    for o in scene.collection.objects:
        o.hide_render = o is not obj and o.type == "MESH" and o.name != "Floor"
    size = max(obj.dimensions)
    cam = bpy.data.objects.get("Cam")
    if not cam:
        cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
        scene.collection.objects.link(cam)
        scene.camera = cam
        sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
        scene.collection.objects.link(sun)
        sun.data.energy, sun.rotation_euler = 4.0, (math.radians(50), 0, math.radians(35))
        floor = bpy.data.meshes.new("Floor")
        floor.from_pydata([(-40, -40, 0), (40, -40, 0), (40, 40, 0), (-40, 40, 0)], [], [(0, 1, 2, 3)])
        scene.collection.objects.link(bpy.data.objects.new("Floor", floor))
        world = bpy.data.worlds.new("World")
        world.use_nodes = True
        world.node_tree.nodes["Background"].inputs[0].default_value = (0.5, 0.6, 0.75, 1)
        world.node_tree.nodes["Background"].inputs[1].default_value = 0.8
        scene.world = world
    at = Vector((0, 0, obj.dimensions.z * 0.45))
    cam.location = at + Vector((0.9, -1.0, 0.4)).normalized() * size * 1.75
    cam.rotation_euler = (at - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = 1100, 700
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


made = [van(), pickup("TaskForce_Pickup", "black", True), pickup("Army_Pickup", "olive", False), truck()]
for obj in made:
    picture(obj, os.path.join(OUT, obj.name + ".png"))
for obj in made:
    for o in bpy.context.view_layer.objects:
        o.select_set(o is obj)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, use_mesh_modifiers=True, mesh_smooth_type="FACE",
                             axis_forward="-Y", axis_up="Z", bake_anim=False)
    mesh = obj.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
    print(f"NH vehicle {obj.name}: {obj.dimensions.x:.2f} x {obj.dimensions.y:.2f} x {obj.dimensions.z:.2f} m, {sum(len(p.vertices) - 2 for p in mesh.polygons)} triangles, {os.path.getsize(path) // 1000} KB")
for obj in made:
    obj.hide_render = False
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "response_vehicles.blend"))
