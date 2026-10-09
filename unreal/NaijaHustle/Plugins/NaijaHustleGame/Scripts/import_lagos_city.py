"""Brings the real-scale Lagos city model into Unreal as the level L_Lagos_City.

The model is the OpenStreetMap-based Lagos made in Blender (lagos_city_detailed.blend), cut into about 5,000
pieces by lagos_retile.py so that far-away parts can be hidden: 600 m cells of buildings and trees, 1.2 km cells
of roads and bridges. The model's own traffic (plain boxes that never move) is left out: in play ANHTraffic puts the
game's vehicles on these roads instead, driving and parked. Credit "(c) OpenStreetMap contributors" (ODbL) in anything published.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>). Environment variables:

  LAGOS_TILES   folder with the piece FBX files and manifest.json   (default ~/Downloads/map lagos/Lagos_Tiles)
  LAGOS_PACK    folder with Textures/ and Shared/                    (default ~/Downloads/map lagos/Lagos_Unreal)
  LAGOS_STAGE   "all" (default), "assets" (textures and materials), "meshes", or "level"
  LAGOS_ONLY    comma-separated FBX names (without .fbx) to limit the "meshes" stage to, for a quick trial
  LAGOS_REIMPORT  comma-separated names (Terrain, Airport, Port, or a piece FBX such as Bridges_Piers) to import again after the FBX changed

Safe to run again: an FBX whose pieces are already in the project is skipped, so a run that was killed for
memory (it happens on the 8 GB Mac) carries on where it stopped. The level is rebuilt from scratch each time.

What it makes, all under /Game/Lagos (generated, so not stored in the repo):
  Textures/, Materials/   the seven building materials (tinted per building by vertex colour), flat colours for
                          roads, a vertex-colour material for trees
  Tiles/<fbx name>/       one static mesh per piece; buildings, roads, bridges and terrain collide per triangle
  Shared/                 terrain, airport and port, one mesh each
and /Game/NaijaHustle/Maps/L_Lagos_City: every piece at its real place with a draw distance by kind, the lighting
rig, and a PlayerStart by Oshodi Motor Park, where Data/lagos_real.json (Scripts/build_lagos_real.py) puts it. That
file also holds the stops, the park's bays and the road graph the game uses in this level, so the mission, the
minimap and the traffic all sit on these streets. The player's body is whichever skin the project has.

World Partition: the level is made partitioned, one file per actor. Everything you can drive or stand on (terrain,
every road, bridges and their piers, airport, port) is not spatially loaded, so it is always there: nothing is ever
missing under a moving car. The buildings and trees, which are nearly all of the memory, load by cell around the
streaming sources (the player, and the vehicle being driven: ANHPlayerController::UpdateStreaming sets how far).
Data layers DL_Crowds, DL_Props, DL_Interiors and DL_Vehicles are made for later content to go into.

Placement: the model's origin is 3.40 E, 6.47 N; 1 cm = 1 cm; Blender north (+Y) is Unreal -Y.
Heights: in the model the land is at 2.00 m and each kind of surface is a flat sheet stacked above it in steps (side
streets at 2.75 m, main roads at 3.05 m), which left a ledge of up to a metre beside every road. Here each kind is
put down separately so main-road surfaces are at Z = 0 and everything else lies within 16 cm below them (FLAT_Z); the
land-use patches and the airport were flattened the same way in the Shared/ FBX files. Bridges are stretched upward
from road level by BRIDGE_RISE (1.5), because as modelled the lower flyovers cleared the road beneath by 2 m. That
steepens their ramps from about 8% to about 12%; at 1.8 it was about 15% and long vehicles could not get up them. The piers
that stood on a road (470 of 2,488) were taken out of Bridges_Piers.fbx in Blender.
"""
import json
import os
import unreal

HOME = os.path.expanduser("~")
TILES = os.environ.get("LAGOS_TILES", os.path.join(HOME, "Downloads", "map lagos", "Lagos_Tiles"))
PACK = os.environ.get("LAGOS_PACK", os.path.join(HOME, "Downloads", "map lagos", "Lagos_Unreal"))
STAGE = os.environ.get("LAGOS_STAGE", "all")
ONLY = [n for n in os.environ.get("LAGOS_ONLY", "").split(",") if n]
REIMPORT = [n for n in os.environ.get("LAGOS_REIMPORT", "").split(",") if n]

