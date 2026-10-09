"""Brings the wardrobe built by build_people_makehuman.py (its "wardrobe" mode) into Unreal.

Run inside the Unreal Editor with:

  NH_WARDROBE   the build's out folder (it holds wardrobe.json, _textures/ and <name>/Wardrobe/*.fbx)
  NH_PEOPLE     optional: comma-separated names, to do only those

Each person must already be in the project as a one-piece character (import_player_gltf.py and
setup_player_materials.py), because the pieces go onto that character's skeleton and the body reuses its skin,
eye, brow, lash and teeth materials. What it makes:

  /Game/Characters/Wardrobe/Textures, Materials        one texture and material per piece, shared by everyone
  /Game/Characters/Player/<name>/Wardrobe/Body_<name>   the body alone, its skin in one slot per patch clothes cover
  /Game/Characters/Player/<name>/Wardrobe/<Slot>_<Piece>[_D]   every piece, fitted to that body
  Data/wardrobe.json in the plugin                      the patches of skin each piece covers on each body

UNHOutfitComponent finds the pieces by those names. Safe to run again: what is there is kept.
"""
import json
import os
import unreal

root = os.environ.get("NH_WARDROBE", "")
listing = json.load(open(os.path.join(root, "wardrobe.json")))
only = [n for n in os.environ.get("NH_PEOPLE", "").split(",") if n]
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
SHARED = "/Game/Characters/Wardrobe"
PLAYER = "/Game/Characters/Player"


def run(filename, destination, name, options=None):
    task = unreal.AssetImportTask()
    for key, value in {"filename": filename, "destination_path": destination, "destination_name": name, "automated": True,
                       "replace_existing": True, "save": False}.items():
        task.set_editor_property(key, value)
    if options:
        task.set_editor_property("options", options)
    tools.import_asset_tasks([task])


