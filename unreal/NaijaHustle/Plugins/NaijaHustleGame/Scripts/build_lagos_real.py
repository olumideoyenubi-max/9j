"""Makes Data/lagos_real.json: the game's map data for the real-scale Lagos level (L_Lagos_City).

  python3 build_lagos_real.py [folder for the OpenStreetMap downloads]     (default ~/Downloads/map lagos/OSM)

Plain Python, run outside Unreal. It downloads (once) the main roads, bus stops and place names of the model's area
from OpenStreetMap through the Overpass API, and writes:

  roads      the road graph: nodes in Unreal cm and ways (class, one-way, bridge, name, node list, and 'l' where a
             one-way road's lanes must stop short of the other carriageway on its left). The traffic drives along it
             and the minimap draws it. `build_lagos_real.py --lanes` works out the 'l's again without downloading.
  busStops   the game's eleven stops at their real places, each moved to the kerb of the nearest main road
  park, parkBays, playerStart   Oshodi: the motor park is a row of bays on the verge just past the Oshodi stop
  districts  place names with a point each; the nearest one names where you are

Same projection as the Blender model (lagos_city_detailed.blend): origin 3.40 E, 6.47 N, flat, 110,611 m per degree
of longitude and 110,574 m per degree of latitude; Blender north is Unreal -Y. Checked against the model: 89% of
points sampled along these roads land on its road surface (the rest are bridges and roads changed since).
Road widths are the model's own, measured per class.

(c) OpenStreetMap contributors, ODbL: credit them in anything published.
"""
import json
import math
import os
import sys
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.expanduser("~"), "Downloads", "map lagos", "OSM")
OUT = os.path.join(HERE, "..", "Data", "lagos_real.json")
BBOX = "6.39,3.289,6.612,3.511"
LON0, LAT0, KX, KY = 3.40, 6.47, 110611.0, 110574.0
CLASSES = ["motorway", "trunk", "primary", "secondary", "tertiary", "link"]
HALF_WIDTH = [1200, 575, 875, 650, 500, 375]      # cm, half the paved width of one carriageway in the model
QUERIES = {
    "major.json": f'[out:json][timeout:180];(way["highway"~"^(motorway|trunk|primary|secondary|tertiary|motorway_link|trunk_link|primary_link|secondary_link|tertiary_link)$"]({BBOX}););out body geom;',
    "places.json": f'[out:json][timeout:120];(node["place"~"suburb|neighbourhood|quarter|town|city"]({BBOX}););out body;',
}
# stop id (naija_rules.json) -> name, latitude, longitude, agbero ticket. Oshodi, Anthony, Fadeyi, Yaba and CMS are
# OpenStreetMap bus stops; the others are placed by hand on the right road and are approximate.
STOPS = [("oshoja", "Oshodi", 6.55725, 3.35141, 400), ("iya", "Charity", 6.5535, 3.3400, 0), ("second", "Anthony", 6.55897, 3.36696, 0),
         ("lagoon", "Gbagada", 6.5567, 3.3860, 0), ("bridge", "Iyana Oworo", 6.5456, 3.4010, 500), ("eko", "CMS", 6.4495, 3.38978, 0),
         ("marketrd", "Fadeyi", 6.52492, 3.36769, 0), ("balogate", "Yaba", 6.51147, 3.3700, 600), ("marketsq", "Tejuosho Market", 6.5075, 3.3668, 0),
         ("ebute", "Olosha", 6.5300, 3.3530, 0), ("church", "Idi-Oro", 6.5219, 3.3564, 0)]


# Places the car trade needs, in the Ladipo spare-parts area of Mushin (invented businesses at roughly real spots):
# id, name, latitude, longitude. Each is put on the verge of the nearest main road.
PLACES = [("mechanic", "Shina Garage, Mechanic Village", 6.5389, 3.3476), ("paint", "Baba Colour Roadside Paint", 6.5452, 3.3528),
          ("chop", "Ladipo Chop Shop", 6.5352, 3.3442)]


def fetch(name):
    path = os.path.join(CACHE, name)
    if not os.path.exists(path):
        os.makedirs(CACHE, exist_ok=True)
        request = urllib.request.Request("https://overpass-api.de/api/interpreter", data=urllib.parse.urlencode({"data": QUERIES[name]}).encode(),
                                         headers={"User-Agent": "NaijaHustle-map-import/1.0 (personal game project)", "Accept": "*/*"})
        with urllib.request.urlopen(request, timeout=300) as reply, open(path, "wb") as out:
            out.write(reply.read())
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)["elements"]


