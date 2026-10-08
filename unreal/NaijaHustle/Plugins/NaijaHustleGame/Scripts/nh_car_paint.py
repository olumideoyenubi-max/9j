"""
NAIJA HUSTLE car paint.

Makes M_NHCarPaint, the clear coat paint that UNHVehicleMaterialComponent drives, and two instances:
MI_NHCarPaint_Body and MI_NHCarPaint_Glass. Run it from the editor: Tools > Execute Python Script.

What the material does (all without textures, so it costs no texture memory):
  - two-layer paint: a metallic flake base under a clear coat, using the Clear Coat shading model
  - Wetness lowers both roughnesses and darkens the paint a little; RainIntensity adds moving ripples to the normal
  - no noise nodes: sparkle, ripples and scrape edges are built from sine waves, which are far cheaper per pixel
  - six impact spheres (DamageHit_<i>_Sphere / _Data) in the mesh's local space: inside one, paint is scraped to
    grey primer with no clear coat, and on a Glass instance the surface turns to rough white cracks

Put MI_NHCarPaint_Body on a car's paint slots and MI_NHCarPaint_Glass on its windows. The component finds them.
"""
import unreal

MAT_DIR = "/Game/NaijaHustle/Environment/Materials"
MAT_NAME = "M_NHCarPaint"
HIT_SLOTS = 6  # NH_VEHICLE_MAX_HITS in NHVehicleMaterialComponent.h

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