DEST = "/Game/Lagos"
LEVEL = "/Game/NaijaHustle/Maps/L_Lagos_City"
TAG = "LagosCity"
UNIT = 10000.0          # one Blender unit of the model is 100 m
ROAD_Z = 305.0          # main-road surface height in the model, cm
LAND_Z = 200.0          # the land's height in the model, cm: buildings, trees, the port and the terrain stand on it
GROUND = -16.0          # where the land goes in the level, cm
# the flat kinds: height in the model, cm, and where that goes in the level
FLAT_Z = {"Roads_Major": (305.0, 0.0), "Road_Markings": (312.0, 2.0), "Railways": (290.0, -1.5), "Roads_Streets": (275.0, -2.0), "Roads_Service": (260.0, -3.0)}
BRIDGE_RISE = 1.5       # bridges, their piers and the pylon are this much taller above road level than modelled
REAL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Data", "lagos_real.json")
with open(REAL, encoding="utf-8") as fh:
    START = json.load(fh)["playerStart"]     # by Oshodi Motor Park, on the verge

FACADES = ["Facade_Punched_A", "Facade_Punched_B", "Facade_Shop", "Facade_Curtain", "Wall_Plain", "Roof_Corrugated", "Roof_Concrete"]
# slot name -> (colour, roughness), the colours of the Blender materials
FLAT = {"Road_Asphalt": ((0.045, 0.045, 0.05), 0.75), "Road_Street": ((0.13, 0.12, 0.105), 0.9), "Road_Service": ((0.24, 0.21, 0.17), 0.95),
        "Road_Paint": ((0.75, 0.74, 0.68), 0.6), "Rail_Ballast": ((0.17, 0.13, 0.11), 0.9), "Bridge_Concrete": ((0.5, 0.49, 0.46), 0.8)}
VERTEX_COLOUR = ["Foliage", "Vehicle_Paint", "Container_Paint", "Boat_Paint"]

# piece kinds, first matching prefix wins: collide per triangle, max draw distance in cm (0 = always drawn), cast shadows
KINDS = [("Buildings_", True, 350000.0, True), ("Trees", False, 120000.0, False), ("Traffic", True, 60000.0, True), ("Roads_Major", True, 600000.0, False),
         ("Roads_Streets", True, 300000.0, False), ("Roads_Service", True, 150000.0, False), ("Road_Markings", False, 40000.0, False),
         ("Railways", False, 300000.0, False), ("Bridges_Decks", True, 0.0, True), ("Bridges_Piers", True, 600000.0, True),
         ("LinkBridge", True, 0.0, True)]
SKIP = {"Trees", "Traffic"}   # the 17-vertex trees (Trees_Low is the set this Mac can draw); the model's box cars (see above)
SHARED = [("Terrain", True), ("Airport", False), ("Port", True)]

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def say(text):
    unreal.log("LAGOS: " + text)


def kind_of(name):
    for prefix, collide, distance, shadow in KINDS:
        if name.startswith(prefix):
            return collide, distance, shadow
    return False, 0.0, False


# ------------------------------------------------------------------------------------------- textures, materials
def import_textures():
    folder = os.path.join(PACK, "Textures")
    tasks = []
    for f in sorted(os.listdir(folder)):
        if f.lower().endswith(".png") and not eal.does_asset_exist(f"{DEST}/Textures/{f[:-4]}"):
            t = unreal.AssetImportTask()
            for key, value in {"filename": os.path.join(folder, f), "destination_path": DEST + "/Textures", "automated": True,
                               "replace_existing": True, "save": True}.items():
                t.set_editor_property(key, value)
            tasks.append(t)
    if tasks:
        tools.import_asset_tasks(tasks)
    for n in FACADES:
        normal = eal.load_asset(f"{DEST}/Textures/T_{n}_Normal")
        normal.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        normal.set_editor_property("srgb", False)
        normal.set_editor_property("flip_green_channel", True)  # the maps are written OpenGL-style
        eal.save_loaded_asset(normal)
        rough = eal.load_asset(f"{DEST}/Textures/T_{n}_Roughness")
        rough.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        rough.set_editor_property("srgb", False)
        eal.save_loaded_asset(rough)
    return len(tasks)


