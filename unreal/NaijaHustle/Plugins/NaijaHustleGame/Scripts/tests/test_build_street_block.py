"""Runs build_street_block.py against the stand-in `unreal` module and checks what it would place.
    python3 unreal/NaijaHustle/Scripts/tests/test_build_street_block.py"""
import importlib.util
import json
import math
import os
import sys

HERE = os.path.dirname(__file__)
sys.path.insert(0, HERE)
import unreal_stub as unreal  # noqa: E402  (installs itself as `unreal`)

spec = importlib.util.spec_from_file_location("build_street_block", os.path.join(HERE, "..", "build_street_block.py"))
bsb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bsb)

fails = 0
def ok(name, cond, info=None):
    global fails
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  {info}"))
    fails += 0 if cond else 1

data = json.load(open(bsb.DATA, encoding="utf-8"))
count = bsb.main()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).actors
tiles = [a for a in actors if isinstance(a, unreal.NHCityTile)]
blds = [a for a in actors if isinstance(a, unreal.NHBlockoutBuilding)]
kinds = {}
for b in blds:
    k = b.get_editor_property("kind")
    kinds[k] = kinds.get(k, 0) + 1
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

ok("creates a World Partition level and saves it", les.calls[0] == ("new", bsb.LEVEL, True) and les.calls[-1] == ("save",), les.calls)
ok("24 city tiles of 16 x 16 cells, each with its one-cell border", len(tiles) == 24 and all(len(t.get_editor_property("cells")) == 18 * 18 == len(t.get_editor_property("dusty_mask")) for t in tiles), len(tiles))
tot = lambda key: sum(len(t.get_editor_property(key)) for t in tiles)
n_props = sum(len(p["items"]) for p in data["props"].values())
ok("every prop, sign, shop front, lamp and road marking lands in exactly one tile",
   (tot("props"), tot("signs"), tot("shopfronts"), tot("lamp_heads"), tot("markings")) == (n_props, len(data["signs"]), len(data["shopfronts"]), len(data["lamps"]), len(data["markings"])),
   (tot("props"), tot("signs"), tot("shopfronts"), tot("lamp_heads"), tot("markings")))
ok("one actor per building plus 3 fuel stations, 11 bus shelters and a footbridge",
   len(blds) == len(data["buildings"]) + 3 + 11 + 1 and kinds.get("NHBuildingKind.FUEL_STATION") == 3 and kinds.get("NHBuildingKind.BUS_SHELTER") == 11 and kinds.get("NHBuildingKind.FOOTBRIDGE") == 1, kinds)
ok("every actor is tagged, labelled, foldered and rebuilt once after its data is set",
   all(a.actor_has_tag(bsb.TAG) and a.label and a.folder for a in actors) and all(a.rebuilt == 1 for a in tiles + blds))

# coordinates: a tile's cells match the source grid, buildings sit on their footprint centre at pavement height
t = next(t for t in tiles if t.get_editor_property("col0") == 16 and t.get_editor_property("row0") == 16)
cells = t.get_editor_property("cells")
ok("tile cells are the map's cells (row 16, cols 15..32)", cells[18:36] == data["tiles"][16][15:33], (cells[18:36], data["tiles"][16][15:33]))
b0, a0 = data["buildings"][0], next(a for a in blds if a.label.endswith("_000"))
ok("a building's actor is at its footprint centre on the pavement, with its size and height",
   (a0.loc.x, a0.loc.y, a0.loc.z) == (b0["x"] + b0["w"] / 2, b0["y"] + b0["d"] / 2, 16) and a0.get_editor_property("size").x == b0["w"] and a0.get_editor_property("height") == b0["h"])
stilts = [a for a in blds if a.get_editor_property("kind") == "NHBuildingKind.STILT"]
ok("stilt houses stand in the lagoon (z = water level)", stilts and all(a.loc.z == -130 for a in stilts), len(stilts))

