"""Builds the extra player skins with MPFB (MakeHuman) in Blender: men and women of different descent, each with
their own hair and clothes, and exports them for Unreal.

  blender -b --python build_people_makehuman.py -- <out folder> [preview] [wardrobe] [name ...]

With no names it builds everyone in PEOPLE. Needs the MPFB extension and, in its user data folder, the skins, hair
and clothes named in PEOPLE and WARDROBE (ASSETS.md says which pack each comes from).

Without "wardrobe": one FBX per person with hair and clothes joined on, <out>/<name>/<name>.fbx, its textures and
preview_front.png / preview_face.png; "preview" stops after the pictures. Then for each: import_player_gltf.py
(it takes the FBX) and setup_player_materials.py.

With "wardrobe": the same people in pieces, for clothes that can be changed in play. <out>/<name>/Wardrobe/ gets
Body_<name>.fbx (no hair or clothes, its skin split into the patches clothes cover) and one FBX for every piece in
WARDROBE that fits the person, named <Slot>_<Piece>[_D], _D marking what the person starts in. Textures shared by
everyone go to <out>/_textures and <out>/wardrobe.json lists it all, with the patches of skin each piece covers,
for import_wardrobe.py.

Every material is renamed NH_<part>, or NHM_<part> where its texture has see-through areas, and its texture is
saved under the same name with any tint already in it.
"""
import bpy, glob, json, math, os, re, sys
import numpy as np
from bl_ext.blender_org.mpfb.services.humanservice import HumanService
from bl_ext.blender_org.mpfb.services.targetservice import TargetService
from bl_ext.blender_org.mpfb.services.locationservice import LocationService

