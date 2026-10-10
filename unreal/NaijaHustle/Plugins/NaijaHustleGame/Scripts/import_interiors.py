"""Brings downloaded room models (made ready by prep_interior_gltf.py) into Unreal as the insides of homes.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>). NH_INTERIOR_DIR is a folder with one sub-folder a
room, each holding a scene.glb (default <repo>/project-files/downloads/nh-interiors/ready). Each room lands in
/Game/Interiors/<sub-folder>/ as one static mesh named SM_<sub-folder> with its materials and textures, colliding
triangle for triangle so its furniture is solid. ANHEstate uses /Game/Interiors/Loft for flats and /Game/Interiors/Classic
for houses when they are there. Rooms already imported are skipped; NH_INTERIOR_REPLACE=1 brings them in again.

Third-party models are not part of the repo: each is listed in ASSETS.md with its source and licence.
"""
import os
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.environ.get("NH_INTERIOR_DIR", os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "..", "project-files", "downloads", "nh-interiors", "ready")))
REPLACE = os.environ.get("NH_INTERIOR_REPLACE", "") not in ("", "0")
# The same import serves the trees planted through the city (ANHTrees): NH_INTERIOR_DEST=/Game/Foliage, NH_INTERIOR_COLLIDE=0
# (a hundred thousand trees are not walked into), NH_INTERIOR_MASKED=1 (leaves are cut out of their cards, drawn from both sides).
DEST = os.environ.get("NH_INTERIOR_DEST", "/Game/Interiors")
COLLIDE = os.environ.get("NH_INTERIOR_COLLIDE", "1") not in ("", "0")
MASKED = os.environ.get("NH_INTERIOR_MASKED", "") not in ("", "0")
eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
pipeline = unreal.load_asset("/Interchange/Pipelines/DefaultGLTFAssetsPipeline")
mesh_settings = pipeline.get_editor_property("mesh_pipeline")
combine = type(mesh_settings.get_editor_property("combine_static_meshes_behavior"))
combine_all = next(getattr(combine, m) for m in dir(combine) if m.isupper() and "ALL" in m)
for key, value in (("combine_static_meshes_behavior", combine_all), ("build_nanite", False), ("import_skeletal_meshes", False), ("collision", False)):
    try:
        mesh_settings.set_editor_property(key, value)
    except Exception as error:
        unreal.log_warning(f"INTERIORS: import setting {key} not applied: {error}")
try:
    pipeline.get_editor_property("common_meshes_properties").set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    pipeline.get_editor_property("animation_pipeline").set_editor_property("import_animations", False)
except Exception as error:
    unreal.log_warning(f"INTERIORS: static-only import settings not applied: {error}")

def leaf_material():
    mel = unreal.MaterialEditingLibrary
    path = "/Game/Foliage/M_NHLeaf"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_NHLeaf", "/Game/Foliage", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.4)
    picture = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    picture.set_editor_property("parameter_name", "BaseColorTexture")
    mel.connect_material_property(picture, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(picture, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 300)
    rough.set_editor_property("r", 0.85)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


done = []
for name in sorted(os.listdir(ROOT)) if os.path.isdir(ROOT) else []:
    source = os.path.join(ROOT, name, "scene.glb")
    dest = f"{DEST}/{name}"
    if not os.path.exists(source):
        continue
    if eal.does_directory_exist(dest):
        if not REPLACE:
            done.append(f"{name}: already there")
            continue
        eal.delete_directory(dest)
    task = unreal.AssetImportTask()
    for key, value in {"filename": source, "destination_path": dest, "automated": True, "replace_existing": True, "save": True}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    registry.scan_paths_synchronous([dest], True)
    meshes = [a for a in registry.get_assets_by_path(dest, recursive=True) if str(a.asset_class_path.asset_name) == "StaticMesh"]
    if not meshes:
        done.append(f"{name}: NOTHING IMPORTED")
        continue
    biggest = max(meshes, key=lambda a: a.get_asset().get_bounding_box().max.x - a.get_asset().get_bounding_box().min.x)
    mesh = biggest.get_asset()
    if COLLIDE:
        mesh.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    if MASKED:
        for a in registry.get_assets_by_path(dest, recursive=True):
            if str(a.asset_class_path.asset_name) != "MaterialInstanceConstant":
                continue
            inst = a.get_asset()
            # A material of this project's for leaves: lit, cut out by the picture's alpha, drawn from both sides, and
            # allowed on instanced meshes. (The importer's own materials are not, and a tree planted as an instance
            # with one of those is drawn in the engine's plain grey.) The instance keeps its picture: same parameter name.
            inst.set_editor_property("parent", leaf_material())
            unreal.MaterialEditingLibrary.update_material_instance(inst)
    wanted = f"{dest}/SM_{name}"
    if str(biggest.package_name) != wanted:
        eal.rename_asset(str(biggest.package_name), wanted)
    eal.save_directory(dest, only_if_is_dirty=False, recursive=True)
    box = mesh.get_bounding_box()
    done.append(f"{name}: {len(meshes)} mesh, {round((box.max.x - box.min.x) / 100, 1)} x {round((box.max.y - box.min.y) / 100, 1)} x {round((box.max.z - box.min.z) / 100, 1)} m, {len(mesh.get_editor_property('static_materials'))} materials")
unreal.log(f"INTERIORS: {done}")
