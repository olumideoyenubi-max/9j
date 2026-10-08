"""Builds the NAIJA HUSTLE player with MPFB (MakeHuman) in Blender and exports an FBX for Unreal.

  blender -b --python mh_build.py -- <out folder> [preview]

Needs the MPFB extension and, in its user data folder, the skin, hair, eyes, eyebrows, eyelashes, teeth, beard,
tank shirt and cargo trousers named below.
"""
import bpy, glob, math, os, sys
from bl_ext.blender_org.mpfb.services.humanservice import HumanService
from bl_ext.blender_org.mpfb.services.targetservice import TargetService
from bl_ext.blender_org.mpfb.services.locationservice import LocationService

out = sys.argv[sys.argv.index("--") + 1]
preview_only = "preview" in sys.argv
os.makedirs(out, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.preferences.addon_enable(module="bl_ext.blender_org.mpfb")
data = LocationService.get_user_data()


def asset(*patterns):
    for pattern in patterns:
        found = sorted(glob.glob(os.path.join(data, pattern)))
        if found:
            return found[0]
    print("NH missing asset", patterns)
    return None


macro = TargetService.get_default_macro_info_dict()
macro.update({"gender": 1.0, "age": 0.5, "muscle": 0.82, "weight": 0.45, "proportions": 0.7, "height": 0.5})
macro["race"] = {"asian": 0.0, "caucasian": 0.0, "african": 1.0}
base = HumanService.create_human(scale=0.1, macro_detail_dict=macro)
base.name = "Body"
rig = HumanService.add_builtin_rig(base, "game_engine")

# The Unreal mannequin's animation Blueprint plants the feet using IK bones, which this rig does not have: add them,
# each sitting on the bone it shadows (the retarget pins them there).
bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True); bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode="EDIT")
bones = rig.data.edit_bones
root = bones["Root"]


def ik_bone(bone_name, parent, like=None):
    bone = bones.new(bone_name)
    source = bones[like] if like else root
    bone.head, bone.tail, bone.roll = source.head.copy(), source.tail.copy(), source.roll
    bone.parent = parent
    bone.use_deform = False
    return bone


foot_root = ik_bone("ik_foot_root", root)
ik_bone("ik_foot_l", foot_root, "foot_l"); ik_bone("ik_foot_r", foot_root, "foot_r")
hand_root = ik_bone("ik_hand_root", root)
gun = ik_bone("ik_hand_gun", hand_root, "hand_r")
ik_bone("ik_hand_l", gun, "hand_l"); ik_bone("ik_hand_r", gun, "hand_r")
bpy.ops.object.mode_set(mode="OBJECT")
skin = asset("skins/young_african_male/*.mhmat")
if skin:
    HumanService.set_character_skin(skin, base, skin_type="MAKESKIN", material_instances=False)

parts = [("Eyes", ["eyes/low-poly/*.mhclo"]), ("Eyebrows", ["eyebrows/eyebrow001/*.mhclo"]), ("Eyelashes", ["eyelashes/eyelashes01/*.mhclo"]),
         ("Teeth", ["teeth/teeth_base/*.mhclo"]), ("Hair", ["hair/short01/*.mhclo", "hair/short0*/*.mhclo"]),
         ("Clothes", ["clothes/grinsegold_full_beard/*.mhclo"]), ("Clothes", ["clothes/elvs_male_tankshirt1/*.mhclo"]),
         ("Clothes", ["clothes/cortu_cargo_pants/*.mhclo"])]
for kind, patterns in parts:
    path = asset(*patterns)
    if path:
        try:
            made = HumanService.add_mhclo_asset(path, base, asset_type=kind, subdiv_levels=0, material_type="MAKESKIN")
            print("NH added", kind, os.path.basename(path), "->", made.name if made else None)
        except Exception as error:
            print("NH FAILED", kind, path, repr(error)[:300])

meshes = [o for o in bpy.data.objects if o.type == "MESH"]
print("NH height", round(base.dimensions.z, 3), "m; meshes", [(o.name, len(o.data.vertices), [m.name for m in o.data.materials if m]) for o in meshes])
print("NH bones", len(rig.data.bones) if rig else None)

# ---- a preview picture, front and three-quarter
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x, scene.render.resolution_y = 900, 1200
scene.render.film_transparent = False
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); scene.collection.objects.link(cam); scene.camera = cam
cam.data.lens = 70
height = base.dimensions.z
for name, angle, look, dist in (("front", 0.0, 0.52, 5.2), ("side", 0.9, 0.52, 5.2), ("face", 0.35, 0.92, 1.5)):
    cam.location = (math.sin(angle) * dist, -math.cos(angle) * dist, height * look)
    cam.rotation_euler = (math.radians(90), 0.0, angle)
    scene.render.filepath = os.path.join(out, f"preview_{name}.png")
    bpy.ops.render.render(write_still=True)
print("NH previews in", out)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "player.blend"))
if preview_only:
    sys.exit(0)

# ---- one mesh on the skeleton, in centimetres with no scale left on anything, as Unreal wants it
TargetService.bake_targets(base)
for o in meshes:
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True); bpy.context.view_layer.objects.active = o
    if o.data.shape_keys:
        bpy.ops.object.shape_key_remove(all=True, apply_mix=True)
    for m in list(o.modifiers):
        if m.type == "MASK":
            bpy.ops.object.modifier_apply(modifier=m.name)
        elif m.type != "ARMATURE":
            o.modifiers.remove(m)
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = base
bpy.ops.object.join()
base.name = "Naija"
rig.name = "Armature"  # Unreal drops an armature object of this name instead of making it an extra root bone
scene.unit_settings.scale_length = 0.01
bpy.ops.object.select_all(action="DESELECT")
for o in (rig, base):
    o.parent = None if o is rig else o.parent
    o.select_set(True)
bpy.context.view_layer.objects.active = rig
rig.scale = (100.0, 100.0, 100.0)
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
print("NH joined", len(base.data.vertices), "vertices,", len(base.data.polygons), "faces,", [m.name for m in base.data.materials], "size", [round(v) for v in base.dimensions], "scales", list(rig.scale), list(base.scale))
bpy.ops.preferences.addon_enable(module="io_scene_fbx")
bpy.ops.export_scene.fbx(filepath=os.path.join(out, "Naija.fbx"), use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
                         mesh_smooth_type="FACE", use_mesh_modifiers=False, path_mode="COPY", embed_textures=True, apply_scale_options="FBX_SCALE_NONE")
print("NH exported", os.path.join(out, "Naija.fbx"), os.path.getsize(os.path.join(out, "Naija.fbx")) // 1000, "KB")