BLACK, DARK_BROWN, BROWN, BLONDE, GREY = (0.03, 0.026, 0.024), (0.07, 0.045, 0.03), (0.2, 0.12, 0.06), (0.72, 0.55, 0.3), (0.62, 0.62, 0.6)
TINT_NAMES = {BLACK: "Black", DARK_BROWN: "DarkBrown", BROWN: "Brown", BLONDE: "Blonde", GREY: "Grey"}
# gender 1 is a man, 0 a woman; race is (asian, caucasian, african); clothes are what the one-piece character wears, inside
# out; wear is what the wardrobe starts on (hair, top or full outfit, bottom, shoes); face is hair that stays on the body
PEOPLE = {
    "Tunde": dict(gender=1.0, race=(0, 0, 1), age=0.45, muscle=0.7, weight=0.5, skin="young_african_male", hair="afro01", tint=BLACK,
                  clothes=["male_casualsuit01", "shoes01"], wear=("Afro", "DenimSet", None, "BrownLeather")),
    "Emeka": dict(gender=1.0, race=(0, 0, 1), age=0.62, muscle=0.55, weight=0.65, skin="middleage_african_male", hair="short04", tint=BLACK,
                  clothes=["male_elegantsuit01", "shoes02"], wear=("Fade", "Suit", None, "GreyLeather"), face=["grinsegold_moustache"]),
    # Baba Driver: an old man. MakeHuman's oldest age, little muscle, grey hair and a grey beard. There is no old skin in
    # the packs, so his is the middle-aged one. ANHPerson casts him wherever a part needs an elderly man (ENHCast::ElderMan).
    "Baba": dict(gender=1.0, race=(0, 0, 1), age=0.95, muscle=0.3, weight=0.6, skin="middleage_african_male", hair="short04", tint=GREY,
                 clothes=["elvs_male_shirt_untucked_bd1", "elvs_male_trouser", "elvs_male_flip_flop_sandals1"],
                 wear=("Fade", "UntuckedShirt", "SmartTrousers", "Slippers"), face=["grinsegold_full_beard"]),
    "Dayo": dict(gender=1.0, race=(0, 0, 1), age=0.4, muscle=0.6, weight=0.4, skin="young_african_male", hair="elvs_braided_rows", tint=BLACK,
                 clothes=["drednicolson_short-tail_camo_tee", "cortu_jeans_shorts", "shoes03"], wear=("Cornrows", "CamoTee", "DenimShorts", "BlackBoots")),
    "Mark": dict(gender=1.0, race=(0, 1, 0), age=0.5, muscle=0.6, weight=0.5, skin="young_caucasian_male", hair="short02", tint=BROWN,
                 clothes=["male_casualsuit03", "shoes01"], wear=("ShortCrop", "StripedShirtSet", None, "BrownLeather")),
    "Chen": dict(gender=1.0, race=(1, 0, 0), age=0.45, muscle=0.5, weight=0.45, skin="young_asian_male", hair="short03", tint=BLACK,
                 clothes=["male_casualsuit05", "shoes03"], wear=("SideFringe", "JacketSet", None, "BlackBoots")),
    "Amaka": dict(gender=0.0, race=(0, 0, 1), age=0.42, muscle=0.5, weight=0.5, skin="young_african_female", hair="braid01", tint=BLACK,
                  clothes=["elvs_ladies_tank1", "toigo_harem_pants", "shoes04"], wear=("SideSwept", "PrintTank", "HaremTrousers", "BlackFlats")),
    "Zainab": dict(gender=0.0, race=(0, 0, 1), age=0.45, muscle=0.5, weight=0.55, skin="young_african_female", hair="elvs_micky_afro", tint=BLACK,
                   clothes=["female_casualsuit02", "shoes05"], wear=("AfroPuffs", "TeeAndShorts", None, "GreyTrainers")),
    "Ngozi": dict(gender=0.0, race=(0, 0, 1), age=0.4, muscle=0.7, weight=0.45, skin="middleage_african_female", hair="short01", tint=BLACK,
                  clothes=["female_sportsuit01", "shoes06"], wear=("LowCut", "GymSet", None, "BlueTrainers")),
    "Kate": dict(gender=0.0, race=(0, 1, 0), age=0.45, muscle=0.5, weight=0.45, skin="young_caucasian_female", hair="ponytail01", tint=BLONDE,
                 clothes=["female_elegantsuit01", "shoes04"], wear=("Ponytail", "OfficeSet", None, "BlackFlats")),
    "Mei": dict(gender=0.0, race=(1, 0, 0), age=0.42, muscle=0.45, weight=0.42, skin="young_asian_female", hair="bob02", tint=BLACK,
                clothes=["punkduck_v_neck_top", "toigo_wool_pants", "shoes05"], wear=("LongBob", "VNeckTop", "WoolTrousers", "GreyTrainers")),
    "Priya": dict(gender=0.0, race=(0.35, 0.65, 0), age=0.45, muscle=0.5, weight=0.5, skin="cutoff3d_indian_female_skin", hair="long01", tint=DARK_BROWN,
                  clothes=["drednicolson_asymmetric_tunic_and_sash", "toigo_harem_pants", "shoes04"], wear=("Long", "Tunic", "HaremTrousers", "BlackFlats")),
}


