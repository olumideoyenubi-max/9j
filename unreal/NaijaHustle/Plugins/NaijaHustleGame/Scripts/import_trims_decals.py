"""Brings the trim sheets and the decal atlas into Unreal and makes their materials.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>) after Scripts/build_trims_decals.py.
Environment variables:
  NH_TRIMS        the folder the pictures are in (default ~/Downloads/nh-trims)
  NH_TRIM_STAGE   "all" (default), "library" (textures and materials) or "city" (lay sample decals in the Lagos map)
  NH_TRIM_REBUILD "1" builds the master materials again in place (after changing them below)
  NH_DECAL_SPOT   "X,Y,Yaw" in cm and degrees: the stretch of road the sample decals go on (default: Oshodi expressway)

What it makes, under /Game/NaijaHustle/Surfaces/Trims (generated, not stored in the repo):
  Textures/T_NH_Trim_<Sheet>_BaseColor|Normal|ORM, T_NH_Decals
  M_NH_Trim, M_NH_Trim_Cutout     one material for every strip of every sheet; Cutout is the same with the
                                  texture's alpha cut away and both sides drawn, for bars and gates
  MI_Trim_<Sheet>[_Cutout]        one instance a sheet. Parameters: Tint, RoughnessScale, Dirt
  M_NH_Decal                      a deferred decal that shows one cell of the atlas
  MI_Decal_<Name>                 one instance a decal (16). Parameters: Cell, Tint, Opacity, Roughness

Which strip or cell is which is in Data/trim_sheets.json.

Stage "city" lays 36 sample ground decals (oil, skids, cracks) on one stretch of road: enough to see them in play.
Wall decals (posters, stencils, graffiti) are not placed: the script has no way yet to know where walls are. They go
on with the building kit (look brief, section 6), and dressing whole streets is the scatter tool's job (section 7).
"""
import json
import math
import os
import random
import unreal

SRC = os.environ.get("NH_TRIMS", os.path.join(os.path.expanduser("~"), "Downloads", "nh-trims"))
STAGE = os.environ.get("NH_TRIM_STAGE", "all")
REBUILD = os.environ.get("NH_TRIM_REBUILD", "") == "1"
SPOT = [float(v) for v in os.environ.get("NH_DECAL_SPOT", "-536979,-958460,67").split(",")]
ROOT = "/Game/NaijaHustle/Surfaces/Trims"
LEVEL = "/Game/NaijaHustle/Maps/L_Lagos_City"
ROAD_Z = 0.0            # major roads in the level are flat at this height (import_lagos_city.py, FLAT_Z)

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
ST = unreal.MaterialSamplerType
M, A, D = unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd, unreal.MaterialExpressionDivide


def say(text):
    unreal.log("TRIMS: " + text)


def import_texture(name, kind):
    dest = f"{ROOT}/Textures/{name}"
    task = unreal.AssetImportTask()
    for key, value in {"filename": os.path.join(SRC, name + ".png"), "destination_path": ROOT + "/Textures", "destination_name": name,
                       "automated": True, "replace_existing": True, "save": False}.items():
        task.set_editor_property(key, value)
    tools.import_asset_tasks([task])
    tex = eal.load_asset(dest)
    tex.set_editor_property("max_texture_size", 2048)
    if kind == "normal":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)     # painted OpenGL-style, like the surface library
    elif kind == "masks":
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    eal.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


def node(mat, cls, x, y, **props):
    n = mel.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def op(mat, cls, a, b, x, y, a_out="", b_out=""):
    n = node(mat, cls, x, y)
    mel.connect_material_expressions(a, a_out, n, "A")
    mel.connect_material_expressions(b, b_out, n, "B")
    return n


def scalar(mat, name, value, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def colour(mat, name, x, y, rgba=(1, 1, 1, 1)):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*rgba))


def fresh(name):
    if eal.does_asset_exist(f"{ROOT}/{name}"):
        if not REBUILD:
            return None
        mat = eal.load_asset(f"{ROOT}/{name}")       # rebuilt in place, so its instances keep their parent
        mel.delete_all_material_expressions(mat)
        return mat
    return tools.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())


