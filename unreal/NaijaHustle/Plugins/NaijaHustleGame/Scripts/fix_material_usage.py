"""Marks the city's materials as used on instanced meshes, so a cook makes the shaders for that.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>). Without it a packaged game logs
"Missing shader resource ... while serializing asset M_Wall_Plain" and draws the kit's buildings in the default grey:
in the editor the flag is set on first use and the shader compiled then; a cooked game cannot compile one.
Safe to run again.
"""
import unreal

library = unreal.EditorAssetLibrary
changed = 0
for folder in ("/Game/Lagos/Materials", "/Game/NaijaHustle/Surfaces", "/Game/NaijaHustle/Kit"):
    for path in library.list_assets(folder, recursive=True, include_folder=False):
        asset = unreal.load_asset(path.split(".")[0])
        if not isinstance(asset, unreal.Material):
            continue
        if not asset.get_editor_property("used_with_instanced_static_meshes"):
            asset.set_editor_property("used_with_instanced_static_meshes", True)
            unreal.MaterialEditingLibrary.recompile_material(asset)
            library.save_loaded_asset(asset, only_if_is_dirty=False)
            changed += 1
            unreal.log(f"USAGE: {path}: used with instanced static meshes")
unreal.log(f"USAGE: {changed} materials changed")
unreal.SystemLibrary.quit_editor()