# piece: (slot, MakeHuman folder, who it fits: m, f or u for both). Full is a top and bottom in one.
WARDROBE = {
    "LowCut": ("Hair", "short01", "u"), "ShortCrop": ("Hair", "short02", "m"), "SideFringe": ("Hair", "short03", "u"), "Fade": ("Hair", "short04", "m"),
    "Afro": ("Hair", "afro01", "u"), "Cornrows": ("Hair", "elvs_braided_rows", "u"), "AfroPuffs": ("Hair", "elvs_micky_afro", "f"),
    "SideSwept": ("Hair", "braid01", "f"), "Ponytail": ("Hair", "ponytail01", "f"), "Bob": ("Hair", "bob01", "f"), "LongBob": ("Hair", "bob02", "f"),
    "Long": ("Hair", "long01", "f"), "BraidBun": ("Hair", "elvs_braid_bun", "f"), "BigCurls": ("Hair", "punkduck_alpha7_curly", "f"),

    "Singlet": ("Top", "elvs_male_tankshirt1", "m"), "LogoTee": ("Top", "elvs_male_logo_tshirt1", "m"),
    "MuscleShirt": ("Top", "elvs_male_muscle_shirt1", "m"), "CamoTee": ("Top", "drednicolson_short-tail_camo_tee", "m"),
    "Polo": ("Top", "namuhekam_male_polo_shirt", "m"), "PlainTee": ("Top", "elvs_crude_t-shirt_male", "m"),
    "UntuckedShirt": ("Top", "elvs_male_shirt_untucked_bd1", "m"), "Hoodie": ("Top", "elvs_hooded_sweat_jacket1", "m"),
    "BohoTop": ("Top", "elvs_male_boho_top1", "m"), "ShirtAndTie": ("Top", "elvs_male_shirt_tie_tucked1", "m"),
    "PrintTank": ("Top", "elvs_ladies_tank1", "f"), "LaraTank": ("Top", "elvs_lara_tank1", "f"),
    "PeasantBlouse": ("Top", "elvs_ruffle_sleeve_peasant_blouse_1", "f"), "DiscoTop": ("Top", "elvs_disco_top2", "f"),
    "CropTop": ("Top", "punkduck_high_neck_crop_top", "f"), "RetroTop": ("Top", "punkduck_retro_top", "f"), "VNeckTop": ("Top", "punkduck_v_neck_top", "f"),
    "SleevelessShirt": ("Top", "punkduck_sleeveless_shirt", "f"), "Tunic": ("Top", "drednicolson_asymmetric_tunic_and_sash", "f"),
    "TuckedTee": ("Top", "toigo_basic_tucked_t-shirt", "f"), "Camisole": ("Top", "toigo_camisole_top", "f"), "RoundNeckTee": ("Top", "joepal_crude_t-shirt_female", "f"),
    "LaceUpBlouse": ("Top", "punkduck_lace_up_blouse", "f"), "OffShoulderTop": ("Top", "punkduck_off-shoulder_long-sleeve_top", "f"),

    "CargoTrousers": ("Bottom", "cortu_cargo_pants", "m"), "DenimShorts": ("Bottom", "cortu_jeans_shorts", "u"),
    "ClassicJeans": ("Bottom", "punkduck_male_classic_jeans", "m"), "Chinos": ("Bottom", "mindfront_male_trousers_1", "m"),
    "SmartTrousers": ("Bottom", "elvs_male_trouser", "m"), "Shorts": ("Bottom", "elvs_male_trouser_short_1", "m"),
    "HaremTrousers": ("Bottom", "toigo_harem_pants", "f"), "WoolTrousers": ("Bottom", "toigo_wool_pants", "f"),
    "TightJeans": ("Bottom", "punkduck_female_tight_jeans", "f"), "Slacks": ("Bottom", "mindfront_female_trousers_1", "f"),
    "BootcutJeans": ("Bottom", "elvs_jeans_bootcut", "f"), "ShortJeans": ("Bottom", "punkduck_female_short_jeans", "f"),

    "DenimSet": ("Full", "male_casualsuit01", "m"), "LongSleeveSet": ("Full", "male_casualsuit02", "m"), "StripedShirtSet": ("Full", "male_casualsuit03", "m"),
    "BlueTeeSet": ("Full", "male_casualsuit04", "m"), "JacketSet": ("Full", "male_casualsuit05", "m"), "WeekendSet": ("Full", "male_casualsuit06", "m"),
    "Suit": ("Full", "male_elegantsuit01", "m"), "Overalls": ("Full", "male_worksuit01", "m"),
    "TeeAndJeans": ("Full", "female_casualsuit01", "f"), "TeeAndShorts": ("Full", "female_casualsuit02", "f"), "GymSet": ("Full", "female_sportsuit01", "f"),
    "OfficeSet": ("Full", "female_elegantsuit01", "f"), "ShiftDress": ("Full", "toigo_shift_dress", "f"), "HalterDress": ("Full", "toigo_halter_dress_midi", "f"),
    "TieredDress": ("Full", "toigo_dress_with_tiered_skirt", "f"), "KeyholeDress": ("Full", "toigo_keyhole_neck_dress", "f"),
    "FashionDress": ("Full", "elvs_simple_fashion_dress_1", "f"), "SixtiesDress": ("Full", "elvs_simple_60s_dress", "f"),
    "LongDress": ("Full", "elvs_halter_dress_long", "f"), "DayDress": ("Full", "mindfront_f_dress_02", "f"), "PartyDress": ("Full", "mindfront_f_dress_05", "f"),

    "BrownLeather": ("Shoes", "shoes01", "m"), "GreyLeather": ("Shoes", "shoes02", "m"), "BlackBoots": ("Shoes", "shoes03", "u"), "BlackFlats": ("Shoes", "shoes04", "f"),
    "GreyTrainers": ("Shoes", "shoes05", "u"), "BlueTrainers": ("Shoes", "shoes06", "u"), "RunningShoes": ("Shoes", "punkduck_running_shoes_01", "u"),
    "Sneakers": ("Shoes", "culturalibre_sneakers", "u"), "Slippers": ("Shoes", "elvs_male_flip_flop_sandals1", "u"), "Sandals": ("Shoes", "dressupdoc_sandals1", "f"),
    "Oxfords": ("Shoes", "mindfront_shoes_oxford_male", "m"), "AnkleBoots": ("Shoes", "toigo_ankle_boots_male", "m"),
}
# How many patches the body's skin is split into, so that the game can hide what a piece covers, and their names
PATCHES = 30
PATCH_NAMES = "ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789"