def build():
    mat = load_or_create(MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    count = [0]

    def node(cls, **props):
        e = mel.create_material_expression(mat, cls, -2200 + 170 * (count[0] // 8), -500 + 120 * (count[0] % 8))
        count[0] += 1
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(src, dst, pin="", out=""):
        if mel.connect_material_expressions(src, out, dst, pin) is False:
            raise RuntimeError(f"{MAT_NAME}: could not connect {type(src).__name__}.{out or 'output'} to {type(dst).__name__}.{pin or 'input'}")

    def op(cls, a, b=None, const_b=None, out_a="", out_b=""):
        e = node(cls)
        link(a, e, "A", out_a)
        if b is not None:
            link(b, e, "B", out_b)
        elif const_b is not None:
            e.set_editor_property("const_b", const_b)
        return e

    def mul(a, b=None, const_b=None, out_a="", out_b=""):
        return op(unreal.MaterialExpressionMultiply, a, b, const_b, out_a, out_b)

    def add(a, b=None, const_b=None):
        return op(unreal.MaterialExpressionAdd, a, b, const_b)

    def sub(a, b=None, const_b=None):
        return op(unreal.MaterialExpressionSubtract, a, b, const_b)

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

    def one(cls, a, pin=""):
        e = node(cls)
        link(a, e, pin)
        return e

    def saturate(a):
        return one(unreal.MaterialExpressionSaturate, a)

    def one_minus(a):
        return one(unreal.MaterialExpressionOneMinus, a)

    def scalar(name, default):
        return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=default)

    def wave(src, kx, ky, kz, phase=None):
        """sin(dot(position, k) + phase): cheap moving bands. Several multiplied together stand in for noise,
        which is too expensive per pixel on a car that fills the screen."""
        k = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(kx, ky, kz, 1.0))
        d = op(unreal.MaterialExpressionDotProduct, src, k)
        if phase is not None:
            d = add(d, phase)
        e = node(unreal.MaterialExpressionSine, period=6.283185)  # a plain sine of its input, in radians
        link(d, e)
        return e


    def vector(name, r, g, b, a=1.0):
        return node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(r, g, b, a))

    # ---- parameters
    paint = vector("PaintColor", 0.35, 0.02, 0.02)
    primer = vector("PrimerColor", 0.33, 0.33, 0.31)
    metallic = scalar("Metallic", 0.7)
    base_rough = scalar("BaseRoughness", 0.4)
    coat = scalar("ClearCoat", 1.0)
    coat_rough = scalar("ClearCoatRoughness", 0.05)
    flake_k = scalar("FlakeIntensity", 0.25)
    rain = scalar("RainIntensity", 0.0)
    wet = scalar("Wetness", 0.0)
    glass = scalar("Glass", 0.0)

    # ---- position in the mesh's own space, which is what the impact spheres are given in
    world = node(unreal.MaterialExpressionWorldPosition)
    local = node(unreal.MaterialExpressionTransformPosition,
                 transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                 transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    link(world, local)

    # ---- damage: the strongest sphere this pixel is inside, for scratches (Data.r) and cracks (Data.g)
    scratch_raw = crack_raw = None
    for i in range(HIT_SLOTS):
        sphere = vector(f"DamageHit_{i}_Sphere", 0.0, 0.0, 0.0, 0.0)
        data = vector(f"DamageHit_{i}_Data", 0.0, 0.0, 0.0, 0.0)
        inside = node(unreal.MaterialExpressionSphereMask, hardness_percent=35.0)
        link(local, inside, "A")
        link(sphere, inside, "B")
        link(sphere, inside, "Radius", "A")
        s = mul(inside, data, out_b="R")
        c = mul(inside, data, out_b="G")
        scratch_raw = s if scratch_raw is None else op(unreal.MaterialExpressionMax, scratch_raw, s)
        crack_raw = c if crack_raw is None else op(unreal.MaterialExpressionMax, crack_raw, c)

    # break the round masks up so they read as scrapes, not discs: three crossed wave bands in local space, 0..1
    bands = mul(mul(wave(local, 0.33, 0.07, 0.05), wave(local, 0.06, 0.41, 0.11)), wave(local, 0.09, 0.05, 0.37))
    breakup = add(mul(bands, const_b=0.5), const_b=0.5)
    scratch_all = saturate(sub(mul(scratch_raw, const_b=1.7), mul(breakup, const_b=0.7)))
    crack_all = saturate(sub(mul(crack_raw, const_b=1.7), mul(breakup, const_b=0.7)))
    scratch = mul(scratch_all, one_minus(glass))   # paint scrapes to primer
    crack = mul(crack_all, glass)                  # glass cracks

    # ---- paint: flakes brighten the base a little, unevenly
    # one random value per pixel: frac(sin(dot(position, k)) * big), the classic cheap sparkle
    flake_noise = one(unreal.MaterialExpressionFrac, mul(wave(local, 12.9898, 78.233, 37.719), const_b=43758.5453))
    flaked = mul(paint, add(mul(flake_noise, flake_k), const_b=1.0))
    wet_dark = mul(flaked, lerp(None, None, wet, const_a=1.0, const_b=0.85))
    scraped = lerp(wet_dark, primer, scratch)
    white = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.75, 0.78, 0.8, 1.0))
    base_color = lerp(scraped, white, crack)

    rough_wet = lerp(base_rough, None, wet, const_b=0.08)
    rough = lerp(lerp(rough_wet, None, scratch, const_b=0.75), None, crack, const_b=0.6)
    metal = mul(metallic, one_minus(scratch))
    coat_out = mul(coat, one_minus(saturate(add(scratch, crack))))
    coat_rough_out = lerp(coat_rough, None, wet, const_b=0.02)

    # ---- rain ripples: crossed wave bands sliding over the surface tilt the normal while it rains
    time = node(unreal.MaterialExpressionTime)
    ripple_x = mul(wave(world, 0.21, 0.13, 0.17, mul(time, const_b=6.0)), wave(world, -0.11, 0.19, 0.23, mul(time, const_b=-5.0)))
    ripple_y = mul(wave(world, 0.15, -0.22, 0.12, mul(time, const_b=5.5)), wave(world, 0.18, 0.09, -0.2, mul(time, const_b=-6.5)))
    strength = mul(rain, const_b=0.06)
    tilt = node(unreal.MaterialExpressionAppendVector)
    link(mul(ripple_x, strength), tilt, "A")
    link(mul(ripple_y, strength), tilt, "B")
    up = node(unreal.MaterialExpressionConstant, r=1.0)
    normal = node(unreal.MaterialExpressionAppendVector)
    link(tilt, normal, "A")
    link(up, normal, "B")
    normal = one(unreal.MaterialExpressionNormalize, normal, "VectorInput")

    # The clear coat outputs are not reachable one by one from Python, so the result goes out as material attributes
    mat.set_editor_property("use_material_attributes", True)
    out = node(unreal.MaterialExpressionMakeMaterialAttributes)
    for src, pin in ((base_color, "BaseColor"), (metal, "Metallic"), (rough, "Roughness"), (normal, "Normal"),
                     (coat_out, "ClearCoat"), (coat_rough_out, "ClearCoatRoughness")):
        link(src, out, pin)
    if mel.connect_material_property(out, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES) is False:
        raise RuntimeError(f"{MAT_NAME}: could not connect the material attributes")
    mel.recompile_material(mat)
    eal.save_asset(f"{MAT_DIR}/{MAT_NAME}")
    return mat, count[0]


def instance(name, parent, scalars):
    path = f"{MAT_DIR}/{name}"
    new = not eal.does_asset_exist(path)
    mi = load_or_create(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    if new:  # values are only set once, so edits made in the editor survive a rerun
        for k, v in scalars.items():
            mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    eal.save_asset(path)
    return mi


material, nodes = build()
instance("MI_NHCarPaint_Body", material, {})
instance("MI_NHCarPaint_Glass", material, {"Glass": 1.0, "Metallic": 0.0, "BaseRoughness": 0.05, "FlakeIntensity": 0.0, "ClearCoat": 1.0, "ClearCoatRoughness": 0.02})
unreal.log(f"NAIJA HUSTLE: {MAT_DIR}/{MAT_NAME} ({nodes} nodes) with {HIT_SLOTS} impact slots, MI_NHCarPaint_Body and MI_NHCarPaint_Glass ready")
