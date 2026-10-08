"""Makes downloaded vehicle models cheaper to draw and gives them see-through glass.

Run inside the Unreal Editor after import_car_gltf.py. For every static mesh under /Game/Vehicles it:

- generates three lower levels of detail (a model with only one), sized so that the first is at most about
  25,000 triangles, switching as the car gets smaller on screen;
- points every material slot that is named like glass at one shared translucent glass material, so the driver can
  be seen from outside and can see out in the cabin view (many downloads have solid dark glass).

Safe to run again: models that already have levels of detail are left alone.
"""
import unreal

library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
GLASS = ("glass", "vitre", "vidrio", "window", "windscreen", "windshield", "verre", "steklo")
NOT_GLASS = ("light", "lamp", "luz", "trim", "frame", "red", "orange", "head", "tail")
# glass that the name does not give away: model folder -> slot names (found from each download's own materials)
EXTRA_GLASS = {"Super_Suv_Urus": ["Material_004"], "Coupe_Suv_X6": ["Material_002"], "Luxury_Suv_GClass": ["Material_004", "Material_005"],
               "Royal_Cullinan": ["material_3"], "Pickup_Hilux": ["01_-_Default"]}


def glass_material():
    path = "/Game/Vehicles/M_NHCarGlass"
    if library.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = tools.create_asset("M_NHCarGlass", "/Game/Vehicles", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    # the default, cheapest translucent lighting: per-pixel lit glass on a dozen cars cost the 8 GB Mac a third of its frame rate
    for prop, value, y in ((unreal.MaterialProperty.MP_BASE_COLOR, None, 0), (unreal.MaterialProperty.MP_OPACITY, 0.28, 200), (unreal.MaterialProperty.MP_ROUGHNESS, 0.08, 400), (unreal.MaterialProperty.MP_SPECULAR, 0.6, 600)):
        if value is None:
            node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -350, y)
            node.set_editor_property("constant", unreal.LinearColor(0.02, 0.03, 0.035, 1))
        else:
            node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, y)
            node.set_editor_property("r", value)
        mel.connect_material_property(node, "", prop)
    mel.recompile_material(mat)
    library.save_loaded_asset(mat)
    return mat


registry.scan_paths_synchronous(["/Game/Vehicles"], True)
subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
glass = glass_material()
for a in sorted(registry.get_assets_by_path("/Game/Vehicles", recursive=True), key=lambda x: str(x.package_name)):
    if str(a.asset_class_path.asset_name) != "StaticMesh":
        continue
    mesh = a.get_asset()
    model = str(a.package_name).split("/")[3]
    tris = mesh.get_num_triangles(0)
    changed = []
    slots = mesh.get_editor_property("static_materials")
    for index, slot in enumerate(slots):
        low = str(slot.material_slot_name).lower()
        named = any(word in low for word in GLASS) and not any(word in low for word in NOT_GLASS)
        if (named or str(slot.material_slot_name) in EXTRA_GLASS.get(model, [])) and slot.material_interface != glass:
            mesh.set_material(index, glass)
            changed.append(str(slot.material_slot_name))
    lods = mesh.get_num_lods()
    if lods < 2:
        first = min(0.5, 25000.0 / max(tris, 1))
        reduction = unreal.StaticMeshReductionOptions()
        reduction.set_editor_property("auto_compute_lod_screen_size", False)
        reduction.set_editor_property("reduction_settings", [unreal.StaticMeshReductionSettings(1.0, 1.0), unreal.StaticMeshReductionSettings(first, 0.4),
                                                             unreal.StaticMeshReductionSettings(first * 0.4, 0.18), unreal.StaticMeshReductionSettings(first * 0.12, 0.07)])
        lods = subsystem.set_lods_with_notification(mesh, reduction, True)
    library.save_loaded_asset(mesh)
    unreal.log(f"NAIJA HUSTLE: {model}: {len(slots)} slots ({', '.join(str(s.material_slot_name) for s in slots)[:160]}); glass: {changed or 'none found'}; levels of detail {[mesh.get_num_triangles(i) for i in range(lods)]}")
    unreal.SystemLibrary.collect_garbage()