def new_material(name):
    return tools.create_asset(name, DEST + "/Materials", unreal.Material, unreal.MaterialFactoryNew())


def constant(mat, value, prop, y):
    node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, y)
    node.set_editor_property("r", value)
    mel.connect_material_property(node, "", prop)


def facade_material(n):
    """Base colour = lerp(texture, texture x vertex colour, texture alpha): the alpha marks what a building's own colour may tint"""
    mat = new_material("M_" + n)
    base = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, -200)
    base.set_editor_property("texture", eal.load_asset(f"{DEST}/Textures/T_{n}_BaseColor"))
    colour = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, 100)
    tinted = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -600, 0)
    mel.connect_material_expressions(base, "RGB", tinted, "A")
    mel.connect_material_expressions(colour, "", tinted, "B")
    mix = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -350, -100)
    mel.connect_material_expressions(base, "RGB", mix, "A")
    mel.connect_material_expressions(tinted, "", mix, "B")
    mel.connect_material_expressions(base, "A", mix, "Alpha")
    mel.connect_material_property(mix, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, 350)
    rough.set_editor_property("texture", eal.load_asset(f"{DEST}/Textures/T_{n}_Roughness"))
    rough.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mel.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    normal = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, 650)
    normal.set_editor_property("texture", eal.load_asset(f"{DEST}/Textures/T_{n}_Normal"))
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if n == "Facade_Curtain":
        constant(mat, 0.4, unreal.MaterialProperty.MP_METALLIC, 200)
    return mat


def flat_material(name, rgb, rough):
    mat = new_material(name)
    node = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -350, 0)
    node.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))
    mel.connect_material_property(node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    constant(mat, rough, unreal.MaterialProperty.MP_ROUGHNESS, 200)
    return mat


def vertex_colour_material(name):
    mat = new_material(name)
    node = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -350, 0)
    mel.connect_material_property(node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    constant(mat, 0.85, unreal.MaterialProperty.MP_ROUGHNESS, 200)
    return mat


def materials():
    """slot name -> material, making the ones that are not there yet"""
    makers = {"M_" + n: (lambda n=n: facade_material(n)) for n in FACADES}
    makers.update({n: (lambda n=n: flat_material(n, *FLAT[n])) for n in FLAT})
    makers.update({n: (lambda n=n: vertex_colour_material(n)) for n in VERTEX_COLOUR})
    out, made = {}, 0
    for name, make in makers.items():
        path = f"{DEST}/Materials/{name}"
        if eal.does_asset_exist(path):
            out[name] = eal.load_asset(path)
            continue
        mat = make()
        mel.recompile_material(mat)
        eal.save_loaded_asset(mat)
        out[name] = mat
        made += 1
    return out, made


# ------------------------------------------------------------------------------------------------------- meshes
def fbx_options(combine, import_materials):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", import_materials)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = ui.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", combine)
    sm.set_editor_property("transform_vertex_to_absolute", True)   # every piece keeps the city's origin
    sm.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    sm.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("generate_lightmap_u_vs", False)        # lighting is dynamic
    sm.set_editor_property("build_nanite", False)                  # not available on Metal SM5
    return ui


def meshes_in(path):
    registry.scan_paths_synchronous([path], True)
    return [a for a in registry.get_assets_by_path(path, recursive=False) if str(a.asset_class_path.asset_name) == "StaticMesh"]


def finish_mesh(mesh, mats, collide):
    slots = mesh.get_editor_property("static_materials")
    for i, slot in enumerate(slots):
        name = str(slot.get_editor_property("material_slot_name"))
        if name in mats:
            mesh.set_material(i, mats[name])
    if collide:
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    return [str(s.get_editor_property("material_slot_name")) for s in slots]


def import_file(fbx, dest, combine, mats, collide, import_materials=False):
    task = unreal.AssetImportTask()
    for key, value in {"filename": fbx, "destination_path": dest, "automated": True, "replace_existing": True, "save": False,
                       "options": fbx_options(combine, import_materials)}.items():
        task.set_editor_property(key, value)
    tools.import_asset_tasks([task])
    slots = set()
    found = meshes_in(dest)
    for a in found:
        slots.update(finish_mesh(a.get_asset(), mats, collide))
    eal.save_directory(dest, only_if_is_dirty=True, recursive=True)
    return len(found), sorted(slots)