def trim_master(name, textures, cutout):
    mat = fresh(name)
    if mat is None:
        return eal.load_asset(f"{ROOT}/{name}")
    if cutout:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property("two_sided", True)
    base = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, -300, parameter_name="BaseColor", texture=textures[0], sampler_type=ST.SAMPLERTYPE_COLOR)
    normal = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0, parameter_name="Normal", texture=textures[1], sampler_type=ST.SAMPLERTYPE_NORMAL)
    orm = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 300, parameter_name="ORM", texture=textures[2], sampler_type=ST.SAMPLERTYPE_MASKS)
    tinted = op(mat, M, base, colour(mat, "Tint", -900, -500), -600, -350, a_out="RGB")
    # the building kit colours each building's metalwork and cloth: custom data 4 to 6 (0 to 3 are the wall's)
    tinted = op(mat, M, tinted, node(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -900, -650, data_index=4, const_default_value=unreal.LinearColor(1, 1, 1, 1)), -450, -450)
    # Dirt darkens the hollows first: the occlusion map already knows where they are
    hollow = node(mat, unreal.MaterialExpressionOneMinus, -600, 150)
    mel.connect_material_expressions(orm, "R", hollow, "")
    dirt = node(mat, unreal.MaterialExpressionOneMinus, -250, 100)
    mel.connect_material_expressions(op(mat, M, hollow, scalar(mat, "Dirt", 0.5, -600, 250), -420, 150), "", dirt, "")
    mel.connect_material_property(op(mat, M, tinted, dirt, -100, -300), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(op(mat, M, orm, scalar(mat, "RoughnessScale", 1.0, -600, 450), -350, 400, a_out="G"), "", MP.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", MP.MP_METALLIC)
    mel.connect_material_property(orm, "R", MP.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(normal, "RGB", MP.MP_NORMAL)
    if cutout:
        mel.connect_material_property(base, "A", MP.MP_OPACITY_MASK)
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)       # the building kit draws everything as instances
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def decal_master(atlas, grid):
    mat = fresh("M_NH_Decal")
    if mat is None:
        return eal.load_asset(ROOT + "/M_NH_Decal")
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1300, 0)
    cell = node(mat, unreal.MaterialExpressionComponentMask, -1100, 150, r=True, g=True, b=False, a=False)
    mel.connect_material_expressions(colour(mat, "Cell", -1300, 150, (0, 0, 0, 0)), "", cell, "")
    where = op(mat, D, op(mat, A, uv, cell, -900, 50), scalar(mat, "Grid", float(grid), -900, 200), -700, 50)
    tex = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0, parameter_name="Atlas", texture=atlas, sampler_type=ST.SAMPLERTYPE_COLOR)
    mel.connect_material_expressions(where, "", tex, "UVs")
    mel.connect_material_property(op(mat, M, tex, colour(mat, "Tint", -500, -200), -200, -100, a_out="RGB"), "", MP.MP_BASE_COLOR)
    mel.connect_material_property(op(mat, M, tex, scalar(mat, "Opacity", 1.0, -500, 250), -200, 150, a_out="A"), "", MP.MP_OPACITY)
    mel.connect_material_property(scalar(mat, "Roughness", 0.8, -200, 350), "", MP.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def instance(name, parent):
    path = f"{ROOT}/{name}"
    inst = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(inst, parent)
    return inst


def finish(inst):
    mel.update_material_instance(inst)
    eal.save_loaded_asset(inst, only_if_is_dirty=False)


def library(layout):
    masters = None
    for sheet, strips in layout["sheets"].items():
        textures = (import_texture(f"T_NH_Trim_{sheet}_BaseColor", "colour"), import_texture(f"T_NH_Trim_{sheet}_Normal", "normal"), import_texture(f"T_NH_Trim_{sheet}_ORM", "masks"))
        masters = masters or (trim_master("M_NH_Trim", textures, False), trim_master("M_NH_Trim_Cutout", textures, True))
        for suffix, master in (("", masters[0]), ("_Cutout", masters[1])):
            if suffix and not any(s["cutout"] for s in strips.values()):
                continue
            inst = instance(f"MI_Trim_{sheet}{suffix}", master)
            for param, tex in zip(("BaseColor", "Normal", "ORM"), textures):
                mel.set_material_instance_texture_parameter_value(inst, param, tex)
            finish(inst)
    atlas = import_texture("T_NH_Decals", "colour")
    master = decal_master(atlas, layout["decals"]["grid"])
    for name, cell in layout["decals"]["cells"].items():
        inst = instance("MI_Decal_" + name, master)
        mel.set_material_instance_vector_parameter_value(inst, "Cell", unreal.LinearColor(cell["column"], cell["row"], 0, 0))
        # oil is slick, paint and paper are not
        mel.set_material_instance_scalar_parameter_value(inst, "Roughness", 0.15 if name == "OilSpill" else 0.85)
        finish(inst)
    say(f"library: {len(layout['sheets'])} trim sheets ({sum(len(s) for s in layout['sheets'].values())} strips), {len(layout['decals']['cells'])} decals, "
        f"{len(layout['sheets']) * 3 + 1} textures, 3 master materials")


def spawn_decal(name, material, where, rotation, size_cm, index):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DecalActor, where, rotation)
    actor.set_actor_label(f"NHDecal_{name}_{index}")
    actor.set_folder_path("Decals")
    actor.decal.set_decal_material(material)
    # X is the depth the decal reaches into the surface; Y and Z are half its width and height
    actor.decal.set_editor_property("decal_size", unreal.Vector(40.0, size_cm / 2.0, size_cm / 2.0))
    return actor