# street faces: a building next to a pavement cell faces it
def faces_ok():
    bad = 0
    for i, b in enumerate(data["buildings"][:200]):
        f = bsb.street_faces(bsb.City(data), b)
        c1 = (b["x"] + b["w"]) // 400
        r0 = b["y"] // 400
        east = data["tiles"][r0][c1] if c1 < 96 else "#"
        if (east in "RKFGP") != bool(f & 1):
            # the east flag looks at every row of the footprint; only a mismatch where row 0 says "open" but no flag counts
            bad += 1 if east in "RKFGP" else 0
    return bad
ok("street faces point at pavements and roads", faces_ok() == 0, faces_ok())

# walk-in shops
shops = [a for a in blds if a.get_editor_property("enterable_shop")]
ok("6 walk-in shops near the start, each on a house's street face, with their fronts left open",
   len(shops) == 6 and all(a.get_editor_property("shop_face") & a.get_editor_property("street_faces") for a in shops)
   and sum(1 for t in tiles for f in t.get_editor_property("shopfronts") if f.get_editor_property("enterable")) == 6,
   [(a.label, a.get_editor_property("shop_face"), a.get_editor_property("street_faces")) for a in shops])
ps = next(a for a in actors if isinstance(a, unreal.PlayerStart))
sx, sy = data["playerStart"]["x"], data["playerStart"]["y"]
ok("walk-in shops are close to the player start (within 250 m)", all(math.hypot(a.loc.x - sx, a.loc.y - sy) < 25000 for a in shops), [(round(a.loc.x), round(a.loc.y)) for a in shops])

# props: fitted transform for one known item
pole = data["props"]["pole"]
x, y, z, qx, qy, qz, qw, s1, s2, s3, col = pole["items"][0]
p = next(p for t in tiles for p in t.get_editor_property("props") if p.get_editor_property("kind") == "pole" and abs(p.get_editor_property("transform").translation.x - x) < 0.01 and abs(p.get_editor_property("transform").translation.y - y) < 0.01)
sc = p.get_editor_property("transform").scale3d
ok("a prop's box is its base shape times the instance scale, in BasicShapes units", abs(sc.z - pole["size"][2] * s3 / 100) < 1e-6 and abs(sc.x - pole["size"][0] * s1 / 100) < 1e-6, (sc.x, sc.y, sc.z))
ok("poles are solid cylinders; chairs are not solid", p.get_editor_property("shape") == "NHShape.CYLINDER" and p.get_editor_property("solid") is True
   and all(not q.get_editor_property("solid") for t in tiles for q in t.get_editor_property("props") if q.get_editor_property("kind") == "chair"))

# signs: no glyphs the default font lacks
ok("sign text avoids characters the default font lacks", all("₦" not in s.get_editor_property("sub") and "·" not in s.get_editor_property("sub") for t in tiles for s in t.get_editor_property("signs")))
rig = next(a for a in actors if isinstance(a, unreal.NHLightingRig))
ok("the level's GameMode Override is NHGameMode", "NHGameMode" in str(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).world.settings.get_editor_property("default_game_mode")))
ok("lighting rig starts on the harsh morning; player starts at home by the motor park", rig.get_editor_property("_applied") == "NHLightingPreset.HARSH_MORNING" and (ps.loc.x, ps.loc.y) == (sx, sy))

# surface types: every prop says what it is made of
kinds_of = {}
for t in tiles:
    for q in t.get_editor_property("props"):
        kinds_of.setdefault(q.get_editor_property("kind"), set()).add(q.get_editor_property("surface").get_editor_property("type"))
ok("every prop's surface has one type per kind; planks are wood, generators metal, sand heaps dirt, goods generic",
   all(len(v) == 1 for v in kinds_of.values()) and kinds_of["plank"] == {"NHSurfaceType.WOOD"} and kinds_of["gen"] == {"NHSurfaceType.METAL"}
   and kinds_of["sand"] == {"NHSurfaceType.DIRT"} and kinds_of["goods"] == {"NHSurfaceType.GENERIC"}, {k: v for k, v in kinds_of.items() if len(v) != 1})