args = sys.argv[sys.argv.index("--") + 1:]
out_root = args[0]
preview_only = "preview" in args
wardrobe = "wardrobe" in args
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


def finish_materials(o, part, tint, folder, solid=False):
    """Gives each of the object's materials one texture under the material's new name, tinted here so that Unreal
    needs no tint. Returns the new material names."""
    made = []
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
            masked = not solid and image.channels == 4 and float((px[:, 3] < 0.5).mean()) > 0.002
            if tint:
                opaque = px[:, 3] > 0.5 if masked else np.ones(len(px), dtype=bool)
                grey = px[:, :3].mean(axis=1)
                grey = np.clip(grey / max(float(grey[opaque].mean()), 1e-4), 0.3, 2.2)
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
        material.name = f"NH{'M' if masked else ''}_{part}" + (f"_{len(made)}" if made else "")
        image.name = material.name
        image.filepath_raw = os.path.join(folder, material.name + ".png")
        image.file_format = "PNG"
        if not (wardrobe and part != "skin" and os.path.isfile(image.filepath_raw)):  # shared textures are written once
            image.save()
        made.append(material.name)
    return made


def make_human(who):
    """A bare body with its skin on the game_engine rig, and the IK bones the Unreal mannequin's animations need."""
    global data
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.preferences.addon_enable(module="bl_ext.blender_org.mpfb")
    data = LocationService.get_user_data()
    macro = TargetService.get_default_macro_info_dict()
    macro.update({"gender": who["gender"], "age": who["age"], "muscle": who["muscle"], "weight": who["weight"], "proportions": 0.7, "height": 0.5})
    macro["race"] = dict(zip(("asian", "caucasian", "african"), who["race"]))
    base = HumanService.create_human(scale=0.1, macro_detail_dict=macro)
    base.name = "Body"
    rig = HumanService.add_builtin_rig(base, "game_engine")

    # The mannequin's animation Blueprint plants the feet using IK bones, which this rig does not have: add them,
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
    skin = asset("skins", who["skin"])
    if skin:
        HumanService.set_character_skin(skin, base, skin_type="MAKESKIN", material_instances=False)
    return base, rig


def add(base, kind, folder, item):
    path = asset(folder, item)
    if not path:
        return None
    try:
        return HumanService.add_mhclo_asset(path, base, asset_type=kind, subdiv_levels=0, material_type="MAKESKIN")
    except Exception as error:
        print("NH FAILED", kind, item, repr(error)[:300])
        return None


def pictures(out, height):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 600, 800
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); scene.collection.objects.link(cam); scene.camera = cam
    cam.data.lens = 70
    for view, angle, look, dist in (("front", 0.35, 0.52, 5.2), ("face", 0.35, 0.92, 1.5)):
        cam.location = (math.sin(angle) * dist, -math.cos(angle) * dist, height * look)
        cam.rotation_euler = (math.radians(90), 0.0, angle)
        scene.render.filepath = os.path.join(out, f"preview_{view}.png")
        bpy.ops.render.render(write_still=True)


