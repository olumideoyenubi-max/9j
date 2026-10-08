"""Creates the city's materials and the weather parameter collection.

Run once inside the Unreal Editor (Tools > Execute Python Script...), before build_street_block.py.
Safe to run again: it rebuilds the material graphs in place and keeps the textures and values already set
on the material instances.

  /Game/NaijaHustle/Lighting/Presets/MPC_NHWeather            Wetness, Puddles, Rain, NightLights (0..1)
  /Game/NaijaHustle/Environment/Materials/M_NHSurface         master material for the city's instanced pieces
  /Game/NaijaHustle/Environment/Materials/MI_NHSurface_<Type> one instance per surface type (ENHSurfaceType)
  /Game/NaijaHustle/Environment/Materials/M_NHBlockout        the plain colour-only material (fallback)
  /Game/NaijaHustle/Environment/Materials/M_NHBlockoutPrim    vehicles and people (the plain graph, reading custom primitive data)

All read 8 floats per piece (written by ANHBlockoutActor / NHShapes):
  [0..2] colour (linear)  [3] roughness  [4] metallic  [5] night glow (nits)  [6] how wet it gets  [7] glow tint
and the weather collection:
  - Wetness darkens surfaces and makes them glossier (by how wet each piece gets)
  - Puddles adds standing water on flat ground (world-space noise): near-black and mirror-smooth
  - NightLights scales the glow of windows, signs, lamps and shop interiors

M_NHSurface adds, on top of that:
  - BaseColor, Normal and ORM texture parameters, projected world-aligned (triplanar, Tiling cm per repeat) and
    multiplied by the piece's colour, roughness and metallic. The defaults are flat, so nothing changes until
    real textures are assigned (Scripts/assign_megascans.py).
  - wear that needs no textures: large world-space grime (Grime), a dirt splash band on the first metre of
    walls (Splash) and a slight tint per actor, so per building (Variation).
"""
import os
import struct
import zlib

import unreal

MAT_DIR = "/Game/NaijaHustle/Environment/Materials"
MAT_NAME = "M_NHBlockout"
MPC_DIR = "/Game/NaijaHustle/Lighting/Presets"
MPC_NAME = "MPC_NHWeather"
WEATHER_PARAMS = ["Wetness", "Puddles", "Rain", "NightLights"]
SURFACE_NAME = "M_NHSurface"
TEX_DIR = "/Game/NaijaHustle/Environment/Textures"
GROUND_Z = 16.0  # pavement height: the splash band starts here
# One material instance per ENHSurfaceType (the names must match the C++ enum), with its starting values
SURFACE_TYPES = {
    "Generic":  {"Tiling": 200.0, "Grime": 0.15, "Splash": 0.0, "Variation": 0.00},
    "Plaster":  {"Tiling": 300.0, "Grime": 0.70, "Splash": 0.85, "Variation": 0.10},
    "Concrete": {"Tiling": 300.0, "Grime": 0.90, "Splash": 0.95, "Variation": 0.10},
    "Dirt":     {"Tiling": 400.0, "Grime": 0.55, "Splash": 0.0, "Variation": 0.06},
    "Asphalt":  {"Tiling": 400.0, "Grime": 0.45, "Splash": 0.0, "Variation": 0.04},
    "Zinc":     {"Tiling": 200.0, "Grime": 0.80, "Splash": 0.0, "Variation": 0.12},
    "Tarp":     {"Tiling": 150.0, "Grime": 0.30, "Splash": 0.0, "Variation": 0.06},
    "Wood":     {"Tiling": 150.0, "Grime": 0.50, "Splash": 0.4, "Variation": 0.10},
    "Fabric":   {"Tiling": 100.0, "Grime": 0.15, "Splash": 0.0, "Variation": 0.00},
    "Metal":    {"Tiling": 150.0, "Grime": 0.35, "Splash": 0.3, "Variation": 0.04},
    "Glass":    {"Tiling": 200.0, "Grime": 0.10, "Splash": 0.0, "Variation": 0.00},
}

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

# ---- flat default textures: white base colour, flat normal, white ORM (full roughness and metallic scale, no occlusion)
def write_png(path, rgb, size=4):
    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)
    row = b"\x00" + bytes(rgb) * size
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as fh:
        fh.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
                 + chunk(b"IDAT", zlib.compress(row * size)) + chunk(b"IEND", b""))


