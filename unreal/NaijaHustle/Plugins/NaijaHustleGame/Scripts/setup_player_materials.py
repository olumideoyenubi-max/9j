"""Gives an imported player character working materials.

An FBX from Blender arrives in Unreal with its textures imported but not connected, so the character comes out in
flat colours. This makes two simple parent materials (opaque, and masked two-sided for hair, beard, lashes and
clothing shells) and points each slot's imported material instance at one of them, with its texture and tint.

Run inside the Unreal Editor with NH_PLAYER_NAME set (the folder under /Game/Characters/Player). Slots are matched
to textures by the HINTS table below; a slot with no texture keeps its flat colour as a tint.
"""
import os
import unreal

name = os.environ.get("NH_PLAYER_NAME", "")
dest = f"/Game/Characters/Player/{name}"
library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
# slot name contains -> (texture name contains, masked and two-sided, roughness)
HINTS = [("body", "male_diffuse", False, 0.55), ("low-poly", "eye", False, 0.25), ("eyebrow", "eyebrow", True, 0.8), ("eyelash", "eyelash", True, 0.8),
         ("teeth", "teeth", False, 0.4), ("beard", "beard", True, 0.85), ("tankshirt", "toptex", True, 0.9), ("cargo", "cargo_pants_diff", True, 0.85),
         ("short0", None, False, 0.7), ("hair", "hair", True, 0.7)]


def parent(masked):
    path = f"/Game/Characters/Player/M_NHPlayer{'Masked' if masked else ''}"
    if library.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = tools.create_asset(path.rsplit("/", 1)[1], path.rsplit("/", 1)[0], unreal.Material, unreal.MaterialFactoryNew())
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    tex.set_editor_property("parameter_name", "BaseColor")
    tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 300)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 100)
    mel.connect_material_expressions(tex, "RGB", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -350, 350)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.7)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if masked:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property("two_sided", True)
        mel.connect_material_property(tex, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    mat.set_editor_property("used_with_skeletal_mesh", True)  # without this the game falls back to the grey checker material
    mel.recompile_material(mat)
    library.save_loaded_asset(mat)
    return mat


registry.scan_paths_synchronous([dest], True)
assets = registry.get_assets_by_path(dest, recursive=True)
textures = {str(a.asset_name): a for a in assets if str(a.asset_class_path.asset_name) == "Texture2D"}
mesh = [a for a in assets if str(a.asset_class_path.asset_name) == "SkeletalMesh"][0].get_asset()
slots = mesh.get_editor_property("materials")
for slot in slots:
    slot_name = str(slot.material_slot_name)
    key = slot_name.lower()[5:] if slot_name.lower().startswith("body_") else slot_name.lower()  # MPFB names every slot Body.<part>
    hint = next((h for h in HINTS if h[0] in key), None)
    texture = next((textures[t].get_asset() for t in sorted(textures) if hint and hint[1] and hint[1].lower() in t.lower()), None)
    masked = bool(hint and hint[2] and texture)
    old = slot.material_interface
    tint = unreal.LinearColor(1, 1, 1, 1)
    if not texture and isinstance(old, unreal.MaterialInstanceConstant):
        for v in old.get_editor_property("vector_parameter_values"):
            if str(v.parameter_info.name) == "DiffuseColor":
                tint = v.parameter_value
    # the import made one material instance per slot: re-point that instance rather than swapping the slot's material
    if not isinstance(old, unreal.MaterialInstanceConstant):
        unreal.log_warning(f"NAIJA HUSTLE: {slot_name} has no material instance to set up")
        continue
    inst = old
    mel.set_material_instance_parent(inst, parent(masked))
    if texture:
        mel.set_material_instance_texture_parameter_value(inst, "BaseColor", texture)
    if "cargo" in key:
        tint = unreal.LinearColor(0.62, 0.6, 0.5, 1)  # a duller olive than the recoloured texture
    mel.set_material_instance_vector_parameter_value(inst, "Tint", tint)
    mel.set_material_instance_scalar_parameter_value(inst, "Roughness", hint[3] if hint else 0.7)
    library.save_loaded_asset(inst)
    unreal.log(f"NAIJA HUSTLE: {slot_name}: {'masked' if masked else 'opaque'}, texture {texture.get_name() if texture else None}, tint ({tint.r:.2f}, {tint.g:.2f}, {tint.b:.2f})")