def settle(base, rig, meshes, keep_masks=True):
    """Shape keys baked in and modifiers gone, then centimetres with no scale left on anything, as Unreal wants it."""
    TargetService.bake_targets(base)
    for o in meshes:
        bpy.ops.object.select_all(action="DESELECT")
        o.select_set(True); bpy.context.view_layer.objects.active = o
        if o.data.shape_keys:
            bpy.ops.object.shape_key_remove(all=True, apply_mix=True)
        for m in list(o.modifiers):
            if m.type == "MASK" and (keep_masks or not m.name.startswith("Delete.")):
                bpy.ops.object.modifier_apply(modifier=m.name)
            elif m.type != "ARMATURE":
                o.modifiers.remove(m)
    rig.name = "Armature"  # Unreal drops an armature object of this name instead of making it an extra root bone
    bpy.context.scene.unit_settings.scale_length = 0.01
    bpy.ops.object.select_all(action="DESELECT")
    rig.parent = None
    for o in [rig] + meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    rig.scale = (100.0, 100.0, 100.0)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)


def export(path, rig, mesh):
    bpy.ops.object.select_all(action="DESELECT")
    for o in (rig, mesh):
        o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.preferences.addon_enable(module="io_scene_fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False, bake_anim=False,
                             mesh_smooth_type="FACE", use_mesh_modifiers=False, path_mode="AUTO" if wardrobe else "COPY", embed_textures=not wardrobe, apply_scale_options="FBX_SCALE_NONE")


def face_parts(base, who):
    """Eyes, brows, lashes, teeth and any face hair: object -> (part name, tint, always solid)."""
    parts = {}
    wanted = [("Eyes", "eyes", "low-poly", None), ("Eyebrows", "eyebrows", "eyebrow001", who["tint"]), ("Eyelashes", "eyelashes", "eyelashes01", None),
              ("Teeth", "teeth", "teeth_base", None)] + [("Clothes", "clothes", f, who["tint"]) for f in who.get("face", [])]
    for kind, folder, item, tint in wanted:
        made = add(base, kind, folder, item)
        if made:
            part = re.sub(r"[^a-z0-9]", "", item.lower())
            parts[made] = (part, tint, part in ("lowpoly", "teethbase"))
    return parts


