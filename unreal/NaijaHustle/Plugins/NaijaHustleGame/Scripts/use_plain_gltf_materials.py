"""Makes glTF models imported in a project with Substrate materials draw properly in one without.

The engine's glTF importer parents every material instance it makes to /InterchangeAssets/gltf/Substrate/M_GLTF when
the project has Substrate on. In a project with Substrate off (this repo's) that parent draws black, so a car or a
character copied over from such a project comes out black. This re-points each of those instances at the importer's
ordinary parent, which reads the same colour, texture, metallic and roughness parameters.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>). It looks under /Game/Vehicles, /Game/Characters/Player
and /Game/Wardrobe. Safe to run again. Not carried over: Substrate-only extras (sheen, transmission), and see-through or
cut-out blending, so such parts become solid; the vehicles' windows already use M_NHCarGlass and are not touched.
"""
import unreal

ROOTS = ["/Game/Vehicles", "/Game/Characters/Player", "/Game/Wardrobe"]
SUBSTRATE = "/InterchangeAssets/gltf/Substrate/M_GLTF"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
plain = eal.load_asset("/InterchangeAssets/gltf/M_Default")
spec_gloss = eal.load_asset("/InterchangeAssets/gltf/M_SpecularGlossiness")
if not plain:
    raise RuntimeError("the engine's /InterchangeAssets/gltf/M_Default is missing: is the Interchange plugin on?")

changed, kept = 0, 0
registry.scan_paths_synchronous(ROOTS, True)
for root in ROOTS:
    for a in registry.get_assets_by_path(root, recursive=True):
        if str(a.asset_class_path.asset_name) != "MaterialInstanceConstant":
            continue
        inst = a.get_asset()
        parent = inst.get_editor_property("parent")
        if not parent or not parent.get_path_name().startswith(SUBSTRATE):
            kept += 1
            continue
        old = mel.get_material_instance_static_switch_parameter_value(inst, "bHasDiffuseSpecGloss")
        mel.set_material_instance_parent(inst, spec_gloss if old and spec_gloss else plain)
        mel.update_material_instance(inst)
        eal.save_loaded_asset(inst, only_if_is_dirty=False)
        changed += 1
unreal.log(f"NAIJA HUSTLE: {changed} glTF material instances moved off the Substrate parent, {kept} others left as they were")
