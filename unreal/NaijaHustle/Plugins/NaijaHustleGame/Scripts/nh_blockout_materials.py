"""Creates the blockout material and the weather parameter collection.

Run once inside the Unreal Editor (Tools > Execute Python Script...), before build_street_block.py.
Safe to run again: it rebuilds the material graph in place.

  /Game/NaijaHustle/Lighting/Presets/MPC_NHWeather          Wetness, Puddles, Rain, NightLights (0..1)
  /Game/NaijaHustle/Environment/Materials/M_NHBlockout      the city's instanced pieces (reads per-instance custom data)
  /Game/NaijaHustle/Environment/Materials/M_NHBlockoutPrim  vehicles and people (the same graph, reading custom primitive data)

Both read 8 floats per piece (written by ANHBlockoutActor / NHShapes):
  [0..2] colour (linear)  [3] roughness  [4] metallic  [5] night glow (nits)  [6] how wet it gets  [7] glow tint
and the weather collection:
  - Wetness darkens surfaces and makes them glossier (by how wet each piece gets)
  - Puddles adds standing water on flat ground (world-space noise): near-black and mirror-smooth
  - NightLights scales the glow of windows, signs, lamps and shop interiors
"""
import unreal

MAT_DIR = "/Game/NaijaHustle/Environment/Materials"
MAT_NAME = "M_NHBlockout"
MPC_DIR = "/Game/NaijaHustle/Lighting/Presets"
MPC_NAME = "MPC_NHWeather"
WEATHER_PARAMS = ["Wetness", "Puddles", "Rain", "NightLights"]

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def load_or_create(name, folder, cls, factory):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    if not eal.does_directory_exist(folder):
        eal.make_directory(folder)
    return tools.create_asset(name, folder, cls, factory)


# ---- weather parameter collection
mpc = load_or_create(MPC_NAME, MPC_DIR, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
existing = {str(p.get_editor_property("parameter_name")) for p in mpc.get_editor_property("scalar_parameters")}
if set(WEATHER_PARAMS) - existing:
    params = list(mpc.get_editor_property("scalar_parameters"))
    for name in WEATHER_PARAMS:
        if name not in existing:
            p = unreal.CollectionScalarParameter()  # the C++ constructor gives each parameter a fresh id
            p.set_editor_property("parameter_name", name)
            p.set_editor_property("default_value", 0.0)
            params.append(p)
    mpc.set_editor_property("scalar_parameters", params)
eal.save_asset(f"{MPC_DIR}/{MPC_NAME}")

# ---- materials: one graph, two sources for the 8 floats
def build(mat_name, per_instance):
    mat = load_or_create(mat_name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    if per_instance:
        mat.set_editor_property("used_with_instanced_static_meshes", True)
    col = [0]

    def node(cls, **props):
        """Creates an expression, laid out left to right in creation order."""
        e = mel.create_material_expression(mat, cls, -1200 + 160 * (col[0] // 6), -300 + 110 * (col[0] % 6))
        col[0] += 1
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(src, dst, pin="", out=""):
        mel.connect_material_expressions(src, out, dst, pin)

    def custom(index):
        if per_instance:
            return node(unreal.MaterialExpressionPerInstanceCustomData, data_index=index)
        # a scalar parameter that reads the component's custom primitive data
        return node(unreal.MaterialExpressionScalarParameter, parameter_name=f"CPD{index}", use_custom_primitive_data=True, primitive_data_index=index)

    def weather(name):
        e = node(unreal.MaterialExpressionCollectionParameter)
        e.set_editor_property("collection", mpc)       # collection first: setting the name then looks up its id
        e.set_editor_property("parameter_name", name)
        return e

    def mul(a, b=None, const_b=None):
        e = node(unreal.MaterialExpressionMultiply)
        link(a, e, "A")
        if b is not None:
            link(b, e, "B")
        else:
            e.set_editor_property("const_b", const_b)
        return e

    def lerp(a, b, alpha, const_a=None, const_b=None):
        e = node(unreal.MaterialExpressionLinearInterpolate)
        if a is not None:
            link(a, e, "A")
        else:
            e.set_editor_property("const_a", const_a)
        if b is not None:
            link(b, e, "B")
        else:
            e.set_editor_property("const_b", const_b)
        link(alpha, e, "Alpha")
        return e

    def saturate(a):
        e = node(unreal.MaterialExpressionSaturate)
        link(a, e)
        return e

    def append(a, b):
        e = node(unreal.MaterialExpressionAppendVector)
        link(a, e, "A")
        link(b, e, "B")
        return e

    r, g, b = custom(0), custom(1), custom(2)
    color = append(append(r, g), b)
    rough, metal, glow, wet_k, warm = custom(3), custom(4), custom(5), custom(6), custom(7)

    wet = mul(weather("Wetness"), wet_k)
    # flat, upward-facing ground holds puddles: (normal.z - 0.6) * 4, clamped
    normal = node(unreal.MaterialExpressionVertexNormalWS)
    nz = node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
    link(normal, nz)
    sub = node(unreal.MaterialExpressionSubtract, const_b=0.6)
    link(nz, sub, "A")
    up = saturate(mul(sub, const_b=4.0))
    # puddle shapes: world-space noise (about 2.5 m blobs), thresholded
    noise = node(unreal.MaterialExpressionNoise, scale=0.004, levels=2, output_min=0.0, output_max=1.0)
    nsub = node(unreal.MaterialExpressionSubtract, const_b=0.55)
    link(noise, nsub, "A")
    puddle = mul(mul(mul(saturate(mul(nsub, const_b=6.0)), up), weather("Puddles")), wet_k)

    darken = mul(lerp(None, None, wet, const_a=1.0, const_b=0.55), lerp(None, None, puddle, const_a=1.0, const_b=0.45))
    base = mul(color, darken)
    rough_wet = lerp(rough, mul(rough, const_b=0.35), wet)
    rough_final = lerp(rough_wet, None, puddle, const_b=0.03)
    metal_final = lerp(metal, None, puddle, const_b=0.0)
    warm_col = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0, 0.72, 0.42, 1.0))
    emissive = mul(mul(lerp(color, warm_col, warm), glow), weather("NightLights"))

    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough_final, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal_final, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_asset(f"{MAT_DIR}/{mat_name}")
    return col[0]


n1 = build(MAT_NAME, True)
n2 = build(MAT_NAME + "Prim", False)
unreal.log(f"NAIJA HUSTLE: {MPC_DIR}/{MPC_NAME}, {MAT_DIR}/{MAT_NAME} ({n1} nodes) and {MAT_NAME}Prim ({n2} nodes) ready")
