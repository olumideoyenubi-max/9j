"""Imports downloaded glTF car models, each as one static mesh, ready for assign_vehicle_meshes.py.

Run inside the Unreal Editor with one environment variable set:

  NH_CAR_DIR   a folder holding one sub-folder per model, each with a scene.gltf (a Sketchfab glTF download,
               unzipped). Name each sub-folder after its vehicle type's keyword, for example Minibus_Danfo,
               Sports_911 or Luxury_Suv_G (the keyword table is in assign_vehicle_meshes.py).

Each model lands in /Game/Vehicles/<sub-folder>/ with its materials and textures. Models already there are
skipped, so the script can be run again after adding more. Set NH_CAR_ONLY to a comma-separated list of
sub-folders to import just those.

Third-party models are not part of the repo: list each one in ASSETS.md with its source and licence.
"""
import os
import unreal

root = os.environ.get("NH_CAR_DIR", "")
only = [n for n in os.environ.get("NH_CAR_ONLY", "").split(",") if n]
if not os.path.isdir(root):
    raise RuntimeError("set NH_CAR_DIR to the folder of downloaded models")

# The glTF import settings are read from the engine's default pipeline; change them for this session only:
# one combined mesh per car, no Nanite (cars are small, and the Mac cannot draw Nanite anyway), nothing rigged.
pipeline = unreal.load_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline")
mesh_settings = pipeline.get_editor_property("mesh_pipeline")
combine = type(mesh_settings.get_editor_property("combine_static_meshes_behavior"))
combine_all = next(getattr(combine, m) for m in dir(combine) if m.isupper() and "ALL" in m)
for key, value in (("combine_static_meshes_behavior", combine_all), ("build_nanite", False), ("import_skeletal_meshes", False), ("collision", False)):
    try:
        mesh_settings.set_editor_property(key, value)
    except Exception as error:
        unreal.log_warning(f"NAIJA HUSTLE: import setting {key} not applied: {error}")
try:
    pipeline.get_editor_property("common_meshes_properties").set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    pipeline.get_editor_property("animation_pipeline").set_editor_property("import_animations", False)
except Exception as error:
    unreal.log_warning(f"NAIJA HUSTLE: static-only import settings not applied: {error}")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
for name in sorted(os.listdir(root)):
    source = os.path.join(root, name, "scene.gltf")
    if not os.path.isfile(source) or (only and name not in only):
        continue
    dest = f"/Game/Vehicles/{name}"
    if not unreal.EditorAssetLibrary.does_directory_have_assets(dest, recursive=True):
        task = unreal.AssetImportTask()
        for key, value in {"filename": source, "destination_path": dest, "automated": True, "replace_existing": True, "save": False}.items():
            task.set_editor_property(key, value)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        unreal.EditorAssetLibrary.save_directory(dest, only_if_is_dirty=False, recursive=True)
    registry.scan_paths_synchronous([dest], True)
    assets = registry.get_assets_by_path(dest, recursive=True)
    kinds = {}
    for a in assets:
        kinds[str(a.asset_class_path.asset_name)] = kinds.get(str(a.asset_class_path.asset_name), 0) + 1
    line = f"NAIJA HUSTLE: {name}: " + ", ".join(f"{count} {kind}" for kind, count in sorted(kinds.items()))
    for a in assets:
        if str(a.asset_class_path.asset_name) == "StaticMesh":
            mesh = a.get_asset()
            box = mesh.get_bounding_box()
            size = box.max - box.min
            line += f"; {a.asset_name}: {mesh.get_num_triangles(0)} triangles, {size.x:.0f} x {size.y:.0f} x {size.z:.0f} cm, {len(mesh.get_editor_property('static_materials'))} material slots"
    unreal.log(line[:900])
    unreal.SystemLibrary.collect_garbage()
