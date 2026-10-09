"""Turns OpenStreetMap's record of one district into the game's street data: every street with its real name.

  python3 build_district_osm.py [district] [folder for the OpenStreetMap downloads]
      district: a key of DISTRICTS below (default oshodi); downloads default to ~/Downloads/map lagos/OSM

Plain Python, run outside Unreal. It downloads the district once through the Overpass API (roads of every class,
railways, waterways and water, parks, bus stops, building footprints) and writes, next to the plugin's other data:

  Data/streets_<district>.csv    one row a street segment (an OpenStreetMap way): name, class, lanes, width, one-way,
                                 bridge, roundabout, speed limit, length. Scripts/import_streets_table.py makes the
                                 DataTable DT_Streets_<District> from it (row struct FNHStreetRow); the row name is
                                 "W" + the OpenStreetMap way id.
  Data/osm_<district>.json       the same streets with their centre lines in Unreal cm (keyed by the same row name:
                                 this is the link between a table row and the road it describes), junctions, and
                                 the district's rail lines, water, parks and bus stops.

and, in the downloads folder (not in the project: it is source for the building generator, not game data):

  buildings_<district>.json      every building footprint in Unreal cm, with floors and height where OpenStreetMap
                                 has them (it rarely does in Lagos: a few dozen of tens of thousands)

Same projection as the Blender model and build_lagos_real.py: origin 3.40 E, 6.47 N, Blender north is Unreal -Y.
Widths: OpenStreetMap has no width for any Lagos street and a lane count for few. Where lanes are given the width is
3.3 m a lane plus a metre of margin; otherwise it is the class's usual width, taken from the city model.

(c) OpenStreetMap contributors, ODbL: credit them in anything published. Street names are real; nothing else here is.
"""
import csv
import json
import math
import os
import sys
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
DISTRICTS = {"oshodi": ("Oshodi", (6.535, 3.325, 6.575, 3.365))}     # name, (south, west, north, east)
DISTRICT = sys.argv[1] if len(sys.argv) > 1 else "oshodi"
CACHE = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.expanduser("~"), "Downloads", "map lagos", "OSM")
LON0, LAT0, KX, KY = 3.40, 6.47, 110611.0, 110574.0
# the classes cars use, in the order the game ranks them, and the width of each in the city model, m
DRIVE = ["motorway", "trunk", "primary", "secondary", "tertiary", "unclassified", "residential", "living_street", "service", "track"]
WIDTH = {"motorway": 24.0, "trunk": 11.5, "primary": 17.5, "secondary": 13.0, "tertiary": 10.0, "unclassified": 7.5, "residential": 7.5,
         "living_street": 6.0, "service": 5.0, "track": 4.0, "link": 7.5}
WALK = ["footway", "path", "steps", "pedestrian", "platform"]


def fetch(name, bbox):
    path = os.path.join(CACHE, f"{name}.json")
    if not os.path.exists(path):
        box = ",".join(str(v) for v in bbox)
        query = (f'[out:json][timeout:240];(way["highway"]({box});way["railway"]({box});way["waterway"]({box});way["natural"="water"]({box});'
                 f'way["leisure"~"park|pitch|garden|playground"]({box});way["building"]({box});node["highway"~"bus_stop|traffic_signals|crossing"]({box});'
                 f'node["public_transport"]({box});node["amenity"~"bus_station|fuel|marketplace"]({box}););out body geom;')
        os.makedirs(CACHE, exist_ok=True)
        request = urllib.request.Request("https://overpass-api.de/api/interpreter", data=urllib.parse.urlencode({"data": query}).encode(),
                                         headers={"User-Agent": "NaijaHustle-map-import/1.0 (personal game project)", "Accept": "*/*"})
        with urllib.request.urlopen(request, timeout=300) as reply, open(path, "wb") as out:
            out.write(reply.read())
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)["elements"]


def project(point):
    """Unreal cm: X east, Y south"""
    return [round((point["lon"] - LON0) * KX * 100.0), round(-(point["lat"] - LAT0) * KY * 100.0)]


def line(way):
    return [project(p) for p in way["geometry"]]


def length_m(points):
    return sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(points, points[1:])) / 100.0


def number(text, default=0.0):
    try:
        return float(str(text).split()[0].replace(",", "."))
    except (ValueError, IndexError):
        return default