def city(layout):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.load_level(LEVEL)
    x0, y0, yaw = SPOT
    # the map streams in: bring in what is within 400 m, or last run's decals are not there to clear
    try:
        box = unreal.Box(unreal.Vector(x0 - 40000, y0 - 40000, -5000), unreal.Vector(x0 + 40000, y0 + 40000, 20000))
        descs = unreal.WorldPartitionBlueprintLibrary.get_intersecting_actor_descs(box)
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
        say(f"city: loaded {len(descs)} actors round the spot")
    except Exception as error:
        say(f"city: could not load the surroundings ({error}); old decals may be left behind")
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if isinstance(actor, unreal.DecalActor) and actor.get_actor_label().startswith("NHDecal_"):
            unreal.EditorLevelLibrary.destroy_actor(actor)
    cells = layout["decals"]["cells"]
    mats = {name: eal.load_asset(f"{ROOT}/MI_Decal_{name}") for name in cells}
    rnd = random.Random(5)
    along = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)))
    side = (-along[1], along[0])
    ground = [n for n, c in cells.items() if c["on"] in ("ground", "any")]
    made = 0
    for i in range(36):                                   # 300 m of road, the lanes either side of the centre line
        d, s = rnd.uniform(-3000, 27000), rnd.uniform(-900, 900)
        name = rnd.choice(ground)
        where = unreal.Vector(x0 + along[0] * d + side[0] * s, y0 + along[1] * d + side[1] * s, ROAD_Z)
        # pointing down; skids run along the road, the rest lie any way round
        roll = yaw + (rnd.choice([0, 180]) if name == "TyreMarks" else rnd.uniform(0, 360))
        spawn_decal(name, mats[name], where, unreal.Rotator(roll=roll, pitch=-90.0, yaw=0.0), cells[name]["size_cm"] * rnd.uniform(0.8, 1.3), i)
        made += 1
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    say(f"city: {made} decals on the road round {x0:.0f}, {y0:.0f}")


with open(os.path.join(SRC, "trims.json"), encoding="utf-8") as fh:
    LAYOUT = json.load(fh)
if STAGE in ("all", "library"):
    library(LAYOUT)
if STAGE in ("all", "city"):
    city(LAYOUT)
say(f"stage '{STAGE}' done")
