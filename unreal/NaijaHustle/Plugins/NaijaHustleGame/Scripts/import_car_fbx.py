"""Imports one car model from an FBX file as a single static mesh, ready for assign_vehicle_meshes.py.

Run inside the Unreal Editor with two environment variables set:

  NH_CAR_FBX   the .fbx file
  NH_CAR_NAME  the asset name. Put the vehicle type's keyword in it so assign_vehicle_meshes.py knows what it is,
               for example SuperSuv_Absolut, Sports_GT or LuxurySuv_G (the keyword table is in that script).

The model lands in /Game/Vehicles/<NH_CAR_NAME>/ with its materials and any textures the FBX points at. Its parts
are combined into one mesh, so the wheels do not turn.

Run it twice. The first run imports and saves the model. The second run, finding the model already there,
generates three lower levels of detail, because a downloaded car is usually far too heavy to draw several times
without them. The two are separate runs because doing both at once ran an 8 GB Mac out of memory on a
1.2 million triangle car.

Third-party models are not part of the repo: list each one in ASSETS.md with its source and licence.
"""
import os
import unreal

fbx = os.environ.get("NH_CAR_FBX", "")
name = os.environ.get("NH_CAR_NAME", "")
if not os.path.isfile(fbx) or not name:
    raise RuntimeError("set NH_CAR_FBX to an .fbx file and NH_CAR_NAME to the asset name")
dest = f"/Game/Vehicles/{name}"

options = unreal.FbxImportUI()
options.set_editor_property("import_mesh", True)
options.set_editor_property("import_as_skeletal", False)
options.set_editor_property("import_animations", False)
options.set_editor_property("import_materials", True)
options.set_editor_property("import_textures", True)
options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
mesh_options = options.get_editor_property("static_mesh_import_data")
mesh_options.set_editor_property("combine_meshes", True)
mesh_options.set_editor_property("auto_generate_collision", False)   # the vehicle has its own box
mesh_options.set_editor_property("generate_lightmap_u_vs", False)    # lighting is dynamic
mesh_options.set_editor_property("remove_degenerates", True)

already = unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/SM_{name}")
task = unreal.AssetImportTask()
for key, value in {"filename": fbx, "destination_path": dest, "destination_name": f"SM_{name}", "automated": True,
                   "replace_existing": True, "save": False, "options": options}.items():
    task.set_editor_property(key, value)
if not already:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.EditorAssetLibrary.save_directory(dest, only_if_is_dirty=False, recursive=True)

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous([dest], True)
assets = registry.get_assets_by_path(dest, recursive=True)
meshes = [a.get_asset() for a in assets if str(a.asset_class_path.asset_name) == "StaticMesh"]
unreal.log(f"NAIJA HUSTLE: imported {len(assets)} assets into {dest}: " + ", ".join(sorted(f"{a.asset_class_path.asset_name}:{a.asset_name}" for a in assets))[:1500])
subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for mesh in meshes:
    box = mesh.get_bounding_box()
    size = box.max - box.min
    tris = mesh.get_num_triangles(0)
    slots = [str(m.material_slot_name) for m in mesh.get_editor_property("static_materials")]
    # lower levels of detail: half, a fifth and a twentieth of the triangles, switching as the car gets smaller on screen
    reduction = unreal.StaticMeshReductionOptions()
    reduction.set_editor_property("auto_compute_lod_screen_size", False)
    reduction.set_editor_property("reduction_settings", [unreal.StaticMeshReductionSettings(1.0, 1.0), unreal.StaticMeshReductionSettings(0.5, 0.5),
                                                         unreal.StaticMeshReductionSettings(0.2, 0.25), unreal.StaticMeshReductionSettings(0.05, 0.1)])
    lods = mesh.get_num_lods()
    if already and lods < 2:
        lods = subsystem.set_lods_with_notification(mesh, reduction, True)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    unreal.log(f"NAIJA HUSTLE: {mesh.get_name()}: {tris} triangles, {size.x:.0f} x {size.y:.0f} x {size.z:.0f} cm, {lods} levels of detail "
               f"({', '.join(str(mesh.get_num_triangles(i)) for i in range(max(lods, 1)))} triangles), {len(slots)} material slots: {', '.join(slots)}")