def main():
    title, bbox = DISTRICTS[DISTRICT]
    elements = fetch(DISTRICT, bbox)
    ways = [e for e in elements if e["type"] == "way"]
    streets, walks, node_use, rows = {}, [], {}, []
    for way in ways:
        tags = way["tags"]
        kind = tags.get("highway")
        if not kind:
            continue
        link = kind.endswith("_link")
        base = kind[:-5] if link else kind
        if base in WALK:
            walks.append({"kind": base, "name": tags.get("name", ""), "pts": line(way)})
            continue
        if base not in DRIVE:
            continue
        lanes = int(number(tags.get("lanes"), 0))
        oneway = tags.get("oneway") in ("yes", "1", "true", "-1") or tags.get("junction") == "roundabout" or base == "motorway"
        width = round(lanes * 3.3 + 1.0, 1) if lanes else WIDTH["link" if link else base]
        points = line(way)
        if tags.get("oneway") == "-1":
            points.reverse()
        row = "W%d" % way["id"]
        streets[row] = {"name": tags.get("name", ""), "class": base, "link": int(link), "lanes": lanes, "width": width, "oneway": int(oneway),
                        "bridge": int(tags.get("bridge", "no") != "no"), "tunnel": int(tags.get("tunnel", "no") != "no"),
                        "roundabout": int(tags.get("junction") == "roundabout"), "pts": points}
        for node in way["nodes"]:
            node_use.setdefault(node, set()).add(row)
        rows.append({"Name": row, "StreetName": tags.get("name", ""), "RoadClass": base, "bLink": str(link).lower(), "Lanes": lanes, "WidthM": width,
                     "bOneWay": str(oneway).lower(), "bBridge": str(tags.get("bridge", "no") != "no").lower(),
                     "bRoundabout": str(tags.get("junction") == "roundabout").lower(), "SpeedLimitKmh": int(number(tags.get("maxspeed"), 0)),
                     "LengthM": round(length_m(points), 1), "District": title, "OsmWayId": str(way["id"])})
    # junctions: nodes where three or more streets meet, or two with different names
    where = {}
    for way in ways:
        if "highway" in way["tags"]:
            for node, point in zip(way["nodes"], way["geometry"]):
                where[node] = project(point)
    junctions = []
    for node, users in node_use.items():
        names = sorted({streets[u]["name"] for u in users if streets[u]["name"]})
        if len(users) >= 3 or len(names) >= 2:
            junctions.append({"at": where[node], "streets": names, "ways": len(users)})

    def others(test, extra=lambda tags: {}):
        return [dict({"name": w["tags"].get("name", ""), "pts": line(w)}, **extra(w["tags"])) for w in ways if test(w["tags"])]

    rail = others(lambda t: "railway" in t and "highway" not in t, lambda t: {"kind": t["railway"]})
    water = others(lambda t: "waterway" in t or t.get("natural") == "water", lambda t: {"kind": t.get("waterway", "water")})
    parks = others(lambda t: "leisure" in t, lambda t: {"kind": t["leisure"]})
    stops = [{"name": n["tags"].get("name", ""), "kind": n["tags"].get("highway") or n["tags"].get("amenity") or n["tags"].get("public_transport"), "at": project(n)}
             for n in elements if n["type"] == "node"]
    buildings = [{"pts": line(w)[:-1], "floors": int(number(w["tags"].get("building:levels"), 0)), "height": number(w["tags"].get("height"), 0.0),
                  "kind": w["tags"]["building"], "name": w["tags"].get("name", "")} for w in ways if "building" in w["tags"] and len(w["geometry"]) >= 4]

    data = os.path.join(HERE, "..", "Data")
    with open(os.path.join(data, f"osm_{DISTRICT}.json"), "w", encoding="utf-8") as fh:
        json.dump({"format": "naija-hustle-district", "version": 1, "district": title, "units": "cm", "axes": "X east, Y south (Unreal)",
                   "credit": "(c) OpenStreetMap contributors, ODbL", "bbox": {"south": bbox[0], "west": bbox[1], "north": bbox[2], "east": bbox[3]},
                   "streets": streets, "junctions": junctions, "walkways": walks, "rail": rail, "water": water, "parks": parks, "stops": stops},
                  fh, separators=(",", ":"), ensure_ascii=False)
    with open(os.path.join(data, f"streets_{DISTRICT}.csv"), "w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(sorted(rows, key=lambda r: (r["StreetName"] == "", r["StreetName"], r["Name"])))
    with open(os.path.join(CACHE, f"buildings_{DISTRICT}.json"), "w", encoding="utf-8") as fh:
        json.dump({"district": title, "credit": "(c) OpenStreetMap contributors, ODbL", "buildings": buildings}, fh, separators=(",", ":"), ensure_ascii=False)

    named = [r for r in rows if r["StreetName"]]
    km = sum(r["LengthM"] for r in rows) / 1000.0
    by_class = {}
    for r in rows:
        by_class[r["RoadClass"]] = by_class.get(r["RoadClass"], 0) + 1
    print(f"{title}: {len(rows)} street segments, {km:.0f} km; {len(named)} named ({len({r['StreetName'] for r in named})} different names, "
          f"{sum(r['LengthM'] for r in named) / 1000.0:.0f} km); lanes known for {sum(1 for r in rows if r['Lanes'])}; "
          f"{sum(1 for r in rows if r['bBridge'] == 'true')} bridge segments, {sum(1 for r in rows if r['bRoundabout'] == 'true')} roundabout segments")
    print("by class: " + ", ".join(f"{k} {v}" for k, v in sorted(by_class.items(), key=lambda kv: -kv[1])))
    print(f"{len(junctions)} junctions, {len(walks)} walkways, {len(rail)} rail lines, {len(water)} waterways and water, {len(parks)} parks and pitches, {len(stops)} stops and signals")
    print(f"{len(buildings)} building footprints ({sum(1 for b in buildings if b['floors'])} with floors, {sum(1 for b in buildings if b['height'])} with a height)")
    for name in (f"osm_{DISTRICT}.json", f"streets_{DISTRICT}.csv"):
        print(f"  Data/{name}: {os.path.getsize(os.path.join(data, name)) // 1000} KB")
    print(f"  {os.path.join(CACHE, 'buildings_' + DISTRICT + '.json')}: {os.path.getsize(os.path.join(CACHE, 'buildings_' + DISTRICT + '.json')) // 1000} KB (not in the project)")


if __name__ == "__main__":
    main()