def check_bounds(dest, pieces):
    """How many imported pieces are not where the manifest says (more than 1 m out)"""
    wrong, missing = 0, 0
    by_name = {str(a.asset_name): a for a in meshes_in(dest)}
    for name, p in pieces.items():
        a = by_name.get(name)
        if not a:
            missing += 1
            continue
        box = a.get_asset().get_bounding_box()
        want = (p["min"][0] * UNIT, -p["max"][1] * UNIT, p["max"][0] * UNIT, -p["min"][1] * UNIT)
        got = (box.min.x, box.min.y, box.max.x, box.max.y)
        if max(abs(w - g) for w, g in zip(want, got)) > 100.0:
            wrong += 1
            if wrong <= 3:
                say(f"  {name}: expected {tuple(round(v) for v in want)}, imported at {tuple(round(v) for v in got)}")
    return wrong, missing


def import_meshes(mats):
    with open(os.path.join(TILES, "manifest.json"), encoding="utf-8") as fh:
        manifest = json.load(fh)
    by_file = {}
    for name, p in manifest.items():
        by_file.setdefault(p["file"], {})[name] = p
    files = [f for f in sorted(by_file) if f not in SKIP and (not ONLY or f in ONLY)]
    done = 0
    for f in files:
        dest = f"{DEST}/Tiles/{f}"
        pieces = by_file[f]
        if f in REIMPORT and eal.does_directory_exist(dest):
            eal.delete_directory(dest)
        have = len(meshes_in(dest)) if eal.does_directory_exist(dest) else 0
        if have >= len(pieces):
            say(f"{f}: {have} pieces already imported")
            continue
        count, slots = import_file(os.path.join(TILES, f + ".fbx"), dest, False, mats, kind_of(f)[0])
        wrong, missing = check_bounds(dest, pieces)
        say(f"{f}: imported {count} of {len(pieces)} pieces; {wrong} misplaced, {missing} missing; slots {', '.join(slots)}")
        done += 1
        unreal.SystemLibrary.collect_garbage()
    if not ONLY:
        for name, collide in SHARED:
            dest = f"{DEST}/Shared/{name}"
            if name in REIMPORT and eal.does_directory_exist(dest):
                eal.delete_directory(dest)
            if eal.does_directory_exist(dest) and meshes_in(dest):
                say(f"{name}: already imported")
                continue
            count, slots = import_file(os.path.join(PACK, "Shared", name + ".fbx"), dest, True, mats, collide, import_materials=True)
            say(f"{name}: imported as {count} mesh; slots {', '.join(slots)}")
            done += 1
            unreal.SystemLibrary.collect_garbage()
    return done


# -------------------------------------------------------------------------------------------------------- level
ALWAYS_LOADED = ("Roads_", "Road_Markings", "Railways", "Bridges_", "LinkBridge")   # with the shared pieces: never streamed out
DATA_LAYERS = ["DL_Crowds", "DL_Props", "DL_Interiors", "DL_Vehicles"]


def fresh_level(les):
    """A new World Partition level in place of whatever was there: the level is generated, never edited by hand"""
    import shutil
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    if eal.does_asset_exist(LEVEL):
        eal.delete_asset(LEVEL)
    for kind in ("__ExternalActors__", "__ExternalObjects__"):
        shutil.rmtree(os.path.join(content, kind, *LEVEL.split("/")[2:]), ignore_errors=True)
    try:
        les.new_level(LEVEL, is_partitioned_world=True)
        return True
    except TypeError:  # an engine whose Python has no World Partition flag
        les.new_level(LEVEL)
        return False


def make_data_layers():
    """The data layers later content goes into, to switch on and off in the editor. Best effort: says what it managed."""
    made = []
    try:
        subsystem = unreal.get_editor_subsystem(unreal.DataLayerEditorSubsystem)
        for name in DATA_LAYERS:
            path = f"{DEST}/DataLayers/{name}"
            asset = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, DEST + "/DataLayers", unreal.DataLayerAsset, unreal.DataLayerFactory())
            eal.save_loaded_asset(asset)
            params = unreal.DataLayerCreationParameters()
            params.set_editor_property("data_layer_asset", asset)
            if subsystem.create_data_layer_instance(params):
                made.append(name)
    except Exception as error:
        unreal.log_warning(f"LAGOS: data layers: {error}")
    return made


