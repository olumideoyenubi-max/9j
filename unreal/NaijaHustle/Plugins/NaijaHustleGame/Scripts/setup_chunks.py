"""Gives every district its pak chunk (living-Lagos brief, 1.3).

Run inside the Unreal Editor (Scripts/mac.sh script <this file>) after Scripts/storage_audit.py, which writes
Data/chunks.json: the chunk of each district (0 is the base install: everything shared, plus Oshodi) and the
district of each 600 m building cell.

For every chunk but the base it makes a Primary Asset Label, /Game/NaijaHustle/Chunks/Chunk_<id>_<district>, that
names the building and tree pieces of that district's cells and gives them the chunk id. Whatever no label names
(roads, bridges, terrain, characters, vehicles, sounds, the maps) stays in chunk 0. Config/DefaultGame.ini turns
chunked packaging on. The labels are generated, so they are not stored in the repo. Safe to run again.
"""
import json
import os
import re
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
DEST = "/Game/NaijaHustle/Chunks"
TILES = "/Game/Lagos/Tiles"
library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def cell_of(name):
    found = re.search(r"__(m?\d+)_(m?\d+)$", name)
    return ",".join(str(-int(v[1:]) if v.startswith("m") else int(v)) for v in found.groups()) if found else None


def main():
    with open(os.path.join(HERE, "..", "Data", "chunks.json"), encoding="utf-8") as fh:
        plan = json.load(fh)
    by_chunk = {}
    for path in library.list_assets(TILES, recursive=True, include_folder=False):
        package = path.split(".")[0]
        folder, name = package.rsplit("/", 2)[-2:]
        cell = cell_of(name) if folder.startswith(("Buildings", "Trees")) else None
        district = plan["cells"].get(cell) if cell else None
        chunk = plan["chunks"].get(district, 0) if district else 0
        if chunk:
            by_chunk.setdefault((chunk, district), []).append(path)
    made = 0
    for (chunk, district), assets in sorted(by_chunk.items()):
        name = f"Chunk_{chunk}_{re.sub(r'[^A-Za-z0-9]', '', district.encode('ascii', 'ignore').decode())}"
        path = f"{DEST}/{name}"
        label = unreal.load_asset(path) if library.does_asset_exist(path) else tools.create_asset(name, DEST, unreal.PrimaryAssetLabel, unreal.DataAssetFactory())
        if not label:
            unreal.log_warning(f"CHUNKS: could not make {path}")
            continue
        rules = unreal.PrimaryAssetRules()
        rules.set_editor_property("chunk_id", chunk)
        rules.set_editor_property("priority", 10)
        rules.set_editor_property("cook_rule", unreal.PrimaryAssetCookRule.ALWAYS_COOK)
        label.set_editor_property("rules", rules)
        label.set_editor_property("label_assets_in_my_directory", False)
        # the list wants the assets themselves, so each chunk's pieces are loaded, named, and let go again
        label.set_editor_property("explicit_assets", [loaded for loaded in (unreal.load_asset(a) for a in assets) if loaded])
        library.save_loaded_asset(label, only_if_is_dirty=False)
        made += 1
        unreal.SystemLibrary.collect_garbage()
    unreal.log(f"CHUNKS: {made} district chunks labelled, {sum(len(a) for a in by_chunk.values())} pieces in them; everything else is chunk 0")


main()
