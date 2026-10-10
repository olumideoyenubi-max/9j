"""Gets a downloaded car model ready for the game: one mesh, the right length, wheels on the ground, nose along +X.

  blender -b --factory-startup --python prep_car_gltf.py -- <in.glb> <out folder> <length in metres> [turn in degrees] [picture.png]

Writes <out folder>/scene.gltf (with its .bin and textures), the layout import_car_gltf.py reads. The model is joined
into one mesh with every part's own placing applied (a model brought in with its parts' placings lost comes out with
its wheels in the wrong place: that is what was wrong with the first SUV), laid along its longest side, stood with its
lowest point on the ground at the middle of its length and width, and scaled to the length given. "turn" spins it
about the upright first (180 if it would otherwise face backwards). Prints its size, for Data/vehicle_meshes.json:
yaw 0, scale 1, offset 0, height as printed.
"""
import bpy, math, os, sys
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out, length = args[0], args[1], float(args[2])
turn = float(args[3]) if len(args) > 3 else 0.0
shot = args[4] if len(args) > 4 else None
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
for o in bpy.data.objects:
    o.select_set(o in meshes)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
car = bpy.context.view_layer.objects.active
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
for o in list(bpy.data.objects):
    if o is not car:
        bpy.data.objects.remove(o, do_unlink=True)
pts = [v.co.copy() for v in car.data.vertices]
size = Vector((max(p.x for p in pts) - min(p.x for p in pts), max(p.y for p in pts) - min(p.y for p in pts), 0))
spin = Matrix.Rotation(math.radians(turn + (90.0 if size.y > size.x else 0.0)), 3, "Z")     # its length along X
pts = [spin @ p for p in pts]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
k = length / (hi.x - lo.x)
mid = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
for v, p in zip(car.data.vertices, pts):
    v.co = (p - mid) * k
car.data.update()
os.makedirs(out, exist_ok=True)
for o in bpy.data.objects:
    o.select_set(o is car)
bpy.ops.export_scene.gltf(filepath=os.path.join(out, "scene.gltf"), export_format="GLTF_SEPARATE", use_selection=True, export_apply=True)
print(f"CAR PREP: {os.path.basename(src)}: {len(car.data.polygons)} faces, {len(car.data.materials)} materials; {(hi.x - lo.x) * k:.2f} x {(hi.y - lo.y) * k:.2f} x {(hi.z - lo.z) * k:.2f} m (height {(hi.z - lo.z) * k * 100:.1f} cm)")
if shot:
    scene = bpy.context.scene
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); scene.collection.objects.link(cam); scene.camera = cam
    cam.data.lens = 50; cam.data.clip_end = 1000
    at = Vector((0, 0, 0.7)); cam.location = at + Vector((1.0, -1.0, 0.35)).normalized() * 11
    cam.rotation_euler = (at - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.engine = "BLENDER_WORKBENCH"; scene.display.shading.light = "STUDIO"; scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 800, 450
    scene.render.filepath = shot; bpy.ops.render.render(write_still=True)
