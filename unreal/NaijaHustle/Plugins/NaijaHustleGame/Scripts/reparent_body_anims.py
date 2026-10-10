"""Puts the bodies' animation blueprints under UNHBodyAnimInstance, so the player's action clips can be laid over them.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>), after the game code is built.

Every ABP_Unarmed* under /Game/Characters/Mannequins/Anims/Unarmed (the mannequin's, and the copy retargeted onto each
body by import_player_gltf.py) has plain AnimInstance as its parent class. With UNHBodyAnimInstance there instead, the
blueprint goes on walking and running the body exactly as before, and the game can lay a weapon clip over the arms and
trunk while it does (see NHBodyAnimInstance.h). A body whose blueprint has not been changed still plays the clips, but
only whole and while stood still. Safe to run again: a blueprint already under the class is left alone.
"""
import unreal

FOLDER = "/Game/Characters/Mannequins/Anims/Unarmed"
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous([FOLDER], True)
parent = unreal.load_class(None, "/Script/NaijaHustleGame.NHBodyAnimInstance")
done, kept, failed = [], [], []
for data in registry.get_assets_by_path(FOLDER, recursive=False):
    name = str(data.asset_name)
    if str(data.asset_class_path.asset_name) != "AnimBlueprint" or not name.startswith("ABP_Unarmed"):
        continue
    blueprint = data.get_asset()
    was = unreal.BlueprintEditorLibrary.generated_class(blueprint)
    if was and unreal.MathLibrary.class_is_child_of(was, parent):
        kept.append(name)
        continue
    unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    now = unreal.BlueprintEditorLibrary.generated_class(blueprint)
    if now and unreal.MathLibrary.class_is_child_of(now, parent) and unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        done.append(name)
    else:
        failed.append(name)
unreal.log(f"BODY ANIMS: put under NHBodyAnimInstance {done}; already there {kept}; failed {failed}")
