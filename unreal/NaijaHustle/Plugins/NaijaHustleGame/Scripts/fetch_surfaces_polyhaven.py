"""Downloads the surface library's textures from Poly Haven (polyhaven.com), which are CC0: public domain.

  python3 fetch_surfaces_polyhaven.py [folder]        (default ~/Downloads/nh-surfaces)

Plain Python, run outside Unreal. For each surface in SURFACES it saves three maps into <folder>/<name>/:
BaseColor.jpg, Normal.jpg (OpenGL convention) and ORM.jpg (Poly Haven's "arm" map: ambient occlusion in red,
roughness in green, metallic in blue, already packed in one texture). Hero surfaces come at 2K, the rest at 1K
(TIER). Scripts/import_surfaces.py then brings the folder into Unreal. Files already there are kept.

These stand in for the Megascans surfaces the look brief asks for, which need the project owner's Fab account; a
Megascans surface dropped into the same folder layout imports the same way. The downloads are not part of the repo.
CC0 needs no credit; ASSETS.md lists them anyway.
"""
import json
import os
import sys
import urllib.request

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.expanduser("~"), "Downloads", "nh-surfaces")
# game name -> (Poly Haven id, kind, tier). Kind picks the master material: "ground" or "building".
SURFACES = {
    # roads and ground
    "Asphalt": ("asphalt_02", "ground", "hero"), "Asphalt_Worn": ("worn_asphalt", "ground", "hero"), "Asphalt_Cracked": ("road_damaged", "ground", "hero"),
    "Asphalt_Patched": ("asphalt_04", "ground", "filler"), "Asphalt_Tarred_Gravel": ("tarred_gravel", "ground", "filler"),
    "Laterite": ("red_dirt_mud_01", "ground", "hero"), "Laterite_Dry": ("dry_mud_field_001", "ground", "filler"), "Red_Sand": ("red_sand", "ground", "filler"),
    "Dirt": ("dirt", "ground", "filler"), "Mud_Tracks": ("muddy_tracks", "ground", "filler"), "Mud_Dry": ("brown_mud_dry", "ground", "filler"),
    "Gravel_Road": ("gravel_road", "ground", "filler"), "Sandy_Gravel": ("sandy_gravel", "ground", "filler"),
    "Concrete_Pavement": ("concrete_pavement", "ground", "filler"), "Concrete_Pavement_Worn": ("concrete_pavement_02", "ground", "filler"),
    "Concrete_Floor_Worn": ("concrete_floor_worn_001", "ground", "filler"), "Interlock_Paving": ("herringbone_pavement", "ground", "filler"),
    # walls
    "Plaster": ("plastered_wall", "building", "hero"), "Plaster_Rough": ("plastered_wall_02", "building", "filler"), "Plaster_Patched": ("plastered_wall_04", "building", "filler"),
    "Plaster_Painted": ("painted_plaster_wall", "building", "hero"), "Plaster_Blue_Weathered": ("blue_plaster_weathered", "building", "filler"),
    "Plaster_Red_Weathered": ("red_plaster_weathered", "building", "filler"), "Plaster_Damaged": ("damaged_plaster", "building", "filler"),
    "Paint_Peeling": ("peeling_painted_wall", "building", "hero"), "Concrete_Painted": ("painted_concrete", "building", "filler"),
    "Concrete_Painted_Worn": ("painted_concrete_02", "building", "filler"), "Concrete_Dirty": ("dirty_concrete", "building", "filler"),
    "Concrete_Brushed": ("brushed_concrete", "building", "filler"), "Concrete_Mossy": ("concrete_moss", "building", "filler"),
    "Blockwork": ("concrete_block_wall", "building", "hero"), "Blockwork_Raw": ("concrete_block_wall_02", "building", "filler"),
    "Brick_Painted": ("painted_brick", "building", "filler"),
    # roofs, metal, shutters
    "Zinc_Rusty": ("rusty_corrugated_iron", "building", "hero"), "Zinc": ("corrugated_iron", "building", "filler"), "Zinc_Worn": ("worn_corrugated_iron", "building", "filler"),
    "Zinc_Sheet": ("corrugated_iron_02", "building", "filler"), "Metal_Sheet_Rusty": ("rusty_metal_sheet", "building", "filler"),
    "Metal_Painted_Rusty": ("rusty_painted_metal", "building", "filler"), "Shutter_Painted": ("painted_metal_shutter", "building", "filler"),
    "Shutter_Rusty": ("rusty_metal_shutter", "building", "filler"), "Roof_Clay_Tiles": ("clay_roof_tiles", "building", "filler"),
    # wood, tiles
    "Planks_Weathered": ("weathered_planks", "building", "filler"), "Planks_Old": ("old_planks_02", "building", "filler"), "Plywood": ("plywood", "building", "filler"),
    "Tiles_Dirty": ("dirty_tiles", "building", "filler"), "Tiles_Floor": ("floor_tiles_06", "building", "filler"),
}
TIER = {"hero": "2k", "filler": "1k"}
MAPS = {"BaseColor": ("Diffuse", "diff"), "Normal": ("nor_gl",), "ORM": ("arm",)}


def get(url):
    request = urllib.request.Request(url, headers={"User-Agent": "NaijaHustle-surface-fetch/1.0"})
    with urllib.request.urlopen(request, timeout=120) as reply:
        return reply.read()


def main():
    os.makedirs(OUT, exist_ok=True)
    manifest, total, missing = {}, 0, []
    for name, (asset, kind, tier) in SURFACES.items():
        folder = os.path.join(OUT, name)
        os.makedirs(folder, exist_ok=True)
        files, got = None, {}
        for out_name, keys in MAPS.items():
            path = os.path.join(folder, out_name + ".jpg")
            if not os.path.exists(path):
                if files is None:
                    try:
                        files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
                    except Exception as error:
                        files = {}
                        print(f"  {name}: {asset}: {error}")
                entry = next((files[k] for k in keys if k in files), None)
                try:
                    url = entry[TIER[tier]]["jpg"]["url"]
                    with open(path, "wb") as fh:
                        fh.write(get(url))
                except Exception:
                    missing.append(f"{name}/{out_name}")
                    continue
            got[out_name] = os.path.getsize(path)
            total += got[out_name]
        if len(got) == 3:
            manifest[name] = {"source": f"https://polyhaven.com/a/{asset}", "licence": "CC0", "kind": kind, "tier": tier, "size": TIER[tier]}
    with open(os.path.join(OUT, "surfaces.json"), "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=1)
    kinds = {}
    for m in manifest.values():
        kinds[(m["kind"], m["tier"])] = kinds.get((m["kind"], m["tier"]), 0) + 1
    print(f"{len(manifest)} of {len(SURFACES)} surfaces complete in {OUT}: {total / 1e6:.0f} MB; " + ", ".join(f"{k[0]} {k[1]} {v}" for k, v in sorted(kinds.items())))
    if missing:
        print("missing: " + ", ".join(missing))


if __name__ == "__main__":
    main()
