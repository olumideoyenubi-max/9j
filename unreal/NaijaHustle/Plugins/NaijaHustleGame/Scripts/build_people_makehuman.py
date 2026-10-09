"""Builds the extra player skins with MPFB (MakeHuman) in Blender: men and women of different descent, each with
their own hair and clothes, and exports one FBX each for Unreal.

  blender -b --python build_people_makehuman.py -- <out folder> [preview] [name ...]

With no names it builds everyone in PEOPLE. Each gets <out>/<name>/<name>.fbx, its textures and
preview_front.png / preview_face.png; "preview" stops after the pictures. Needs the MPFB extension and, in its
user data folder, the skins, hair and clothes named in PEOPLE (ASSETS.md says which pack each comes from).

Every material is renamed NH_<part>, or NHM_<part> where its texture has see-through areas, and its texture is
saved under the same name with any tint already in it: setup_player_materials.py connects them by that name.
Then for each: import_player_gltf.py (it takes the FBX) and setup_player_materials.py.
"""
import bpy, glob, math, os, re, sys
import numpy as np
from bl_ext.blender_org.mpfb.services.humanservice import HumanService
from bl_ext.blender_org.mpfb.services.targetservice import TargetService
from bl_ext.blender_org.mpfb.services.locationservice import LocationService

BLACK, DARK_BROWN, BROWN, BLONDE = (0.03, 0.026, 0.024), (0.07, 0.045, 0.03), (0.2, 0.12, 0.06), (0.72, 0.55, 0.3)
# gender 1 is a man, 0 a woman; race is (asian, caucasian, african); clothes are listed inside out
PEOPLE = {
    "Tunde": dict(gender=1.0, race=(0, 0, 1), age=0.45, muscle=0.7, weight=0.5, skin="young_african_male", hair="afro01", tint=BLACK,
                  clothes=["male_casualsuit01", "shoes01"]),
    "Emeka": dict(gender=1.0, race=(0, 0, 1), age=0.62, muscle=0.55, weight=0.65, skin="middleage_african_male", hair="short04", tint=BLACK,
                  clothes=["grinsegold_moustache", "male_elegantsuit01", "shoes02"]),
    "Dayo": dict(gender=1.0, race=(0, 0, 1), age=0.4, muscle=0.6, weight=0.4, skin="young_african_male", hair="elvs_braided_rows", tint=BLACK,
                 clothes=["drednicolson_short-tail_camo_tee", "cortu_jeans_shorts", "shoes03"]),
    "Mark": dict(gender=1.0, race=(0, 1, 0), age=0.5, muscle=0.6, weight=0.5, skin="young_caucasian_male", hair="short02", tint=BROWN,
                 clothes=["male_casualsuit03", "shoes01"]),
    "Chen": dict(gender=1.0, race=(1, 0, 0), age=0.45, muscle=0.5, weight=0.45, skin="young_asian_male", hair="short03", tint=BLACK,
                 clothes=["male_casualsuit05", "shoes03"]),
    "Amaka": dict(gender=0.0, race=(0, 0, 1), age=0.42, muscle=0.5, weight=0.5, skin="young_african_female", hair="braid01", tint=BLACK,
                  clothes=["elvs_ladies_tank1", "toigo_harem_pants", "shoes04"]),
    "Zainab": dict(gender=0.0, race=(0, 0, 1), age=0.45, muscle=0.5, weight=0.55, skin="young_african_female", hair="elvs_micky_afro", tint=BLACK,
                   clothes=["female_casualsuit02", "shoes05"]),
    "Ngozi": dict(gender=0.0, race=(0, 0, 1), age=0.4, muscle=0.7, weight=0.45, skin="middleage_african_female", hair="short01", tint=BLACK,
                  clothes=["female_sportsuit01", "shoes06"]),
    "Kate": dict(gender=0.0, race=(0, 1, 0), age=0.45, muscle=0.5, weight=0.45, skin="young_caucasian_female", hair="ponytail01", tint=BLONDE,
                 clothes=["female_elegantsuit01", "shoes04"]),
    "Mei": dict(gender=0.0, race=(1, 0, 0), age=0.42, muscle=0.45, weight=0.42, skin="young_asian_female", hair="bob02", tint=BLACK,
                clothes=["punkduck_v_neck_top", "toigo_wool_pants", "shoes05"]),
    "Priya": dict(gender=0.0, race=(0.35, 0.65, 0), age=0.45, muscle=0.5, weight=0.5, skin="cutoff3d_indian_female_skin", hair="long01", tint=DARK_BROWN,
                  clothes=["drednicolson_asymmetric_tunic_and_sash", "toigo_harem_pants", "shoes04"]),
}

args = sys.argv[sys.argv.index("--") + 1:]
out_root = args[0]
preview_only = "preview" in args
names = [a for a in args[1:] if a in PEOPLE] or list(PEOPLE)
data = None


def asset(kind, name):
    found = sorted(glob.glob(os.path.join(data, kind, name, "*.mhmat" if kind == "skins" else "*.mhclo")))
    if not found:
        print("NH missing asset", kind, name)
    return found[0] if found else None


def base_image(material):
    """The material's colour texture. MPFB mixes it with a shading map on the way to the shader: connect it straight
    to the base colour so that the preview and the FBX both use it alone."""
    if not material or not material.use_nodes:
        return None
    nodes = material.node_tree.nodes
    shader = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
    node = nodes.get("diffuseTexture")
    if not shader or not node or node.type != "TEX_IMAGE" or not node.image:
        return None
    material.node_tree.links.new(node.outputs["Color"], shader.inputs["Base Color"])
    nodes.active = node  # the preview draws the active texture
    return node.image