def mesh_options(skeleton):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("create_physics_asset", False)
    ui.set_editor_property("skeleton", skeleton)
    ui.get_editor_property("skeletal_mesh_import_data").set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def hidden_material():
    """Draws nothing: the body's skin slots take it where clothes cover them."""
    path = f"{PLAYER}/M_NHPlayerHidden"
    if library.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = tools.create_asset("M_NHPlayerHidden", PLAYER, unreal.Material, unreal.MaterialFactoryNew())
    zero = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 0)
    zero.set_editor_property("r", 0.0)
    mel.connect_material_property(zero, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(zero, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(mat)
    library.save_loaded_asset(mat)
    return mat


def colourable(masked):
    """The player's two parent materials, with a Wash added: at 1 the texture goes grey before the Tint, so that a
    piece can be given any colour whatever colour it was made in. Rebuilt once; the instances keep their values."""
    mat = unreal.load_asset(f"{PLAYER}/M_NHPlayer{'Masked' if masked else ''}")
    if "Wash" in [str(n) for n in mel.get_scalar_parameter_names(mat)]:
        return
    mel.delete_all_material_expressions(mat)
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0)
    tex.set_editor_property("parameter_name", "BaseColor")
    tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    wash = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, 250)
    wash.set_editor_property("parameter_name", "Wash")
    wash.set_editor_property("default_value", 0.0)
    grey = mel.create_material_expression(mat, unreal.MaterialExpressionDesaturation, -600, 100)
    mel.connect_material_expressions(tex, "RGB", grey, "")
    mel.connect_material_expressions(wash, "", grey, "Fraction")
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 350)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 100)
    mel.connect_material_expressions(grey, "", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -350, 350)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.7)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if masked:
        mel.connect_material_property(tex, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(mat)
    library.save_loaded_asset(mat)
    unreal.log(f"NAIJA HUSTLE: wardrobe: {mat.get_name()} can now be washed and tinted")


def shared_material(name):
    """The piece's material: the player's parent material with the piece's texture, masked where the name says NHM_."""
    path = f"{SHARED}/Materials/{name}"
    if library.does_asset_exist(path):
        return unreal.load_asset(path)
    texture_path = f"{SHARED}/Textures/T_{name}"
    if not library.does_asset_exist(texture_path):
        source = os.path.join(root, "_textures", name + ".png")
        if not os.path.isfile(source):
            unreal.log_warning(f"NAIJA HUSTLE: wardrobe: no texture {source}")
            return None
        run(source, f"{SHARED}/Textures", "T_" + name)
        library.save_asset(texture_path, only_if_is_dirty=False)
    parent = unreal.load_asset(f"{PLAYER}/M_NHPlayer{'Masked' if name.startswith('NHM_') else ''}")
    inst = tools.create_asset(name, f"{SHARED}/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(inst, parent)
    mel.set_material_instance_texture_parameter_value(inst, "BaseColor", unreal.load_asset(texture_path))
    mel.set_material_instance_scalar_parameter_value(inst, "Roughness", 0.8)
    library.save_loaded_asset(inst)
    return inst


def own_material(person, slot):
    """A body slot's material, from the one-piece character: every NH_skin_<area> slot shows NH_skin."""
    name = "NH_skin" if slot.startswith("NH_skin") else slot
    for candidate in (name + "1", name):  # the import put a 1 on whichever of the texture and material came second
        asset = unreal.load_asset(f"{PLAYER}/{person}/{candidate}") if library.does_asset_exist(f"{PLAYER}/{person}/{candidate}") else None
        if isinstance(asset, unreal.MaterialInterface):
            return asset
    return None


def put_materials(mesh, pick):
    slots = mesh.get_editor_property("materials")
    missing = []
    for i in range(len(slots)):  # a slot read from the array is a copy: change it and put it back
        slot = slots[i]
        material = pick(str(slot.material_slot_name))
        if material:
            slot.set_editor_property("material_interface", material)
            slots[i] = slot
        else:
            missing.append(str(slot.material_slot_name))
    mesh.set_editor_property("materials", slots)
    return missing


hidden_material()
colourable(False)
colourable(True)
for person, entry in listing.items():
    if only and person not in only:
        continue
    skeleton_path = f"{PLAYER}/{person}/{person}_Skeleton"
    if not library.does_asset_exist(skeleton_path):
        unreal.log_warning(f"NAIJA HUSTLE: wardrobe: {person} is not in the project yet (run import_player_gltf.py for them first)")
        continue
    skeleton = unreal.load_asset(skeleton_path)
    dest = f"{PLAYER}/{person}/Wardrobe"
    made = 0
    registry.scan_paths_synchronous([dest], True)
    there = {str(a.asset_name): a for a in registry.get_assets_by_path(dest, recursive=False)}
    for piece in [entry["body"]] + entry["pieces"]:
        path = f"{dest}/{piece['asset']}"
        if piece is entry["body"]:
            # the body is brought in again when its skin is split differently from the one in the project
            old = unreal.load_asset(path) if library.does_asset_exist(path) else None
            if old and sorted(str(m.material_slot_name) for m in old.get_editor_property("materials")) == sorted(piece["materials"]):
                continue
            if old:
                del old
                library.delete_asset(path)
        elif any(n == piece["asset"] or n.startswith(piece["asset"].removesuffix("_D") + "_") for n in there):
            continue  # there already, perhaps under a name from an earlier build
        run(os.path.join(root, piece["file"]), dest, piece["asset"], mesh_options(skeleton))
        mesh = unreal.load_asset(path) if library.does_asset_exist(path) else None
        if not isinstance(mesh, unreal.SkeletalMesh):
            unreal.log_warning(f"NAIJA HUSTLE: wardrobe: {piece['file']} did not import as a skeletal mesh")
            continue
        body = piece is entry["body"]
        missing = put_materials(mesh, (lambda slot: own_material(person, slot)) if body else shared_material)
        if missing:
            unreal.log_warning(f"NAIJA HUSTLE: wardrobe: {person} {piece['asset']}: no material for {missing}")
        library.save_loaded_asset(mesh, only_if_is_dirty=False)  # a property set from Python does not always mark the asset as changed
        made += 1
    library.save_directory(SHARED, only_if_is_dirty=True, recursive=True)
    library.save_directory(dest, only_if_is_dirty=True, recursive=True)
    unreal.log(f"NAIJA HUSTLE: WARDROBE {person}: {made} new, {len(entry['pieces'])} pieces in all")

# what each piece covers on each body, for UNHOutfitComponent
covers = {person: {piece["piece"]: piece["covers"] for piece in entry["pieces"]} for person, entry in listing.items()}
target = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_plugins_dir()), "NaijaHustleGame", "Data", "wardrobe.json")
json.dump(covers, open(target, "w"), indent=1, sort_keys=True)
unreal.log(f"NAIJA HUSTLE: wardrobe: wrote {target}")
