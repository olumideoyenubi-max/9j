"""Brings the modular building kit into Unreal: one static mesh a piece, all sharing a few materials.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>) after Scripts/build_kit.py, and after the surface
library (import_surfaces.py) and the trim sheets (import_trims_decals.py), whose materials the kit uses.
Environment variable:  NH_KIT   the folder of .glb pieces (default ~/Downloads/nh-kit)

What it makes, under /Game/NaijaHustle/Kit (generated, not stored in the repo):
  SM_Kit_<Piece>     the pieces. They carry no collision: the generator gives each wall one plain box instead
  M_NH_KitFlat       a plain colour, for glass, tanks and the like; MI_Kit_Glass, _Dark, _Tank, _Appliance
  MI_Kit_Wall        painted plaster from the surface library; the building's colour comes from the generator
  MI_Kit_RoofFlat    a flat concrete roof; MI_Kit_RoofZinc, _Concrete, _Door likewise from the surface library

A piece's material slots are named in build_kit.py; SLOTS below says which material each name gets.
"""
import json
import os
import unreal

SRC = os.environ.get("NH_KIT", os.path.join(os.path.expanduser("~"), "Downloads", "nh-kit"))
ROOT = "/Game/NaijaHustle/Kit"
SURFACES = "/Game/NaijaHustle/Surfaces"

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
MP = unreal.MaterialProperty


def say(text):
    unreal.log("KIT: " + text)


def flat_master():
    path = ROOT + "/M_NH_KitFlat"
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)       # built again in place, so its instances keep their parent
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_NH_KitFlat", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    colour = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
    colour.set_editor_property("parameter_name", "Colour")
    colour.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1))
    mel.connect_material_property(colour, "", MP.MP_BASE_COLOR)
    for name, value, prop, y in (("Roughness", 0.6, MP.MP_ROUGHNESS, 200), ("Metallic", 0.0, MP.MP_METALLIC, 350)):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        mel.connect_material_property(node, "", prop)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)       # the building kit draws everything as instances
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def instance(name, parent, scalars=None, colour=None):
    path = f"{ROOT}/{name}"
    inst = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(inst, parent)
    for key, value in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(inst, key, value)
    if colour:
        mel.set_material_instance_vector_parameter_value(inst, "Colour", unreal.LinearColor(colour[0], colour[1], colour[2], 1))
    mel.update_material_instance(inst)
    eal.save_loaded_asset(inst, only_if_is_dirty=False)
    return inst


def materials():
    flat = flat_master()
    surface = lambda name: eal.load_asset(f"{SURFACES}/MI_{name}")
    trim = lambda name: eal.load_asset(f"{SURFACES}/Trims/MI_Trim_{name}")
    return {
        # the kit sets each building's own colour, so the per-panel variation is turned right down
        "Wall": instance("MI_Kit_Wall", surface("Plaster_Painted"), {"TintVariation": 0.03, "GroundZ": -16.0, "Grime": 0.7, "RainStreaks": 0.18}),
        "TrimFrames": trim("Frames"), "TrimFramesCutout": trim("Frames_Cutout"), "TrimShop": trim("Shop"), "TrimShopCutout": trim("Shop_Cutout"),
        "Glass": instance("MI_Kit_Glass", flat, {"Roughness": 0.08, "Metallic": 0.7}, (0.05, 0.08, 0.1)),
        "Dark": instance("MI_Kit_Dark", flat, {"Roughness": 0.9}, (0.015, 0.014, 0.013)),
        "Tank": instance("MI_Kit_Tank", flat, {"Roughness": 0.45}, (0.02, 0.02, 0.022)),
        "Appliance": instance("MI_Kit_Appliance", flat, {"Roughness": 0.5}, (0.62, 0.62, 0.6)),
        "RoofZinc": instance("MI_Kit_RoofZinc", surface("Zinc_Rusty"), {"TintVariation": 0.1, "Grime": 0.0, "RainStreaks": 0.0}),
        "RoofFlat": instance("MI_Kit_RoofFlat", surface("Concrete_Dirty"), {"Tiling": 4.0, "Grime": 0.0, "RainStreaks": 0.0, "Dust": 0.7}),
        "Concrete": instance("MI_Kit_Concrete", surface("Concrete_Brushed"), {"GroundZ": -16.0}),
        "Door": instance("MI_Kit_Door", surface("Metal_Painted_Rusty"), {"Grime": 0.3, "RainStreaks": 0.2}),
    }


def import_piece(name, mats):
    scratch = f"{ROOT}/_Import/{name}"
    dest = f"{ROOT}/SM_Kit_{name}"
    if eal.does_directory_exist(scratch):
        eal.delete_directory(scratch)
    task = unreal.AssetImportTask()
    for key, value in {"filename": os.path.join(SRC, name + ".glb"), "destination_path": scratch, "automated": True, "replace_existing": True, "save": False}.items():
        task.set_editor_property(key, value)
    tools.import_asset_tasks([task])
    registry.scan_paths_synchronous([scratch], True)
    found = [a for a in registry.get_assets_by_path(scratch, recursive=True) if str(a.asset_class_path.asset_name) == "StaticMesh"]
    if not found:
        say(f"{name}: the import made no mesh")
        return None
    if eal.does_asset_exist(dest):
        eal.delete_asset(dest)
    eal.rename_asset(str(found[0].package_name), dest)
    mesh = eal.load_asset(dest)
    slots = mesh.get_editor_property("static_materials")
    missing = []
    for i, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        key = next((k for k in sorted(mats, key=len, reverse=True) if slot_name == k or slot_name.startswith(k)), None)
        if key:
            mesh.set_material(i, mats[key])
        else:
            missing.append(slot_name)
    eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    eal.delete_directory(scratch)
    box = mesh.get_bounding_box()
    return {"slots": [str(s.get_editor_property("material_slot_name")) for s in slots], "missing": missing,
            "min": [round(box.min.x), round(box.min.y), round(box.min.z)], "max": [round(box.max.x), round(box.max.y), round(box.max.z)]}


with open(os.path.join(SRC, "kit.json"), encoding="utf-8") as fh:
    KIT = json.load(fh)
MATS = materials()
lost = [k for k, v in MATS.items() if v is None]
if lost:
    say("materials not found (run import_surfaces.py and import_trims_decals.py first): " + ", ".join(lost))
done = 0
with unreal.ScopedSlowTask(len(KIT["pieces"]), "Importing the building kit") as slow:
    for piece in KIT["pieces"]:
        slow.enter_progress_frame(1)
        info = import_piece(piece, MATS)
        if info:
            done += 1
            say(f"{piece}: {info['min']} to {info['max']}, slots {info['slots']}" + (f", NO MATERIAL for {info['missing']}" if info["missing"] else ""))
if eal.does_directory_exist(ROOT + "/_Import"):
    eal.delete_directory(ROOT + "/_Import")
say(f"{done} of {len(KIT['pieces'])} pieces imported, {len(MATS)} materials")