def flat_orm():
    """A white masks texture, the same kind as a real ORM map. Falls back to the engine's white square."""
    path = f"{TEX_DIR}/T_NHFlat_ORM"
    try:
        if not eal.does_asset_exist(path):
            png = os.path.join(unreal.Paths.project_saved_dir(), "NaijaHustle", "T_NHFlat_ORM.png")
            write_png(png, (255, 255, 255))
            task = unreal.AssetImportTask()
            for k, v in {"filename": png, "destination_path": TEX_DIR, "destination_name": "T_NHFlat_ORM", "automated": True, "replace_existing": True, "save": False}.items():
                task.set_editor_property(k, v)
            tools.import_asset_tasks([task])
        tex = eal.load_asset(path)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        eal.save_asset(path)
        return tex, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    except Exception as err:  # no import in this editor session: the engine's white texture reads the same
        unreal.log_warning(f"NAIJA HUSTLE: could not make T_NHFlat_ORM ({err}); using the engine's white texture")
        return eal.load_asset("/Engine/EngineResources/WhiteSquareTexture"), unreal.MaterialSamplerType.SAMPLERTYPE_COLOR


# ---- materials: one graph, two sources for the 8 floats, with or without textures and wear
def build(mat_name, per_instance, textured=False):
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
        if mel.connect_material_expressions(src, out, dst, pin) is False:
            raise RuntimeError(f"{mat_name}: could not connect {type(src).__name__} to {type(dst).__name__}.{pin or 'input'}")

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

    def mask(src, channels):
        e = node(unreal.MaterialExpressionComponentMask, r="r" in channels, g="g" in channels, b="b" in channels, a=False)
        link(src, e)
        return e

    def binary(cls, a, b=None, const_b=None):
        e = node(cls)
        link(a, e, "A")
        if b is not None:
            link(b, e, "B")
        else:
            e.set_editor_property("const_b", const_b)
        return e

    def add(a, b=None, const_b=None):
        return binary(unreal.MaterialExpressionAdd, a, b, const_b)

    def subtract(a, const_b):
        return binary(unreal.MaterialExpressionSubtract, a, None, const_b)

    def divide(a, b=None, const_b=None):
        return binary(unreal.MaterialExpressionDivide, a, b, const_b)

    def unary(cls, a, pin=""):
        e = node(cls)
        link(a, e, pin)
        return e

    def scalar(name, default):
        return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=default)

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

    surface_color, ao, world_normal = color, None, None  # the plain materials use the piece's colour as it is
    if textured:
        # world-aligned (triplanar) projection: one sample per axis, blended by how much the face looks along it
        pos = node(unreal.MaterialExpressionWorldPosition)
        tiled = divide(pos, scalar("Tiling", 200.0))
        uvs = [mask(tiled, "gb"), mask(tiled, "rb"), mask(tiled, "rg")]  # looking along X, Y, Z
        facing = unary(unreal.MaterialExpressionAbs, normal)
        sharp = mul(mul(facing, facing), mul(facing, facing))
        ones = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        total = node(unreal.MaterialExpressionDotProduct)
        link(sharp, total, "A")
        link(ones, total, "B")
        weights = divide(sharp, total)
        w = [mask(weights, "r"), mask(weights, "g"), mask(weights, "b")]

        def samples(name, texture, sampler_type):
            out = []
            for uv in uvs:  # the same parameter three times: one texture slot on the instance
                e = node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=name, texture=texture, sampler_type=sampler_type,
                         sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
                link(uv, e, "UVs")
                out.append(e)
            return out

        def blend(three):
            return add(add(mul(three[0], w[0]), mul(three[1], w[1])), mul(three[2], w[2]))

        orm_texture, orm_sampler = flat_orm()
        albedo = blend(samples("BaseColor", eal.load_asset("/Engine/EngineResources/WhiteSquareTexture"), unreal.MaterialSamplerType.SAMPLERTYPE_COLOR))
        orm = blend(samples("ORM", orm_texture, orm_sampler))
        ao = mask(orm, "r")
        rough = mul(rough, mask(orm, "g"))
        metal = mul(metal, mask(orm, "b"))

        # normals: each projection bends the face normal by its map's X and Y, along the two world axes it spans
        n = [mask(normal, "r"), mask(normal, "g"), mask(normal, "b")]
        bent = []
        for axis, t in enumerate(samples("Normal", eal.load_asset("/Engine/EngineMaterials/FlatNormal"), unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)):
            spans = [i for i in range(3) if i != axis]  # the world axes this projection's U and V run along
            parts = list(n)
            parts[spans[0]] = add(mask(t, "r"), n[spans[0]])
            parts[spans[1]] = add(mask(t, "g"), n[spans[1]])
            bent.append(append(append(parts[0], parts[1]), parts[2]))
        world_normal = unary(unreal.MaterialExpressionNormalize, blend(bent), "VectorInput")
        mat.set_editor_property("tangent_space_normal", False)

        # wear without textures. Grime: big soft patches in world space
        grime_noise = node(unreal.MaterialExpressionNoise, scale=0.002, levels=3, output_min=0.0, output_max=1.0)
        patches = saturate(mul(subtract(grime_noise, 0.3), const_b=3.0))
        worn = mul(mul(color, albedo), lerp(None, None, mul(patches, scalar("Grime", 0.3)), const_a=1.0, const_b=0.45))
        # dirt splashed up the first metre of walls (not on the ground or on roofs)
        height = subtract(mask(pos, "b"), GROUND_Z)
        band = saturate(unary(unreal.MaterialExpressionOneMinus, divide(height, const_b=100.0)))
        wall = unary(unreal.MaterialExpressionOneMinus, up)
        splash = mul(mul(mul(band, wall), scalar("Splash", 0.0)), lerp(None, None, patches, const_a=0.6, const_b=1.0))
        dirt = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.5, 0.36, 0.25, 1.0))
        worn = lerp(worn, mul(worn, dirt), splash)
        # a slight tint per actor (so per building): a hash of the actor's position, -1..1, times Variation
        actor_xy = mask(node(unreal.MaterialExpressionActorPositionWS), "rg")
        seed = node(unreal.MaterialExpressionDotProduct)
        link(actor_xy, seed, "A")
        link(node(unreal.MaterialExpressionConstant2Vector, r=0.0129898, g=0.078233), seed, "B")
        hash01 = unary(unreal.MaterialExpressionFrac, mul(unary(unreal.MaterialExpressionSine, seed), const_b=43758.5453))
        tint = add(mul(mul(subtract(hash01, 0.5), const_b=2.0), scalar("Variation", 0.0)), const_b=1.0)
        surface_color = mul(worn, tint)
    # puddle shapes: world-space noise (about 2.5 m blobs), thresholded
    noise = node(unreal.MaterialExpressionNoise, scale=0.004, levels=2, output_min=0.0, output_max=1.0)
    nsub = node(unreal.MaterialExpressionSubtract, const_b=0.55)
    link(noise, nsub, "A")
    puddle = mul(mul(mul(saturate(mul(nsub, const_b=6.0)), up), weather("Puddles")), wet_k)

    darken = mul(lerp(None, None, wet, const_a=1.0, const_b=0.55), lerp(None, None, puddle, const_a=1.0, const_b=0.45))
    base = mul(surface_color, darken)
    rough_wet = lerp(rough, mul(rough, const_b=0.35), wet)
    rough_final = lerp(rough_wet, None, puddle, const_b=0.03)
    metal_final = lerp(metal, None, puddle, const_b=0.0)
    warm_col = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0, 0.72, 0.42, 1.0))
    emissive = mul(mul(lerp(color, warm_col, warm), glow), weather("NightLights"))

    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough_final, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal_final, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if textured:
        mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        mel.connect_material_property(world_normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(mat)
    eal.save_asset(f"{MAT_DIR}/{mat_name}")
    return col[0]


def build_instances():
    """MI_NHSurface_<Type> for every surface type. An instance that already exists keeps its textures and values."""
    parent = eal.load_asset(f"{MAT_DIR}/{SURFACE_NAME}")
    made = 0
    for name, values in SURFACE_TYPES.items():
        path = f"{MAT_DIR}/MI_NHSurface_{name}"
        is_new = not eal.does_asset_exist(path)
        mi = load_or_create(f"MI_NHSurface_{name}", MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        if is_new:
            for param, value in values.items():
                mel.set_material_instance_scalar_parameter_value(mi, param, value)
            made += 1
        mel.update_material_instance(mi)
        eal.save_asset(path)
    return made


n1 = build(MAT_NAME, True)
n2 = build(MAT_NAME + "Prim", False)
n3 = build(SURFACE_NAME, True, textured=True)
made = build_instances()
unreal.log(f"NAIJA HUSTLE: {MPC_DIR}/{MPC_NAME}, {MAT_DIR}/{MAT_NAME} ({n1} nodes) and {MAT_NAME}Prim ({n2} nodes), "
           f"{SURFACE_NAME} ({n3} nodes) and {len(SURFACE_TYPES)} MI_NHSurface_<Type> instances ({made} new) ready")