def project(lat, lon):
    """Unreal cm: X east, Y south"""
    return round((lon - LON0) * KX * 100.0), round(-(lat - LAT0) * KY * 100.0)


def build_roads():
    nodes, index, ways = [], {}, []
    for way in fetch("major.json"):
        tags = way["tags"]
        kind = tags["highway"]
        cls = CLASSES.index("link") if kind.endswith("_link") else CLASSES.index(kind)
        ids = []
        for osm_id, point in zip(way["nodes"], way["geometry"]):
            if osm_id not in index:
                index[osm_id] = len(nodes)
                nodes.append(project(point["lat"], point["lon"]))
            if not ids or ids[-1] != index[osm_id]:
                ids.append(index[osm_id])
        if len(ids) < 2:
            continue
        oneway = tags.get("oneway") in ("yes", "1", "true") or tags.get("junction") == "roundabout" or kind in ("motorway", "motorway_link")
        if tags.get("oneway") == "-1":
            ids.reverse()
            oneway = True
        bridge = tags.get("bridge", "no") != "no"
        ways.append({"c": cls, "o": int(oneway), "b": int(bridge), "name": tags.get("name", ""), "n": ids})
    return nodes, ways


def nearest(nodes, ways, x, y, allow):
    """The closest point on a way of an allowed class: (distance, point, unit direction of travel, class)"""
    best = None
    for way in ways:
        if way["c"] not in allow:
            continue
        for a, b in zip(way["n"], way["n"][1:]):
            ax, ay = nodes[a]
            bx, by = nodes[b]
            dx, dy = bx - ax, by - ay
            length2 = dx * dx + dy * dy
            if length2 < 1:
                continue
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / length2))
            px, py = ax + dx * t, ay + dy * t
            d = math.hypot(px - x, py - y)
            if best is None or d < best[0]:
                length = math.sqrt(length2)
                best = (d, (px, py), (dx / length, dy / length), way["c"])
    return best


