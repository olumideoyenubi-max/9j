"""Brings the luxury house and the apartment tower (build_estate_homes.py) into Unreal as /Game/Estate/SM_Mansion and SM_Tower.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>). NH_ESTATE_PACK is the folder with the two FBX files
(default <repo>/project-files/downloads/nh-estate). The meshes collide triangle for triangle, so a porch can be walked
under and a wall driven into; their flat-coloured materials are made from the files' own (Est_*). Safe to run again.
"""
import os
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
PACK = os.environ.get("NH_ESTATE_PACK", os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "..", "project-files", "downloads", "nh-estate")))
DEST = "/Game/Estate"
GLOW = {"Est_Light": 8.0}
ROUGH = {"Est_Glass": 0.08, "Est_Water": 0.05, "Est_Metal": 0.3, "Est_Dark": 0.5}
METAL = {"Est_Glass": 0.6, "Est_Metal": 1.0}
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
made = []
for name in ("SM_Mansion", "SM_Tower"):
    path = os.path.join(PACK, name + ".fbx")
    if not os.path.exists(path):
        unreal.log_warning(f"ESTATE: {path} is missing; run build_estate_homes.py in Blender first")
        continue
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    data = ui.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    task = unreal.AssetImportTask()
    for key, value in {"filename": path, "destination_path": DEST, "automated": True, "replace_existing": True, "save": False, "options": ui}.items():
        task.set_editor_property(key, value)
    tools.import_asset_tasks([task])
    mesh = eal.load_asset(f"{DEST}/{name}")
    if not isinstance(mesh, unreal.StaticMesh):
        unreal.log_warning(f"ESTATE: {name} did not come in")
        continue
    mesh.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    box = mesh.get_bounding_box()
    made.append(f"{name} {round((box.max.x - box.min.x) / 100)} x {round((box.max.y - box.min.y) / 100)} x {round((box.max.z - box.min.z) / 100)} m, {len(mesh.get_editor_property('static_materials'))} materials")
# the files' materials carry only their colour: give the glass its shine, the metal its metal, the lights their glow
for mat_name in ("Est_Render", "Est_Glass", "Est_Stone", "Est_Dark", "Est_Wood", "Est_Water", "Est_Light", "Est_Metal", "Est_Green"):
    mat = eal.load_asset(f"{DEST}/{mat_name}") if eal.does_asset_exist(f"{DEST}/{mat_name}") else None
    if not isinstance(mat, unreal.Material):
        continue
    for prop, value in ((unreal.MaterialProperty.MP_ROUGHNESS, ROUGH.get(mat_name, 0.8)), (unreal.MaterialProperty.MP_METALLIC, METAL.get(mat_name, 0.0))):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 300 if prop == unreal.MaterialProperty.MP_ROUGHNESS else 450)
        node.set_editor_property("r", value)
        mel.connect_material_property(node, "", prop)
    if mat_name in GLOW:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 600)
        node.set_editor_property("constant", unreal.LinearColor(1.0 * GLOW[mat_name], 0.86 * GLOW[mat_name], 0.6 * GLOW[mat_name], 1))
        mel.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
unreal.log(f"ESTATE: {made}")