def flat_colour(material):
    shader = next((n for n in material.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None) if material and material.use_nodes else None
    return tuple(shader.inputs["Base Color"].default_value[:3]) if shader else (0.5, 0.5, 0.5)


def build(name, who):
    out = os.path.join(out_root, name)
    os.makedirs(os.path.join(out, "textures"), exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.preferences.addon_enable(module="bl_ext.blender_org.mpfb")
    global data
    data = LocationService.get_user_data()
    macro = TargetService.get_default_macro_info_dict()
    macro.update({"gender": who["gender"], "age": who["age"], "muscle": who["muscle"], "weight": who["weight"], "proportions": 0.7, "height": 0.5})
    macro["race"] = dict(zip(("asian", "caucasian", "african"), who["race"]))
    base = HumanService.create_human(scale=0.1, macro_detail_dict=macro)
    base.name = "Body"
    rig = HumanService.add_builtin_rig(base, "game_engine")

    # The Unreal mannequin's animation Blueprint plants the feet using IK bones, which this rig does not have: add
    # them, each sitting on the bone it shadows (the retarget pins them there).
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

    skin = asset("skins", who["skin"])
    if skin:
        HumanService.set_character_skin(skin, base, skin_type="MAKESKIN", material_instances=False)
    # object -> (part name for its material, tint or None)
    parts = {base: ("skin", None)}
    wanted = [("Eyes", "eyes", "low-poly", None), ("Eyebrows", "eyebrows", "eyebrow001", who["tint"]), ("Eyelashes", "eyelashes", "eyelashes01", None),
              ("Teeth", "teeth", "teeth_base", None), ("Hair", "hair", who["hair"], who["tint"])]
    wanted += [("Clothes", "clothes", c, who["tint"] if "beard" in c else None) for c in who["clothes"]]
    for kind, folder, item, tint in wanted:
        path = asset(folder, item)
        if not path:
            continue
        try:
            made = HumanService.add_mhclo_asset(path, base, asset_type=kind, subdiv_levels=0, material_type="MAKESKIN")
            parts[made] = (re.sub(r"[^a-z0-9]", "", item.lower()), tint)
        except Exception as error:
            print("NH FAILED", name, kind, item, repr(error)[:300])

    # one texture per material, under the material's new name, tinted here so that Unreal needs no tint
    for o, (part, tint) in parts.items():
        for material in [m for m in o.data.materials if m]:
            image = base_image(material)
            if image and image.size[0]:
                limit = 2048 if part == "skin" else 1024
                if max(image.size) > limit:
                    image.scale(*(max(1, round(s * limit / max(image.size))) for s in image.size))
                px = np.empty(image.size[0] * image.size[1] * 4, dtype=np.float32)
                image.pixels.foreach_get(px)
                px = px.reshape(-1, 4)
                # the eye and teeth textures have clear areas too, but those parts must stay solid
                masked = part not in ("skin", "lowpoly", "teethbase") and image.channels == 4 and float((px[:, 3] < 0.5).mean()) > 0.002
                if tint:
                    solid = px[:, 3] > 0.5 if masked else np.ones(len(px), dtype=bool)
                    grey = px[:, :3].mean(axis=1)
                    grey = np.clip(grey / max(float(grey[solid].mean()), 1e-4), 0.3, 2.2)
                    px[:, :3] = np.clip(grey[:, None] * np.array(tint, dtype=np.float32), 0, 1)
                    image.pixels.foreach_set(px.ravel())
            else:  # no texture: a small flat one in the material's colour, or the tint
                masked = False
                image = bpy.data.images.new(part, 8, 8)
                image.generated_color = (*(tint or flat_colour(material)), 1.0)
                material.use_nodes = True
                shader = next(n for n in material.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
                node = material.node_tree.nodes.new("ShaderNodeTexImage")
                node.image = image
                material.node_tree.nodes.active = node
                material.node_tree.links.new(node.outputs["Color"], shader.inputs["Base Color"])
            material.name = f"NH{'M' if masked else ''}_{part}"
            image.name = material.name
            image.filepath_raw = os.path.join(out, "textures", material.name + ".png")
            image.file_format = "PNG"
            image.save()

    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    print("NH", name, "height", round(base.dimensions.z, 3), "m; parts", [(o.name, len(o.data.vertices), [m.name for m in o.data.materials if m]) for o in meshes])

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 600, 800
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); scene.collection.objects.link(cam); scene.camera = cam
    cam.data.lens = 70
    height = base.dimensions.z
    for view, angle, look, dist in (("front", 0.35, 0.52, 5.2), ("face", 0.35, 0.92, 1.5)):
        cam.location = (math.sin(angle) * dist, -math.cos(angle) * dist, height * look)
        cam.rotation_euler = (math.radians(90), 0.0, angle)
        scene.render.filepath = os.path.join(out, f"preview_{view}.png")
        bpy.ops.render.render(write_still=True)
    if preview_only:
        return

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
    base.name = name
    rig.name = "Armature"  # Unreal drops an armature object of this name instead of making it an extra root bone
    scene.unit_settings.scale_length = 0.01
    bpy.ops.object.select_all(action="DESELECT")
    for o in (rig, base):
        o.parent = None if o is rig else o.parent
        o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    rig.scale = (100.0, 100.0, 100.0)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bpy.ops.preferences.addon_enable(module="io_scene_fbx")
    fbx = os.path.join(out, f"{name}.fbx")
    bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
                             mesh_smooth_type="FACE", use_mesh_modifiers=False, path_mode="COPY", embed_textures=True, apply_scale_options="FBX_SCALE_NONE")
    print("NH", name, "exported", fbx, os.path.getsize(fbx) // 1000, "KB,", len(base.data.polygons), "faces,", round(base.dimensions.z), "cm,", [m.name for m in base.data.materials])


for person in names:
    build(person, PEOPLE[person])
