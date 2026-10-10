"""Gets a downloaded room model ready to be the inside of a home: trimmed, stood on the floor, and centred on a clear spot.

  blender -b --factory-startup --python prep_interior_gltf.py -- <in.glb> <out folder> [keep: xmin,xmax]

Writes <out folder>/scene.glb, for import_interiors.py. What it does:
  - with "keep", drops every object whose middle is outside that span of X (a kit laid out beside the room it builds)
  - puts the floor at height 0
  - finds the most open patch of floor (the point furthest from anything standing between ankle and head height)
    and makes it the model's origin: the game stands the player at the origin when they come in
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
keep = [float(v) for v in args[2].split(",")] if len(args) > 2 else None
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
dropped = 0
if keep:
    for o in list(meshes):
        mid = sum(((o.matrix_world @ Vector(c)) for c in o.bound_box), Vector()) / 8
        if not keep[0] <= mid.x <= keep[1]:
            bpy.data.objects.remove(o, do_unlink=True)
            meshes.remove(o)
            dropped += 1
bm = bmesh.new()
for o in meshes:
    m = o.data.copy()
    m.transform(o.matrix_world)
    bm.from_mesh(m)
    bpy.data.meshes.remove(m)
tree = BVHTree.FromBMesh(bm)
pts = [v.co for v in bm.verts]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
# the floor: what a ray from waist height finds going down, most often
drops = []
step = 0.25
grid = [(lo.x + (i + 0.5) * step, lo.y + (j + 0.5) * step) for i in range(int((hi.x - lo.x) / step)) for j in range(int((hi.y - lo.y) / step))]
for x, y in grid:
    hit = tree.ray_cast(Vector((x, y, lo.z + 1.0)), Vector((0, 0, -1)))
    if hit[0] is not None:
        drops.append(round(hit[0].z, 2))
floor = max(set(drops), key=drops.count)
best, where = -1.0, None
for x, y in grid:
    down = tree.ray_cast(Vector((x, y, floor + 1.0)), Vector((0, 0, -1)))
    up = tree.ray_cast(Vector((x, y, floor + 0.3)), Vector((0, 0, 1)))
    if down[0] is None or abs(down[0].z - floor) > 0.05 or up[0] is None or up[0].z - floor < 2.1:
        continue                                   # no floor here, or something overhead, or no ceiling: outside the room
    # how far the nearest thing is at chest and head height (the floor itself is a metre off, so that is the most it can say),
    # and of equally open spots, the one nearest the middle of the room
    clear = min((tree.find_nearest(Vector((x, y, floor + h)))[3] or 9.0) for h in (1.0, 1.5))
    score = min(clear, 0.9) - 0.02 * (Vector((x, y)) - Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2))).length
    if score > best:
        best, where = score, Vector((x, y, floor))
if where is None:
    where = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, floor))
for o in bpy.data.objects:
    if o.parent is None:
        o.matrix_world.translation -= where
os.makedirs(out, exist_ok=True)
bpy.ops.export_scene.gltf(filepath=os.path.join(out, "scene.glb"), export_format="GLB", export_apply=True)
print(f"INTERIOR PREP: {os.path.basename(src)}: {len(meshes)} meshes kept, {dropped} dropped; room {hi.x - lo.x:.1f} x {hi.y - lo.y:.1f} x {hi.z - floor:.1f} m; stands at {[round(v, 2) for v in where]} score {best:.2f}; {os.path.getsize(os.path.join(out, 'scene.glb')) // 1000} KB")
