"""Brings the surface library into Unreal and builds the two master materials everything is an instance of.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>) after Scripts/fetch_surfaces_polyhaven.py.
Environment variables:
  NH_SURFACES   the folder of surfaces (default ~/Downloads/nh-surfaces): one sub-folder a surface holding
                BaseColor.jpg, Normal.jpg (OpenGL), ORM.jpg (AO, roughness, metallic), and surfaces.json listing them
  NH_SURFACE_STAGE   "all" (default), "library" (textures, masters, instances) or "city" (put them on the Lagos map)
  NH_SURFACE_REBUILD "1" builds the two master materials again in place (after changing them below)

What it makes, under /Game/NaijaHustle/Surfaces (generated, not stored in the repo):
  Textures/T_<Name>_BaseColor|Normal|ORM   base colour sRGB; normal as a normal map with green flipped; ORM as masks,
                                           linear. Capped at 2048. Three textures a surface: no unique textures per building.
  T_NH_Noise                               one small noise texture all the variation is driven from
  M_NH_Building                            walls, roofs, shutters: the mesh's UVs
  M_NH_Ground                              roads, pavements, earth: mapped by world position, so it needs no UVs
  MI_<Name>                                one instance a surface

The variation is in the shader, not in textures. Building: a tint with per-instance variation, paint fade, grime rising
from the ground, rain streaks down walls, moss low down, dust on upward faces, wetness from the game's weather.
Ground: large-scale colour variation to hide the tiling, wetness and puddles from the weather. Every amount is a
parameter, so an instance (or one building) can be cleaner or dirtier. Not done: edge wear (needs curvature data the
meshes do not carry) and streaks placed under windows (the shader does not know where windows are).

Stage "city" then puts ground instances on the Lagos map's roads, bridges and earth in place of their flat colours.
"""
import json
import os
import unreal

SRC = os.environ.get("NH_SURFACES", os.path.join(os.path.expanduser("~"), "Downloads", "nh-surfaces"))
STAGE = os.environ.get("NH_SURFACE_STAGE", "all")
REBUILD = os.environ.get("NH_SURFACE_REBUILD", "") == "1"
ROOT = "/Game/NaijaHustle/Surfaces"
WEATHER = "/Game/NaijaHustle/Lighting/Presets/MPC_NHWeather"
# the Lagos map's material slots -> surface, tile size in cm
CITY = {"Road_Asphalt": ("Asphalt", 600), "Road_Street": ("Asphalt_Patched", 450), "Road_Service": ("Laterite", 500), "Bridge_Concrete": ("Concrete_Floor_Worn", 500),
        "Rail_Ballast": ("Gravel_Road", 400), "Ground_Laterite": ("Laterite_Dry", 900), "LU_BareSand": ("Red_Sand", 800), "LU_Parking": ("Concrete_Pavement_Worn", 500),
        "LU_Industrial": ("Concrete_Floor_Worn", 700), "LU_Commercial": ("Concrete_Pavement", 500)}

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
MP = unreal.MaterialProperty
ST = unreal.MaterialSamplerType


def say(text):
    unreal.log("SURFACES: " + text)


def import_texture(path, name, kind):
    dest = f"{ROOT}/Textures/{name}"
    if not eal.does_asset_exist(dest):
        task = unreal.AssetImportTask()
        for key, value in {"filename": path, "destination_path": ROOT + "/Textures", "destination_name": name, "automated": True, "replace_existing": True, "save": False}.items():
            task.set_editor_property(key, value)
        tools.import_asset_tasks([task])
        tex = eal.load_asset(dest)
        tex.set_editor_property("max_texture_size", 2048)
        if kind == "normal":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("flip_green_channel", True)   # the maps are OpenGL-style
        elif kind == "masks":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("srgb", False)
        eal.save_loaded_asset(tex, only_if_is_dirty=False)
    return eal.load_asset(dest)


