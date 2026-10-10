"""Cuts the Lagos city model (lagos_city_detailed.blend) into small pieces for Unreal.

  blender -b lagos_city_detailed.blend --python lagos_retile.py -- <out folder> [only=<object name>]

The model's own tiles are 6 km across, so a whole tile is drawn whenever any corner of it is in view. This cuts the
simplified buildings, the trees, the roads and the bridges into square cells (600 m for buildings and trees, 1.2 km
for roads and bridges), one FBX per source object with one mesh per cell, so Unreal can hide whatever is far away.
Every piece keeps the shared world origin: place each at 0,0,0. One Blender unit is 100 m; the FBX is in centimetres.
Also writes manifest.json (each piece's bounds and size) for the Unreal side.
"""
import bpy, json, os, sys
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
out = args[0]
only = next((a.split("=", 1)[1] for a in args[1:] if a.startswith("only=")), None)
os.makedirs(out, exist_ok=True)
bpy.ops.preferences.addon_enable(module="io_scene_fbx")
SOURCES = [(o.name, 6.0) for o in bpy.data.collections["Buildings_Low"].objects if o.type == "MESH"]
SOURCES += [("Trees", 6.0)] + [(o.name, 12.0) for name in ("Roads", "Bridges") for o in bpy.data.collections[name].objects if o.type == "MESH"]
manifest_path = os.path.join(out, "manifest.json")
manifest = json.load(open(manifest_path)) if os.path.exists(manifest_path) else {}
scratch = bpy.data.collections.new("NH_Export")
bpy.context.scene.collection.children.link(scratch)


def cut(obj, size):
    me = obj.data
    nv, nl, npoly = len(me.vertices), len(me.loops), len(me.polygons)
    co = np.empty(nv * 3, np.float32); me.vertices.foreach_get("co", co); co = co.reshape(-1, 3)
    world = np.array(obj.matrix_world, np.float32)
    co = co @ world[:3, :3].T + world[:3, 3]
    lv = np.empty(nl, np.int32); me.loops.foreach_get("vertex_index", lv)
    ls = np.empty(npoly, np.int32); me.polygons.foreach_get("loop_start", ls)
    lt = np.empty(npoly, np.int32); me.polygons.foreach_get("loop_total", lt)
    mat = np.empty(npoly, np.int32); me.polygons.foreach_get("material_index", mat)
    smooth = np.empty(npoly, bool); me.polygons.foreach_get("use_smooth", smooth)
    centre = np.empty(npoly * 3, np.float32); me.polygons.foreach_get("center", centre); centre = centre.reshape(-1, 3) @ world[:3, :3].T + world[:3, 3]
    uv = None
    if me.uv_layers.active:
        uv = np.empty(nl * 2, np.float32); me.uv_layers.active.data.foreach_get("uv", uv); uv = uv.reshape(-1, 2)
    colour = me.color_attributes.active_color or (me.color_attributes[0] if me.color_attributes else None)
    col = None
    if colour:
        col = np.empty(len(colour.data) * 4, np.float32); colour.data.foreach_get("color", col); col = col.reshape(-1, 4)
    cx = np.floor(centre[:, 0] / size).astype(np.int64); cy = np.floor(centre[:, 1] / size).astype(np.int64)
    key = cx * 100000 + cy
    order = np.argsort(key, kind="stable")
    bounds = np.flatnonzero(np.diff(key[order])) + 1
    made = []
    for faces in np.split(order, bounds):
        faces = np.sort(faces)
        total = lt[faces]
        starts = np.concatenate(([0], np.cumsum(total)[:-1])).astype(np.int32)
        loops = np.repeat(ls[faces], total) + (np.arange(int(total.sum()), dtype=np.int32) - np.repeat(starts, total))
        used, remap = np.unique(lv[loops], return_inverse=True)
        name = f"{obj.name}__{int(cx[faces[0]])}_{int(cy[faces[0]])}".replace("-", "m")
        new = bpy.data.meshes.new(name)
        new.vertices.add(len(used)); new.loops.add(len(loops)); new.polygons.add(len(faces))
        new.vertices.foreach_set("co", co[used].ravel())
        new.loops.foreach_set("vertex_index", remap.astype(np.int32))
        new.polygons.foreach_set("loop_start", starts); new.polygons.foreach_set("loop_total", total)
        new.polygons.foreach_set("material_index", mat[faces]); new.polygons.foreach_set("use_smooth", smooth[faces])
        if uv is not None:
            new.uv_layers.new(name=me.uv_layers.active.name).data.foreach_set("uv", uv[loops].ravel())
        if col is not None:
            layer = new.color_attributes.new(colour.name, colour.data_type, colour.domain)
            layer.data.foreach_set("color", (col[loops] if colour.domain == "CORNER" else col[used]).ravel())
        for m in me.materials:
            new.materials.append(m)
        new.update()
        piece = bpy.data.objects.new(name, new)
        scratch.objects.link(piece)
        made.append(piece)
        lo, hi = co[used].min(axis=0), co[used].max(axis=0)
        manifest[name] = {"file": obj.name, "min": [round(float(v), 3) for v in lo], "max": [round(float(v), 3) for v in hi], "verts": int(len(used)), "faces": int(len(faces))}
    return made


for source, size in SOURCES:
    if only and source != only:
        continue
    target = os.path.join(out, source + ".fbx")
    if os.path.exists(target) and not only:
        continue
    pieces = cut(bpy.data.objects[source], size)
    bpy.ops.object.select_all(action="DESELECT")
    for p in pieces:
        p.select_set(True)
    bpy.context.view_layer.objects.active = pieces[0]
    bpy.ops.export_scene.fbx(filepath=target, use_selection=True, object_types={"MESH"}, global_scale=100.0, apply_scale_options="FBX_SCALE_NONE",
                             mesh_smooth_type="FACE", use_mesh_modifiers=False, bake_anim=False, colors_type="SRGB", path_mode="AUTO")
    print(f"NH {source}: {len(pieces)} pieces, {sum(len(p.data.vertices) for p in pieces)} verts, {os.path.getsize(target) // 1000} KB")
    for p in pieces:
        mesh = p.data
        bpy.data.objects.remove(p)
        bpy.data.meshes.remove(mesh)
    json.dump(manifest, open(manifest_path, "w"))
pylon = bpy.data.objects.get("LinkBridge_Pylon")
if pylon:
    import mathutils
    bb = [pylon.matrix_world @ mathutils.Vector(c) for c in pylon.bound_box]
    print("NH link bridge pylon centre", round(sum(b.x for b in bb) / 8, 2), round(sum(b.y for b in bb) / 8, 2), "height", round(max(b.z for b in bb), 2))
