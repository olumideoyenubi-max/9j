"""Gets a downloaded tree model ready to be planted through the city, and lists where the city's trees stand.

  blender -b --factory-startup --python prep_tree_gltf.py -- tree <in.glb> <out folder>
      one mesh, 10 m tall, standing on the origin with its trunk's foot there: <out folder>/scene.glb
  blender -b --factory-startup --python prep_tree_gltf.py -- places <Trees_Low.fbx> <out.json>
      every tree of the model's tree pieces (each is a loose cone-and-stick): its place and height, in Unreal cm,
      as a flat list [x, y, height, x, y, height, ...] for ANHTrees
"""
import bpy, bmesh, json, os, sys
from mathutils import Vector

mode, src, out = sys.argv[sys.argv.index("--") + 1:][:3]
bpy.ops.wm.read_factory_settings(use_empty=True)
if mode == "tree":
    bpy.ops.import_scene.gltf(filepath=src)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    for o in bpy.data.objects:
        o.select_set(o in meshes)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    tree = bpy.context.view_layer.objects.active
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    pts = [v.co.copy() for v in tree.data.vertices]
    low, high = min(p.z for p in pts), max(p.z for p in pts)
    foot = [p for p in pts if p.z < low + (high - low) * 0.06]
    at = Vector((sum(p.x for p in foot) / len(foot), sum(p.y for p in foot) / len(foot), low))
    k = 10.0 / (high - low)
    for v in tree.data.vertices:
        v.co = (v.co - at) * k
    for o in list(bpy.data.objects):
        if o is not tree:
            bpy.data.objects.remove(o, do_unlink=True)
    os.makedirs(out, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(out, "scene.glb"), export_format="GLB", export_apply=True)
    print(f"TREE PREP: {os.path.basename(src)}: {len(tree.data.polygons)} faces, {len(tree.data.materials)} materials, {os.path.getsize(os.path.join(out, 'scene.glb')) // 1000} KB")
else:
    bpy.ops.import_scene.fbx(filepath=src)
    flat, count = [], 0
    for o in bpy.data.objects:
        if o.type != "MESH":
            continue
        bm = bmesh.new()
        bm.from_mesh(o.data)
        seen = set()
        for start in bm.verts:
            if start.index in seen:
                continue
            part, stack = [], [start]
            seen.add(start.index)
            while stack:
                v = stack.pop()
                part.append(o.matrix_world @ v.co)
                for e in v.link_edges:
                    w = e.other_vert(v)
                    if w.index not in seen:
                        seen.add(w.index)
                        stack.append(w)
            lo, hi = min(p.z for p in part), max(p.z for p in part)
            if hi - lo < 1.5 or len(part) < 4:
                continue
            x, y = sum(p.x for p in part) / len(part), sum(p.y for p in part) / len(part)
            flat += [round(x * 100), round(-y * 100), round((hi - lo) * 100)]      # metres, Y north: Unreal cm, Y south
            count += 1
        bm.free()
    # a trunk and its crown may be separate loose parts on the same spot: one tree a spot, the taller
    spots = {}
    for k in range(0, len(flat), 3):
        key = (round(flat[k] / 150), round(flat[k + 1] / 150))
        if key not in spots or flat[k + 2] > spots[key][2]:
            spots[key] = flat[k:k + 3]
    merged = [v for s in spots.values() for v in s]
    with open(out, "w") as fh:
        json.dump({"format": "naija-hustle-trees", "note": "Where the city's trees stand: x, y, height in Unreal cm, three numbers a tree. Written by Scripts/prep_tree_gltf.py from the model's Trees_Low pieces.", "trees": merged}, fh, separators=(",", ":"))
    hs = sorted(merged[2::3])
    print(f"TREE PLACES: {count} loose parts, {len(merged) // 3} trees; heights {hs[0] / 100:.1f} to {hs[-1] / 100:.1f} m, median {hs[len(hs) // 2] / 100:.1f} m; {os.path.getsize(out) // 1000} KB")