class Graph:
    """A material being built: g.n(Class, x, y, prop=value) makes a node, g.link joins two, g.out wires an output"""

    def __init__(self, name):
        if eal.does_asset_exist(f"{ROOT}/{name}"):       # rebuilt in place, so its instances keep their parent
            self.mat = eal.load_asset(f"{ROOT}/{name}")
            mel.delete_all_material_expressions(self.mat)
        else:
            self.mat = tools.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())

    def n(self, cls, x, y, **props):
        node = mel.create_material_expression(self.mat, cls, x, y)
        for key, value in props.items():
            node.set_editor_property(key, value)
        return node

    def link(self, a, b, pin, out=""):
        mel.connect_material_expressions(a, out, b, pin)
        return b

    def op(self, cls, a, b, x, y, a_out="", b_out=""):
        node = self.n(cls, x, y)
        mel.connect_material_expressions(a, a_out, node, "A")
        mel.connect_material_expressions(b, b_out, node, "B")
        return node

    def one(self, cls, source, x, y, out="", **props):
        node = self.n(cls, x, y, **props)
        mel.connect_material_expressions(source, out, node, "")
        return node

    def lerp(self, a, b, alpha, x, y, a_out="", b_out="", alpha_out=""):
        node = self.n(unreal.MaterialExpressionLinearInterpolate, x, y)
        mel.connect_material_expressions(a, a_out, node, "A")
        mel.connect_material_expressions(b, b_out, node, "B")
        mel.connect_material_expressions(alpha, alpha_out, node, "Alpha")
        return node

    def scalar(self, name, value, x, y):
        return self.n(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)

    def colour(self, name, rgb, x, y):
        return self.n(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def const(self, value, x, y):
        return self.n(unreal.MaterialExpressionConstant, x, y, r=value)

    def tex(self, name, texture, sampler, uv, x, y):
        node = self.n(unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=texture, sampler_type=sampler)
        if uv is not None:
            mel.connect_material_expressions(uv, "", node, "UVs")
        return node

    def weather(self, name, x, y):
        return self.n(unreal.MaterialExpressionCollectionParameter, x, y, collection=eal.load_asset(WEATHER), parameter_name=name)

    def finish(self):
        mel.set_material_usage(self.mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)       # the building kit draws everything as instances
        mel.recompile_material(self.mat)
        eal.save_loaded_asset(self.mat, only_if_is_dirty=False)
        return self.mat


M, A, S, D = unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd, unreal.MaterialExpressionSubtract, unreal.MaterialExpressionDivide
SAT, INV = unreal.MaterialExpressionSaturate, unreal.MaterialExpressionOneMinus


def building_master(defaults, noise):
    if eal.does_asset_exist(ROOT + "/M_NH_Building") and not REBUILD:
        return eal.load_asset(ROOT + "/M_NH_Building")
    g = Graph("M_NH_Building")
    uv = g.op(M, g.n(unreal.MaterialExpressionTextureCoordinate, -2400, -600), g.scalar("Tiling", 1.0, -2400, -500), -2200, -600)
    base = g.tex("BaseColor", defaults[0], ST.SAMPLERTYPE_COLOR, uv, -2000, -700)
    normal = g.tex("Normal", defaults[1], ST.SAMPLERTYPE_NORMAL, uv, -2000, -400)
    orm = g.tex("ORM", defaults[2], ST.SAMPLERTYPE_MASKS, uv, -2000, -100)
    wp = g.n(unreal.MaterialExpressionWorldPosition, -2400, 300)
    vn = g.n(unreal.MaterialExpressionVertexNormalWS, -2400, 700)
    x = g.one(unreal.MaterialExpressionComponentMask, wp, -2200, 250, r=True, g=False, b=False, a=False)
    y = g.one(unreal.MaterialExpressionComponentMask, wp, -2200, 350, r=False, g=True, b=False, a=False)
    z = g.one(unreal.MaterialExpressionComponentMask, wp, -2200, 450, r=False, g=False, b=True, a=False)
    nz = g.one(unreal.MaterialExpressionComponentMask, vn, -2200, 700, r=False, g=False, b=True, a=False)
    along = g.op(A, x, y, -2000, 300)                       # along a wall whichever way it faces
    # large slow noise over the wall: where paint has faded, where moss took
    append = g.n(unreal.MaterialExpressionAppendVector, -1800, 300)
    mel.connect_material_expressions(along, "", append, "A")
    mel.connect_material_expressions(z, "", append, "B")
    wall_uv = g.op(D, append, g.scalar("NoiseSize", 900.0, -1800, 450), -1600, 300)
    blotch = g.tex("Noise", noise, ST.SAMPLERTYPE_MASKS, wall_uv, -1400, 300)
    # streaks: the same noise stretched down the wall
    s_append = g.n(unreal.MaterialExpressionAppendVector, -1800, 650)
    mel.connect_material_expressions(g.op(D, along, g.scalar("StreakWidth", 260.0, -2000, 600), -1900, 600), "", s_append, "A")
    mel.connect_material_expressions(g.op(D, z, g.scalar("StreakLength", 2400.0, -2000, 750), -1900, 750), "", s_append, "B")
    streak_tex = g.n(unreal.MaterialExpressionTextureSample, -1400, 650, texture=noise, sampler_type=ST.SAMPLERTYPE_MASKS)
    mel.connect_material_expressions(s_append, "", streak_tex, "UVs")

    # 1 tint, varied a little per instance, and the mesh's own vertex colour where it carries a building's colour
    vary = g.lerp(g.op(S, g.const(1.0, -1500, -900), g.scalar("TintVariation", 0.12, -1700, -900), -1400, -900), g.op(A, g.const(1.0, -1500, -820), g.scalar("TintVariation", 0.12, -1700, -820), -1400, -820),
                  g.n(unreal.MaterialExpressionPerInstanceRandom, -1500, -750), -1200, -850)
    tinted = g.op(M, g.op(M, base, g.colour("Tint", (1, 1, 1), -1700, -1050), -1200, -1000, a_out="RGB"), vary, -1000, -950)
    # the building kit gives every instance its building's paint colour (custom data 0 to 2) and how faded it is (3)
    tinted = g.op(M, tinted, g.n(unreal.MaterialExpressionPerInstanceCustomData3Vector, -1200, -1100, data_index=0, const_default_value=unreal.LinearColor(1, 1, 1, 1)), -900, -1050)
    vc = g.lerp(g.n(unreal.MaterialExpressionConstant3Vector, -1200, -1200, constant=unreal.LinearColor(1, 1, 1, 1)), g.n(unreal.MaterialExpressionVertexColor, -1400, -1200), g.scalar("VertexColourAmount", 0.0, -1400, -1100), -1000, -1150, b_out="RGB")
    colour = g.op(M, tinted, vc, -800, -1000)
    # 2 paint fade: paler and greyer in blotches
    faded = g.op(A, g.op(M, g.one(unreal.MaterialExpressionDesaturation, colour, -700, -850), g.const(1.12, -800, -780), -550, -850), g.const(0.03, -550, -760), -400, -850)
    colour = g.lerp(colour, faded, g.op(M, g.one(SAT, g.op(M, g.op(S, blotch, g.const(0.35, -1200, 250), -1000, 250, a_out="R"), g.const(3.0, -1000, 330), -850, 250), -700, 250), g.op(A, g.scalar("PaintFade", 0.35, -850, 400), g.n(unreal.MaterialExpressionPerInstanceCustomData, -850, 480, data_index=3, const_default_value=0.0), -700, 420), -550, 250), -250, -900)
    # 3 grime rising from the ground
    height = g.one(SAT, g.one(INV, g.op(D, g.op(S, z, g.scalar("GroundZ", 0.0, -1800, 950), -1600, 900), g.scalar("GrimeHeight", 220.0, -1600, 1000), -1400, 900), -1250, 900), -1100, 900)
    grime = g.op(M, g.one(SAT, g.op(A, g.op(M, height, height, -950, 900), g.op(M, g.op(M, height, blotch, -950, 1000, b_out="G"), g.const(0.8, -950, 1080), -800, 1000), -650, 900), -500, 900), g.scalar("Grime", 0.6, -650, 1050), -350, 900)
    colour = g.lerp(colour, g.op(M, colour, g.colour("GrimeColour", (0.32, 0.26, 0.2), -400, -650), -250, -700), grime, -50, -900)
    # 4 rain streaks down the walls (not on roofs or ledges)
    wall = g.one(SAT, g.one(INV, g.op(M, g.one(unreal.MaterialExpressionAbs, nz, -2000, 1150), g.const(2.0, -2000, 1230), -1850, 1150), -1700, 1150), -1550, 1150)
    streaks = g.op(M, g.op(M, g.one(SAT, g.op(M, g.op(S, streak_tex, g.const(0.55, -1200, 700), -1050, 650, a_out="B"), g.const(4.0, -1050, 730), -900, 650), -750, 650), wall, -600, 650), g.scalar("RainStreaks", 0.45, -750, 800), -450, 650)
    colour = g.lerp(colour, g.op(M, colour, g.const(0.55, 0, -780), 100, -800), streaks, 250, -900)
    # 5 moss low down, where the blotches are
    moss = g.op(M, g.op(M, g.one(SAT, g.op(S, g.op(M, height, g.const(1.5, -350, 1250), -200, 1200), g.const(0.3, -200, 1300), -50, 1200), 100, 1200), g.one(SAT, g.op(M, g.op(S, blotch, g.const(0.45, -350, 1400), -200, 1400, a_out="R"), g.const(4.0, -200, 1480), -50, 1400), 100, 1400), 250, 1300),
               g.scalar("Moss", 0.0, 100, 1550), 400, 1300)
    colour = g.lerp(colour, g.colour("MossColour", (0.07, 0.11, 0.04), 300, -650), moss, 500, -900)
    # 6 dust settled on what faces up
    dust = g.op(M, g.op(M, g.one(SAT, nz, -2000, 1650), blotch, -1000, 1650, b_out="G"), g.scalar("Dust", 0.4, -1000, 1800), -800, 1650)
    colour = g.lerp(colour, g.colour("DustColour", (0.42, 0.33, 0.24), 550, -650), dust, 750, -900)
    # 7 wet from the game's weather: darker and shinier
    wet = g.weather("Wetness", 600, 300)
    colour = g.op(M, colour, g.lerp(g.const(1.0, 800, -500), g.const(0.68, 800, -420), wet, 950, -480), 1100, -900)
    rough = g.lerp(g.lerp(orm, g.const(0.95, 600, 0), g.op(M, grime, g.const(0.7, 500, 120), 650, 100), 800, 0, a_out="G"), g.const(0.22, 900, 150), g.op(M, wet, g.const(0.8, 800, 250), 950, 250), 1100, 0)
    mel.connect_material_property(colour, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", MP.MP_METALLIC)
    mel.connect_material_property(orm, "R", MP.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(normal, "RGB", MP.MP_NORMAL)
    return g.finish()


def ground_master(defaults, noise):
    if eal.does_asset_exist(ROOT + "/M_NH_Ground") and not REBUILD:
        return eal.load_asset(ROOT + "/M_NH_Ground")
    g = Graph("M_NH_Ground")
    g.mat.set_editor_property("tangent_space_normal", False)     # roads have no UVs to hang a tangent space on
    wp = g.n(unreal.MaterialExpressionWorldPosition, -2400, 0)
    xy = g.one(unreal.MaterialExpressionComponentMask, wp, -2200, 0, r=True, g=True, b=False, a=False)
    uv = g.op(D, xy, g.scalar("TileSize", 450.0, -2200, 120), -2000, 0)
    base = g.tex("BaseColor", defaults[0], ST.SAMPLERTYPE_COLOR, uv, -1700, -500)
    normal = g.tex("Normal", defaults[1], ST.SAMPLERTYPE_NORMAL, uv, -1700, -200)
    orm = g.tex("ORM", defaults[2], ST.SAMPLERTYPE_MASKS, uv, -1700, 100)
    macro = g.tex("Noise", noise, ST.SAMPLERTYPE_MASKS, g.op(D, xy, g.scalar("VariationSize", 7000.0, -2200, 420), -2000, 400), -1700, 400)
    # the same picture repeating to the horizon is what gives tiling away: lighter and darker across it in big patches
    patch = g.lerp(g.op(S, g.const(1.0, -1300, -700), g.scalar("Variation", 0.22, -1500, -700), -1150, -700), g.op(A, g.const(1.0, -1300, -620), g.scalar("Variation", 0.22, -1500, -620), -1150, -620), macro, -950, -660, alpha_out="R")
    colour = g.op(M, g.op(M, base, g.colour("Tint", (1, 1, 1), -1500, -850), -1300, -500, a_out="RGB"), patch, -750, -550)
    wet = g.weather("Wetness", -900, 600)
    puddle = g.op(M, g.one(SAT, g.op(M, g.op(S, macro, g.const(0.52, -1300, 500), -1150, 450, a_out="G"), g.const(6.0, -1150, 530), -1000, 450), -850, 450), g.weather("Puddles", -900, 700), -650, 450)
    colour = g.op(M, colour, g.lerp(g.const(1.0, -500, -300), g.const(0.62, -500, -220), g.one(SAT, g.op(A, wet, puddle, -500, 550), -350, 550), -200, -280), 0, -550)
    rough = g.lerp(g.lerp(orm, g.const(0.3, -300, 150), g.op(M, wet, g.const(0.7, -450, 250), -300, 250), -100, 100, a_out="G"), g.const(0.04, 0, 250), puddle, 150, 100)
    world_normal = g.lerp(normal, g.n(unreal.MaterialExpressionConstant3Vector, -200, 400, constant=unreal.LinearColor(0, 0, 1, 1)), puddle, 150, 350, a_out="RGB")   # standing water lies flat
    mel.connect_material_property(colour, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", MP.MP_METALLIC)
    mel.connect_material_property(orm, "R", MP.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(world_normal, "", MP.MP_NORMAL)
    return g.finish()


def library():
    with open(os.path.join(SRC, "surfaces.json"), encoding="utf-8") as fh:
        surfaces = json.load(fh)
    textures = {}
    with unreal.ScopedSlowTask(len(surfaces), "Importing surfaces") as task:
        for name in sorted(surfaces):
            task.enter_progress_frame(1)
            folder = os.path.join(SRC, name)
            textures[name] = (import_texture(os.path.join(folder, "BaseColor.jpg"), f"T_{name}_BaseColor", "colour"),
                              import_texture(os.path.join(folder, "Normal.jpg"), f"T_{name}_Normal", "normal"),
                              import_texture(os.path.join(folder, "ORM.jpg"), f"T_{name}_ORM", "masks"))
    noise = import_texture(os.path.join(SRC, "_Noise", "T_NH_Noise.png"), "T_NH_Noise", "masks")
    masters = {"building": building_master(textures["Plaster"], noise), "ground": ground_master(textures["Asphalt"], noise)}
    made = 0
    for name, info in sorted(surfaces.items()):
        path = f"{ROOT}/MI_{name}"
        if eal.does_asset_exist(path):
            continue
        inst = tools.create_asset("MI_" + name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(inst, masters[info["kind"]])
        for param, tex in zip(("BaseColor", "Normal", "ORM"), textures[name]):
            mel.set_material_instance_texture_parameter_value(inst, param, tex)
        mel.update_material_instance(inst)
        eal.save_loaded_asset(inst, only_if_is_dirty=False)
        made += 1
    say(f"library: {len(surfaces)} surfaces, {len(textures) * 3 + 1} textures, 2 master materials, {made} instances made this run")


def city():
    """Ground instances onto the Lagos map: each slot's own instance, with the tile size that suits it"""
    registry.scan_paths_synchronous(["/Game/Lagos"], True)
    mats = {}
    for slot, (surface, tile) in CITY.items():
        path = f"{ROOT}/City/MI_City_{slot}"
        inst = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
            "MI_City_" + slot, ROOT + "/City", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        # set every run, so changing CITY above and running the stage again is enough
        mel.set_material_instance_parent(inst, eal.load_asset(f"{ROOT}/MI_{surface}"))
        mel.set_material_instance_scalar_parameter_value(inst, "TileSize", float(tile))
        mel.update_material_instance(inst)
        eal.save_loaded_asset(inst, only_if_is_dirty=False)
        mats[slot] = inst
    changed = 0
    meshes = [a for a in registry.get_assets_by_path("/Game/Lagos", recursive=True) if str(a.asset_class_path.asset_name) == "StaticMesh"
              and any(k in str(a.package_path) for k in ("/Roads_", "/Bridges_", "/Railways", "/Shared/Terrain", "/LinkBridge"))]
    with unreal.ScopedSlowTask(len(meshes), "Surfacing the city") as task:
        for a in meshes:
            task.enter_progress_frame(1)
            mesh = a.get_asset()
            touched = False
            for i, slot in enumerate(mesh.get_editor_property("static_materials")):
                name = str(slot.get_editor_property("material_slot_name"))
                if name in mats and mesh.get_material(i) != mats[name]:
                    mesh.set_material(i, mats[name])
                    touched = True
            if touched:
                eal.save_loaded_asset(mesh, only_if_is_dirty=False)
                changed += 1
    say(f"city: {changed} of {len(meshes)} road, bridge and terrain meshes given surfaces ({', '.join(f'{k}: {v[0]}' for k, v in CITY.items())})")


if STAGE in ("all", "library"):
    library()
if STAGE in ("all", "city"):
    city()
say(f"stage '{STAGE}' done")
