"""Builds one 600 m cell of the Lagos map out of the modular kit, from OpenStreetMap's building outlines.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>) after import_kit.py.
Environment variables:
  NH_KIT_CELL      "X,Y": the cell, counted in 600 m steps east and north of the map's origin (default -12,16: Oshodi,
                   round Olaiya Street). The same numbering as the city's building tiles (m12_16).
  NH_KIT_FOOTPRINTS  the outlines (default ~/Downloads/map lagos/OSM/buildings_oshodi.json, from build_district_osm.py)
  NH_KIT_UNDO      "1": take the kit buildings out again and bring the cell's old block buildings back

For each outline in the cell it decides, from a seed made of the building's position (so it is the same every run):
  storeys   OpenStreetMap's, where it has them (rarely); otherwise by floor area: small ones low, bigger ones up to 6
  front     the wall nearest a street (Data/osm_<district>.json): shops, doors and balconies go there
and hands the outline, storeys, front and seed to an ANHKitBlock actor, one for each 300 m quarter of the cell, which
puts the building together from kit pieces when it loads. Nothing else is stored: no meshes, no instance lists.

The cell's old buildings (the plain blocks from the Blender model) are marked editor-only, so the game does not load
them; NH_KIT_UNDO reverses that.
"""
import json
import math
import os
import random
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
CELL = [int(v) for v in os.environ.get("NH_KIT_CELL", "-12,16").split(",")]
FOOTPRINTS = os.environ.get("NH_KIT_FOOTPRINTS", os.path.join(os.path.expanduser("~"), "Downloads", "map lagos", "OSM", "buildings_oshodi.json"))
UNDO = os.environ.get("NH_KIT_UNDO", "") == "1"
LEVEL = "/Game/NaijaHustle/Maps/L_Lagos_City"
SIZE, QUARTER = 60000.0, 30000.0
GROUND = -16.0          # where the land lies in the level (import_lagos_city.py)
X0, Y0 = CELL[0] * SIZE, -(CELL[1] + 1) * SIZE          # the cell's corner in the level: X east, Y south
TAG = "__" + "_".join(("m" if v < 0 else "") + str(abs(v)) for v in CELL)       # how the city's tiles name this cell


def say(text):
    unreal.log("KITBLOCK: " + text)


def area(points):
    return abs(sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(points, points[1:] + points[:1]))) / 2.0


def to_segment(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / max(1.0, dx * dx + dy * dy)))
    return math.hypot(p[0] - a[0] - dx * t, p[1] - a[1] - dy * t)


def storeys(building, square_m, rnd):
    if building.get("floors"):
        return max(1, min(12, building["floors"]))
    if building.get("height"):
        return max(1, min(12, round(building["height"] / 3.0)))
    if square_m < 60:
        return 1
    table = [(1, 35), (2, 45), (3, 20)] if square_m < 200 else [(2, 35), (3, 35), (4, 20), (5, 10)] if square_m < 600 else [(1, 40), (2, 30), (3, 15), (4, 8), (6, 7)]
    roll, total = rnd.uniform(0, 100), 0
    for floors, share in table:
        total += share
        if roll <= total:
            return floors
    return table[-1][0]


def front_edge(points, segments):
    """The wall, at least 2.5 m long, whose middle is nearest a street"""
    best, best_d = -1, 1e12
    for i, (a, b) in enumerate(zip(points, points[1:] + points[:1])):
        if math.hypot(b[0] - a[0], b[1] - a[1]) < 250:
            continue
        mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
        d = min((to_segment(mid, s0, s1) for s0, s1 in segments), default=1e12)
        if d < best_d:
            best, best_d = i, d
    return best


def main():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.load_level(LEVEL)
    box = unreal.Box(unreal.Vector(X0, Y0, -5000), unreal.Vector(X0 + SIZE, Y0 + SIZE, 30000))
    descs = unreal.WorldPartitionBlueprintLibrary.get_intersecting_actor_descs(box)
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    old, gone = 0, 0
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith("Buildings_") and label.endswith(TAG):
            actor.set_editor_property("is_editor_only_actor", not UNDO)
            actor.modify()
            old += 1
        elif label.startswith(f"NHKit{TAG}"):
            unreal.EditorLevelLibrary.destroy_actor(actor)
            gone += 1
    if UNDO:
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        say(f"cell {CELL}: {gone} kit blocks removed, {old} old building tiles brought back")
        return

    with open(FOOTPRINTS, encoding="utf-8") as fh:
        buildings = json.load(fh)["buildings"]
    segments = []
    for name in os.listdir(os.path.join(HERE, "..", "Data")):
        if name.startswith("osm_") and name.endswith(".json"):
            with open(os.path.join(HERE, "..", "Data", name), encoding="utf-8") as fh:
                for street in json.load(fh)["streets"].values():
                    pts = street["pts"]
                    segments += [(a, b) for a, b in zip(pts, pts[1:]) if X0 - 10000 < a[0] < X0 + SIZE + 10000 and Y0 - 10000 < a[1] < Y0 + SIZE + 10000]
    blocks, skipped, floors_seen = {}, 0, {}
    for b in buildings:
        pts = b["pts"]
        cx, cy = sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts)
        if not (X0 <= cx < X0 + SIZE and Y0 <= cy < Y0 + SIZE):
            continue
        square_m = area(pts) / 10000.0
        if len(pts) < 3 or square_m < 9:
            skipped += 1
            continue
        seed = (int(cx) * 73856093 ^ int(cy) * 19349663) & 0x7FFFFFF
        rnd = random.Random(seed)
        spec = unreal.NHKitBuilding()
        key = (int((cx - X0) // QUARTER), int((cy - Y0) // QUARTER))
        ox, oy = X0 + (key[0] + 0.5) * QUARTER, Y0 + (key[1] + 0.5) * QUARTER
        spec.set_editor_property("footprint", [unreal.Vector2D(p[0] - ox, p[1] - oy) for p in pts])
        spec.set_editor_property("floors", storeys(b, square_m, rnd))
        spec.set_editor_property("front_edge", front_edge(pts, segments))
        spec.set_editor_property("seed", seed)
        blocks.setdefault(key, []).append(spec)
        floors_seen[spec.floors] = floors_seen.get(spec.floors, 0) + 1
    instances = 0
    for key, specs in sorted(blocks.items()):
        where = unreal.Vector(X0 + (key[0] + 0.5) * QUARTER, Y0 + (key[1] + 0.5) * QUARTER, 0.0)
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.NHKitBlock, where, unreal.Rotator(0, 0, 0))
        actor.set_actor_label(f"NHKit{TAG}_{key[0]}{key[1]}")
        actor.set_folder_path("Kit")
        actor.set_editor_property("ground_z", GROUND)
        actor.set_editor_property("buildings", specs)
        actor.rebuild()
        instances += actor.instance_count()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    count = sum(len(v) for v in blocks.values())
    say(f"cell {CELL} ({X0:.0f}, {Y0:.0f}): {count} buildings in {len(blocks)} blocks, {instances} kit pieces placed; "
        f"storeys {', '.join(f'{k}: {v}' for k, v in sorted(floors_seen.items()))}; {skipped} outlines under 9 square metres left out; "
        f"{old} old building tiles switched off; {len(segments)} street segments used to find fronts")


main()
