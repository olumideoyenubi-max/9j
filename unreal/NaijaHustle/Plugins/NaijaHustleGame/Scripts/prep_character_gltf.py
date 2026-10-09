"""Turns a rigged character downloaded from Sketchfab as glTF into an FBX that Unreal imports standing up.

  blender -b --python prep_character_gltf.py -- <scene.gltf> <out folder> <name>

Does what a Sketchfab character needs before import_player_gltf.py: takes the number suffixes off the bone
names, joins the parts into one mesh, moves every rotation and scale off the nodes above the skeleton into the
vertices and bones, puts the result in centimetres and shrinks textures wider than 2048. Writes <name>.fbx,
its textures and preview_front.png / preview_face.png to the out folder, and prints the height: check the
previews and the height before importing.
"""
import bpy, math, os, re, sys

source, out, name = sys.argv[sys.argv.index("--") + 1:][:3]
os.makedirs(os.path.join(out, "textures"), exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=source)
rigs = [o for o in bpy.data.objects if o.type == "ARMATURE"]
meshes = [o for o in bpy.data.objects if o.type == "MESH" and any(m.type == "ARMATURE" for m in o.modifiers)]
if len(rigs) != 1 or not meshes:
    raise SystemExit(f"NH expected one skeleton with skinned meshes, found {len(rigs)} skeletons and {len(meshes)} skinned meshes")
rig = rigs[0]
bpy.context.scene.frame_set(0)
if bpy.data.actions and "keeppose" not in sys.argv:  # a download's own clip: go back to the pose it was bound in
    for action in list(bpy.data.actions):
        bpy.data.actions.remove(action)
    for bone in rig.pose.bones:
        bone.location, bone.rotation_quaternion, bone.rotation_euler, bone.scale = (0, 0, 0), (1, 0, 0, 0), (0, 0, 0), (1, 1, 1)
# The importer keeps the bind pose as the rest pose and puts the nodes' own rotations on top as a pose, which is
# what stands a Sketchfab character up: make the pose it shows the rest pose.
for o in meshes:
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True); bpy.context.view_layer.objects.active = o
    if o.data.shape_keys:
        bpy.ops.object.shape_key_remove(all=True, apply_mix=True)
    for modifier in [m for m in o.modifiers if m.type == "ARMATURE"]:
        bpy.ops.object.modifier_apply(modifier=modifier.name)
bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True); bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode="POSE")
bpy.ops.pose.armature_apply(selected=False)
bpy.ops.object.mode_set(mode="OBJECT")
for o in meshes:
    o.modifiers.new("Armature", "ARMATURE").object = rig

# Sketchfab adds _012 and the like to every node name
numbered = [b for b in rig.data.bones if re.search(r"_\d+$", b.name)]
renames = {}
if len(numbered) > 0.9 * len(rig.data.bones):
    for bone in numbered:
        renames[bone.name] = re.sub(r"_\d+$", "", bone.name)
if "root" not in [renames.get(b.name, b.name) for b in rig.data.bones]:
    top = [b for b in rig.data.bones if not b.parent]
    if len(top) == 1:
        renames[top[0].name] = "root"
for old, new in renames.items():
    rig.data.bones[old].name = new  # vertex groups follow

# everything off the nodes above the skeleton
keep = [rig] + meshes
bpy.ops.object.select_all(action="DESELECT")
for o in keep:
    o.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
for o in [o for o in bpy.data.objects if o not in keep]:
    bpy.data.objects.remove(o)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
body = meshes[0]
bpy.context.view_layer.objects.active = body
if len(meshes) > 1:
    bpy.ops.object.join()
body.name = name
rig.name = "Armature"  # Unreal drops an armature object of this name instead of making it an extra root bone

# feet on the ground, then centimetres with no scale left on anything
low = min((body.matrix_world @ v.co).z for v in body.data.vertices)
height = body.dimensions.z
print(f"NH {name}: {height:.3f} tall as downloaded, lowest point {low:.3f}, {len(rig.data.bones)} bones, {len(body.data.polygons)} faces, materials {[m.name for m in body.data.materials if m]}")
unit = 1.0
if not 1.2 < height < 2.3:  # not metres: make it 1.75 m and say so
    unit = 1.75 / height
    print(f"NH {name}: not in metres, scaled by {unit:.4f}")
scene = bpy.context.scene
scene.unit_settings.scale_length = 0.01
body.parent = None
bpy.ops.object.select_all(action="DESELECT")
for o in (rig, body):
    o.scale = (100.0 * unit,) * 3
    o.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

for image in bpy.data.images:
    if image.size[0] == 0:
        continue
    width, height_px = image.size
    if max(width, height_px) > 2048:
        ratio = 2048 / max(width, height_px)
        image.scale(max(1, round(width * ratio)), max(1, round(height_px * ratio)))
    image.filepath_raw = os.path.join(out, "textures", os.path.splitext(os.path.basename(image.filepath or image.name))[0] + ".png")
    image.file_format = "PNG"
    image.save()

scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x, scene.render.resolution_y = 600, 800
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); scene.collection.objects.link(cam); scene.camera = cam
cam.data.lens, cam.data.clip_end = 70, 5000
tall = body.dimensions.z
for view, angle, look, dist in (("front", 0.35, 0.52, 3.0), ("face", 0.35, 0.92, 0.85)):
    cam.location = (math.sin(angle) * dist * tall, -math.cos(angle) * dist * tall, tall * look)
    cam.rotation_euler = (math.radians(90), 0.0, angle)
    scene.render.filepath = os.path.join(out, f"preview_{view}.png")
    bpy.ops.render.render(write_still=True)

bpy.ops.object.select_all(action="DESELECT")
for o in (rig, body):
    o.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.preferences.addon_enable(module="io_scene_fbx")
fbx = os.path.join(out, f"{name}.fbx")
bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
                         mesh_smooth_type="FACE", use_mesh_modifiers=False, path_mode="COPY", embed_textures=True, apply_scale_options="FBX_SCALE_NONE")
print(f"NH {name}: exported {fbx}, {os.path.getsize(fbx) // 1000} KB, {round(body.dimensions.z)} cm tall, scales {list(rig.scale)} {list(body.scale)}")