ok("every prop kind with a type is a real prop kind", set(bsb.PROP_TYPE) <= set(data["props"]), set(bsb.PROP_TYPE) - set(data["props"]))

# overhead cables
city = bsb.City(data)
poles = bsb.utility_poles(data)
spans = bsb.cable_spans(city, data)
cable_actors = [a for a in actors if isinstance(a, unreal.NHCables)]
placed = [c for a in cable_actors for c in a.get_editor_property("spans")]
ok("utility poles are picked out of the 'pole' props (not lamp posts, masts or table legs)", 60 <= len(poles) <= 140 and all(880 <= p[2] <= 1000 for p in poles), len(poles))
ok("every cable lands in exactly one NHCables actor, rebuilt once", len(placed) == len(spans) > 100 and all(a.rebuilt == 1 for a in cable_actors), (len(placed), len(spans)))
lines = [s_ for s_ in spans if s_[3] == 3.0]
drops = [s_ for s_ in spans if s_[3] == 2.0]
runs = [math.hypot(a[0] - b[0], a[1] - b[1]) for a, b, _, _ in lines]
ok("power lines join poles 8 to 16 m apart along one street, with slack", lines and all(790 <= r <= 1610 for r in runs)
   and all(abs(a[0] - b[0]) < 151 or abs(a[1] - b[1]) < 151 for a, b, _, _ in lines) and all(0.02 < s_[2] < 0.12 for s_ in spans), (min(runs), max(runs)))
def per_pole(pred):
    out = {}
    for a, b, _, _ in drops:
        key = (a[0], a[1])
        if pred(key):
            out[key] = out.get(key, 0) + 1
    return out
in_dusty = lambda k: city.is_dusty(int(k[0] // 400), int(k[1] // 400))
ok("service drops reach a nearby wall below the pole top: up to 2 per pole in the dusty quarter, 1 elsewhere",
   drops and all(a[2] > b[2] >= 250 for a, b, _, _ in drops) and max(per_pole(in_dusty).values(), default=0) == 2
   and max(per_pole(lambda k: not in_dusty(k)).values(), default=0) == 1, (len(drops), per_pole(in_dusty)))
def bundle(pred):
    nearest = lambda q: min(poles, key=lambda p_: math.hypot(p_[0] - q[0], p_[1] - q[1]))[:2]
    pairs = {}
    for a, b, _, _ in lines:
        if pred(nearest(a)):
            key = (nearest(a), nearest(b))
            pairs[key] = pairs.get(key, 0) + 1
    return set(pairs.values())
ok("three lines per span, five in the dusty quarter", bundle(lambda k: not in_dusty(k)) == {3} and bundle(in_dusty) == {5}, (bundle(lambda k: not in_dusty(k)), bundle(in_dusty)))

ok("loads the level's World Partition actors before clearing, and saves every changed file after", len(unreal.WorldPartitionBlueprintLibrary.loaded) == 1 and unreal.EditorLoadingAndSavingUtils.saves == 1)

# reflection captures
caps = [a for a in actors if isinstance(a, unreal.SphereReflectionCapture)]
js = bsb.junctions(city)
ok("one reflection capture over every road junction, above head height, reaching the next junction's", 20 <= len(js) == len(caps) <= 120 and all(a.loc.z == 350 and data["tiles"][int(a.loc.y // 400)][int(a.loc.x // 400)] == "R"
   and a.get_editor_property("capture_component").get_editor_property("influence_radius") >= 3000 for a in caps), (len(js), len(caps)))

# second run: removes its own actors first
before = len(actors)
bsb.main()
ok("running again replaces the blockout instead of doubling it", len(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).actors) == before, (before, len(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).actors)))

print("ALL PASS" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