def lane_limits(nodes, ways):
    """Gives a one-way road with the other carriageway close on its left an 'l': how far, cm, its lanes may reach that way.

    A dual carriageway is two one-way roads on the map. Taken at its class's full width each, their lanes lay over each
    other wherever the two are closer together than that width (on the Eko Bridge, 12 m apart and 24 m wide, the fast
    lane of each side was the fast lane of the other, head on). With 'l', a road's lanes stop 40 cm short of the line
    midway between the two."""
    segs, cell = {}, 6000.0
    for k, way in enumerate(ways):
        way.pop("l", None)
        for a, b in zip(way["n"], way["n"][1:]):
            (ax, ay), (bx, by) = nodes[a], nodes[b]
            length = math.hypot(bx - ax, by - ay)
            if length < 1:
                continue
            for c in range(int(length // 2000) + 1):       # a point every 20 m or so
                t = (c + 0.5) / (int(length // 2000) + 1)
                x, y = ax + (bx - ax) * t, ay + (by - ay) * t
                segs.setdefault((int(x // cell), int(y // cell)), []).append((x, y, (bx - ax) / length, (by - ay) / length, k))
    changed = 0
    for k, way in enumerate(ways):
        if not way["o"]:
            continue
        gaps = []
        for a, b in zip(way["n"], way["n"][1:]):
            (ax, ay), (bx, by) = nodes[a], nodes[b]
            length = math.hypot(bx - ax, by - ay)
            if length < 1:
                continue
            ux, uy = (bx - ax) / length, (by - ay) / length
            for c in range(int(length // 2000) + 1):
                t = (c + 0.5) / (int(length // 2000) + 1)
                x, y = ax + (bx - ax) * t, ay + (by - ay) * t
                best = None
                for cx in (-1, 0, 1):
                    for cy in (-1, 0, 1):
                        for ox, oy, vx, vy, other in segs.get((int(x // cell) + cx, int(y // cell) + cy), ()):
                            # going the other way, and on the driver's left (Unreal: X east, Y south, so right is (-uy, ux))
                            side = (ox - x) * -uy + (oy - y) * ux
                            if other == k or ux * vx + uy * vy > -0.85 or side > -200 or abs((ox - x) * ux + (oy - y) * uy) > 1500:
                                continue
                            best = -side if best is None else min(best, -side)
                gaps.append(best)
        near = sorted(g for g in gaps if g is not None)
        if len(near) * 2 < len(gaps) or not near:           # beside the other carriageway for under half its length: left alone
            continue
        left = max(300, round(near[len(near) // 5] / 2 - 40))
        if left < HALF_WIDTH[way["c"]]:
            way["l"] = left
            changed += 1
    return changed


def main():
    if "--lanes" in sys.argv:                               # only work out the lane limits again, on the file as it is
        with open(OUT, encoding="utf-8") as fh:
            out = json.load(fh)
        flat = out["roads"]["nodes"]
        changed = lane_limits([(flat[i], flat[i + 1]) for i in range(0, len(flat), 2)], out["roads"]["ways"])
        with open(OUT, "w", encoding="utf-8") as fh:
            json.dump(out, fh, separators=(",", ":"), ensure_ascii=False)
        print(f"{os.path.normpath(OUT)}: {changed} one-way roads given a lane limit beside their other carriageway")
        return
    nodes, ways = build_roads()
    lane_limits(nodes, ways)
    main_roads = {CLASSES.index(c) for c in ("trunk", "primary", "secondary", "tertiary")}
    stops, notes = [], []
    for stop_id, name, lat, lon, agbero in STOPS:
        x, y = project(lat, lon)
        d, (px, py), (ux, uy), cls = nearest(nodes, ways, x, y, main_roads)
        rx, ry = -uy, ux                                   # the driver's right, in Unreal's left-handed X east, Y south
        if (x - px) * rx + (y - py) * ry < 0:              # the stop is on the other side: serve it going the other way
            ux, uy, rx, ry = -ux, -uy, -rx, -ry
        half = HALF_WIDTH[cls]
        stops.append({"id": stop_id, "name": name, "kerb": [round(px + rx * (half - 180)), round(py + ry * (half - 180))],
                      "wait": [round(px + rx * (half + 200)), round(py + ry * (half + 200))], "agbero": agbero,
                      "yaw": round(math.degrees(math.atan2(uy, ux)), 1)})
        notes.append(f"{name} {d / 100:.0f} m to a {CLASSES[cls]} road")
    # Oshodi Motor Park: bays nose to tail on the verge beside the road (clear of the traffic's lanes), starting 30 m past the Oshodi stop
    oshodi = stops[0]
    yaw = math.radians(oshodi["yaw"])
    ux, uy = math.cos(yaw), math.sin(yaw)
    rx, ry = -uy, ux
    kx, ky = oshodi["kerb"]
    bays = [{"n": i + 1, "x": round(kx + ux * (3000 + 800 * i) + rx * 400), "y": round(ky + uy * (3000 + 800 * i) + ry * 400), "yaw": oshodi["yaw"]} for i in range(8)]
    park = [round(kx + ux * 2200 + rx * 900), round(ky + uy * 2200 + ry * 900)]
    start = {"x": round(kx + ux * 1800 + rx * 1000), "y": round(ky + uy * 1800 + ry * 1000), "z": 120, "yaw": oshodi["yaw"], "note": "on the verge by Oshodi Motor Park"}
    places = []
    for place_id, name, lat, lon in PLACES:
        x, y = project(lat, lon)
        d, (px, py), (ux, uy), cls = nearest(nodes, ways, x, y, main_roads)
        rx, ry = -uy, ux
        places.append({"id": place_id, "name": name, "x": round(px + rx * (HALF_WIDTH[cls] + 350)), "y": round(py + ry * (HALF_WIDTH[cls] + 350))})
    districts = [{"name": p["tags"]["name"], "x": project(p["lat"], p["lon"])[0], "y": project(p["lat"], p["lon"])[1]}
                 for p in fetch("places.json") if p["tags"].get("name") and p["tags"]["place"] != "city"]
    out = {"format": "naija-hustle-real-city", "version": 1, "units": "cm", "axes": "X east, Y south, Z up (Unreal)",
           "credit": "(c) OpenStreetMap contributors, ODbL", "origin": {"lon": LON0, "lat": LAT0},
           "roads": {"classes": CLASSES, "halfWidth": HALF_WIDTH, "nodes": [c for n in nodes for c in n], "ways": ways},
           "busStops": stops, "places": places, "park": park, "parkBays": bays, "playerStart": start, "districts": districts}
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(out, fh, separators=(",", ":"), ensure_ascii=False)
    length = sum(math.hypot(nodes[b][0] - nodes[a][0], nodes[b][1] - nodes[a][1]) for w in ways for a, b in zip(w["n"], w["n"][1:])) / 100000.0
    print(f"{os.path.normpath(OUT)}: {len(nodes)} nodes, {len(ways)} ways, {length:.0f} km of road, {len(stops)} stops, {len(bays)} bays, {len(districts)} districts, {os.path.getsize(OUT) // 1000} KB")
    print("stops: " + "; ".join(notes))
    print(f"start {start['x']}, {start['y']} facing {start['yaw']}")


if __name__ == "__main__":
    main()