def build(name, who):
    """One mesh with hair and clothes joined on."""
    out = os.path.join(out_root, name)
    os.makedirs(os.path.join(out, "textures"), exist_ok=True)
    base, rig = make_human(who)
    parts = {base: ("skin", None, True)}
    parts.update(face_parts(base, who))
    for kind, folder, item, tint in [("Hair", "hair", who["hair"], who["tint"])] + [("Clothes", "clothes", c, None) for c in who["clothes"]]:
        made = add(base, kind, folder, item)
        if made:
            parts[made] = (re.sub(r"[^a-z0-9]", "", item.lower()), tint, False)
    for o, (part, tint, solid) in parts.items():
        finish_materials(o, part, tint, os.path.join(out, "textures"), solid)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    print("NH", name, "height", round(base.dimensions.z, 3), "m; parts", [(o.name, len(o.data.vertices), [m.name for m in o.data.materials if m]) for o in meshes])
    pictures(out, base.dimensions.z)
    if preview_only:
        return
    settle(base, rig, meshes)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = base
    bpy.ops.object.join()
    base.name = name
    fbx = os.path.join(out, f"{name}.fbx")
    export(fbx, rig, base)
    print("NH", name, "exported", fbx, os.path.getsize(fbx) // 1000, "KB,", len(base.data.polygons), "faces,", round(base.dimensions.z), "cm,", [m.name for m in base.data.materials])


def build_wardrobe(name, who, listing):
    """The same person in pieces: a body whose skin is split into areas, and every piece that fits, each on the skeleton."""
    out = os.path.join(out_root, name, "Wardrobe")
    shared = os.path.join(out_root, "_textures")
    os.makedirs(out, exist_ok=True); os.makedirs(shared, exist_ok=True)
    os.makedirs(os.path.join(out_root, name, "textures"), exist_ok=True)
    base, rig = make_human(who)
    finish_materials(base, "skin", None, os.path.join(out_root, name, "textures"), True)
    body_parts = face_parts(base, who)
    for o, (part, tint, solid) in body_parts.items():
        finish_materials(o, part, tint, os.path.join(out_root, name, "textures"), solid)

    sex = "m" if who["gender"] > 0.5 else "f"
    tint_name = TINT_NAMES[who["tint"]]
    pieces = {}  # piece -> (object, slot, materials, the body's vertex group that it hides)
    for piece, (slot, folder, fits) in WARDROBE.items():
        if fits not in ("u", sex):
            continue
        before = {g.name for g in base.vertex_groups}
        made = add(base, "Hair" if slot == "Hair" else "Clothes", "hair" if slot == "Hair" else "clothes", folder)
        if not made:
            continue
        hides = [g.name for g in base.vertex_groups if g.name not in before and g.name.startswith("Delete.")]
        part = f"{piece}_{tint_name}" if slot == "Hair" else piece
        pieces[piece] = (made, slot, finish_materials(made, part, who["tint"] if slot == "Hair" else None, shared), hides[0] if hides else None)

    # Every piece says which of the body's faces it hides. Faces hidden by the same pieces make one patch of skin, the
    # PATCHES biggest get a material slot each, and a smaller one joins the patch it differs least from.
    groups = {g.index: g.name for g in base.vertex_groups}
    delete_group = {p[3]: piece for piece, p in pieces.items() if p[3]}
    order = sorted(pieces)
    hidden = {piece: set() for piece in pieces}
    for v in base.data.vertices:
        for g in v.groups:
            if groups[g.group] in delete_group and g.weight > 0.5:
                hidden[delete_group[groups[g.group]]].add(v.index)
    signature = {polygon.index: tuple(any(i in hidden[piece] for i in polygon.vertices) for piece in order) for polygon in base.data.polygons}
    bare = tuple(False for _ in order)
    sizes = {}
    for key in signature.values():
        sizes[key] = sizes.get(key, 0) + 1
    kept = [bare] + sorted((k for k in sizes if k != bare), key=lambda k: -sizes[k])[:PATCHES]
    nearest = {k: k if k in kept else min(kept, key=lambda other: sum(a != b for a, b in zip(k, other))) for k in sizes}
    skin = base.data.materials[0]
    slot = {bare: 0}
    for number, key in enumerate(kept[1:]):
        copy = skin.copy()
        copy.name = f"{skin.name}_{PATCH_NAMES[number]}"
        base.data.materials.append(copy)
        slot[key] = len(base.data.materials) - 1
    for polygon in base.data.polygons:
        polygon.material_index = slot[nearest[signature[polygon.index]]]
    covers = {piece: "".join(PATCH_NAMES[number] for number, key in enumerate(kept[1:]) if key[at]) for at, piece in enumerate(order)}
    print("NH", name, "skin:", len(sizes), "ways of being covered, kept", len(kept) - 1, "patches;", sum(n for k, n in sizes.items() if k not in kept), "faces joined a neighbour")

    body_meshes = [base] + list(body_parts)
    piece_meshes = [p[0] for p in pieces.values()]
    settle(base, rig, body_meshes + piece_meshes, keep_masks=False)
    bpy.ops.object.select_all(action="DESELECT")
    for o in body_meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = base
    bpy.ops.object.join()
    base.name = f"Body_{name}"
    export(os.path.join(out, base.name + ".fbx"), rig, base)
    entry = {"body": {"file": f"{name}/Wardrobe/{base.name}.fbx", "asset": base.name, "materials": [m.name for m in base.data.materials], "faces": len(base.data.polygons)}, "pieces": []}
    hair, top, bottom, shoes = who["wear"]
    for piece, (o, slot_name, materials, _) in pieces.items():
        o.name = f"{slot_name}_{piece}" + ("_D" if piece in (hair, top, bottom, shoes) else "")
        export(os.path.join(out, o.name + ".fbx"), rig, o)
        entry["pieces"].append({"file": f"{name}/Wardrobe/{o.name}.fbx", "asset": o.name, "piece": piece, "covers": covers[piece], "materials": materials, "faces": len(o.data.polygons)})
    listing[name] = entry
    print("NH", name, "wardrobe:", len(pieces), "pieces;", " ".join(f"{p['asset']}:{p['faces']}" for p in entry["pieces"]))


if wardrobe:
    listing_path = os.path.join(out_root, "wardrobe.json")
    listing = json.load(open(listing_path)) if os.path.isfile(listing_path) else {}
    for person in names:
        build_wardrobe(person, PEOPLE[person], listing)
        json.dump(listing, open(listing_path, "w"), indent=1)
else:
    for person in names:
        build(person, PEOPLE[person])