def build_level():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    partitioned = fresh_level(les)
    registry.scan_paths_synchronous([DEST], True)
    counts, always = {}, 0
    assets = [a for a in registry.get_assets_by_path(DEST, recursive=True) if str(a.asset_class_path.asset_name) == "StaticMesh"]
    with unreal.ScopedSlowTask(len(assets), "Placing Lagos") as task:
        for a in sorted(assets, key=lambda x: str(x.package_name)):
            task.enter_progress_frame(1)
            group = str(a.package_path).split("/")[-1]           # the FBX name, or Terrain / Airport / Port
            if group in SKIP:
                continue
            shared = "/Shared/" in str(a.package_path) + "/"
            collide, distance, shadow = (True, 0.0, False) if shared else kind_of(group)
            bridge = not shared and (group.startswith("Bridges_") or group.startswith("LinkBridge"))
            z = -ROAD_Z * BRIDGE_RISE if bridge else FLAT_Z[group][1] - FLAT_Z[group][0] if group in FLAT_Z and not shared else GROUND - LAND_Z
            actor = eas.spawn_actor_from_object(a.get_asset(), unreal.Vector(0.0, 0.0, z), unreal.Rotator(0.0, 0.0, 0.0))
            if bridge:
                actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, BRIDGE_RISE))
            actor.set_editor_property("tags", [unreal.Name(TAG)])
            actor.set_actor_label(str(a.asset_name))
            actor.set_folder_path("Lagos/" + ("Shared" if shared else group))
            comp = actor.static_mesh_component
            comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
            comp.set_editor_property("ld_max_draw_distance", distance)
            comp.set_editor_property("cast_shadow", shadow or (shared and group == "Port"))
            if not collide:
                comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            if shared or group.startswith(ALWAYS_LOADED):
                actor.set_editor_property("is_spatially_loaded", False)   # ground, roads and bridges: always there
                always += 1
            counts[group] = counts.get(group, 0) + 1

    def extra(cls, x, y, z, label, yaw=0.0):
        actor = eas.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        actor.set_editor_property("tags", [unreal.Name(TAG)])
        actor.set_actor_label(label)
        actor.set_folder_path("NaijaHustle")
        actor.set_editor_property("is_spatially_loaded", False)
        return actor

    rig = extra(unreal.NHLightingRig, START["x"], START["y"], 0.0, "LightingRig")
    rig.set_editor_property("preset", unreal.NHLightingPreset.HARSH_MORNING)
    rig.apply_preset(unreal.NHLightingPreset.HARSH_MORNING)
    extra(unreal.PlayerStart, START["x"], START["y"], 120.0, "PlayerStart_Oshodi", START["yaw"])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.NHGameMode.static_class())
    layers = make_data_layers()
    les.save_current_level()
    try:  # one file per actor: every placed piece has a file of its own to write
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    except Exception as error:
        unreal.log_warning(f"LAGOS: could not save every changed file ({error}); use File > Save All")
    say(f"World Partition {'on' if partitioned else 'NOT available: a plain level'}; {always} pieces always loaded, {sum(counts.values()) - always} loaded by cell; data layers: {', '.join(layers) or 'none made'}")
    return counts


def main():
    for folder in (TILES, os.path.join(PACK, "Textures"), os.path.join(PACK, "Shared")):
        if not os.path.isdir(folder):
            raise RuntimeError(f"Lagos model folder not found: {folder} (set LAGOS_TILES and LAGOS_PACK)")
    new_textures = import_textures()
    mats, new_materials = materials()
    say(f"{new_textures} textures imported, {new_materials} materials made ({len(mats)} in all)")
    if STAGE in ("all", "meshes"):
        say(f"mesh stage finished: {import_meshes(mats)} files imported this run")
    if STAGE in ("all", "level"):
        counts = build_level()
        say(f"{LEVEL} built: {sum(counts.values())} pieces placed: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))
    say(f"stage '{STAGE}' done")


if __name__ == "__main__":
    main()
