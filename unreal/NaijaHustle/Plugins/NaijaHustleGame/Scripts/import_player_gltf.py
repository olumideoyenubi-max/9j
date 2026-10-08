"""Imports a rigged character from a glTF download and gives it the mannequin's animations.

Run inside the Unreal Editor with two environment variables set:

  NH_PLAYER_GLTF   the character's scene.gltf (a rigged, skinned model). If its parts are separate meshes, merge
                   their primitives into one mesh first, or each part becomes its own skeletal mesh. If the
                   skeleton's root bone sits under rotated nodes (Sketchfab downloads do), put that rotation,
                   on the root bone's children first and resize the vertices, bone offsets and bind matrices so that no node carries a scale, leaving the root bone plain. Otherwise the character comes in lying down or 100 times too big, or does so once animated
  NH_PLAYER_NAME   the asset folder name; the character lands in /Game/Characters/Player/<name>/

Needs the Third Person template's mannequin (Content/Characters/Mannequins), whose animation Blueprint
ABP_Unarmed is copied and retargeted onto the new skeleton. The result is logged as the body mesh and animation
class to give ANHCharacter (BodyMesh, BodyAnimClass). Run it again to redo only the steps that are missing.

Third-party characters are not part of the repo: list each one in ASSETS.md with its source and licence.
"""
import os
import unreal

source = os.environ.get("NH_PLAYER_GLTF", "")
name = os.environ.get("NH_PLAYER_NAME", "")
if not os.path.isfile(source) or not name:
    raise RuntimeError("set NH_PLAYER_GLTF to a scene.gltf and NH_PLAYER_NAME to the asset folder name")
dest = f"/Game/Characters/Player/{name}"
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
library = unreal.EditorAssetLibrary


def assets_of(path, kind):
    registry.scan_paths_synchronous([path], True)
    return [a for a in registry.get_assets_by_path(path, recursive=True) if str(a.asset_class_path.asset_name) == kind]


if not assets_of(dest, "SkeletalMesh"):
    task = unreal.AssetImportTask()
    for key, value in {"filename": source, "destination_path": dest, "automated": True, "replace_existing": True, "save": False}.items():
        task.set_editor_property(key, value)
    tools.import_asset_tasks([task])
    library.save_directory(dest, only_if_is_dirty=False, recursive=True)
meshes = assets_of(dest, "SkeletalMesh")
if not meshes:
    raise RuntimeError(f"no skeletal mesh came out of {source}: is the model rigged?")
body = meshes[0].get_asset()
box = body.get_bounds().box_extent
unreal.log(f"NAIJA HUSTLE: body {meshes[0].package_name}: {box.x * 2:.0f} x {box.y * 2:.0f} x {box.z * 2:.0f} cm, {len(body.get_editor_property('materials'))} materials, skeleton {body.get_editor_property('skeleton').get_path_name()}")

manny = unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
rigs = f"{dest}/Rigs"


def ik_rig(asset_name, mesh):
    path = f"{rigs}/{asset_name}"
    if library.does_asset_exist(path):
        return unreal.load_asset(path)
    rig = tools.create_asset(asset_name, rigs, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    controller = unreal.IKRigController.get_controller(rig)
    controller.set_skeletal_mesh(mesh)
    controller.apply_auto_generated_retarget_definition()
    controller.apply_auto_fbik()
    unreal.log(f"NAIJA HUSTLE: {asset_name}: retarget root {controller.get_retarget_root()}, chains {[str(c.chain_name) for c in controller.get_retarget_chains()]}")
    return rig


rig_from = ik_rig("IK_Manny", manny)
rig_to = ik_rig(f"IK_{name}", body)
retargeter_path = f"{rigs}/RTG_Manny_to_{name}"
if library.does_asset_exist(retargeter_path):
    retargeter = unreal.load_asset(retargeter_path)
else:
    retargeter = tools.create_asset(f"RTG_Manny_to_{name}", rigs, unreal.IKRetargeter, unreal.IKRetargetFactory())
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    side = unreal.RetargetSourceOrTarget
    controller.set_ik_rig(side.SOURCE, rig_from)
    controller.set_ik_rig(side.TARGET, rig_to)
    controller.set_preview_mesh(side.SOURCE, manny)
    controller.set_preview_mesh(side.TARGET, body)
    for step in ("add_default_ops", "assign_ik_rig_to_all_ops"):
        if hasattr(controller, step):
            try:
                getattr(controller, step)() if step == "add_default_ops" else None
            except Exception as error:
                unreal.log_warning(f"NAIJA HUSTLE: {step}: {error}")
    controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    controller.auto_align_all_bones(side.TARGET)  # the mannequin stands in an A-pose; most downloads are in a T-pose
library.save_directory(rigs, only_if_is_dirty=False, recursive=True)

# the copies land beside the originals, with the character's name on the end
anims = "/Game/Characters/Mannequins/Anims/Unarmed"
if not library.does_asset_exist(f"{anims}/ABP_Unarmed_{name}"):
    blueprint = registry.get_asset_by_object_path("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed")
    made = unreal.IKRetargetBatchOperation.duplicate_and_retarget([blueprint], manny, body, retargeter, "", "", "", f"_{name}", "", True)
    unreal.log(f"NAIJA HUSTLE: retargeted {len(made)} animation assets")
    library.save_directory(dest, only_if_is_dirty=False, recursive=True)
    library.save_directory("/Game/Characters/Mannequins", only_if_is_dirty=True, recursive=True)
if library.does_asset_exist(f"{anims}/ABP_Unarmed_{name}"):
    unreal.log(f"NAIJA HUSTLE: BodyMesh {meshes[0].package_name}.{meshes[0].asset_name}  BodyAnimClass {anims}/ABP_Unarmed_{name}.ABP_Unarmed_{name}_C")
