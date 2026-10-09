"""Measures what the project costs on disk, without opening Unreal. Run it after every step:

    python3 Plugins/NaijaHustleGame/Scripts/storage_audit.py            (from unreal/NaijaHustle)

It prints: the project's folders, Content by folder, the 20 largest assets, assets stored more than once (same
name and size in different folders), and what each district's pak chunk would hold. The chunk figures are the
editor's uncooked sizes: a cooked chunk is smaller, but the proportions hold.

Chunks (Data/chunks.json is written here and read by setup_chunks.py): chunk 0 is the base install, which is
everything shared plus Oshodi; every other district that has buildings of its own gets a chunk. A building or tree
piece belongs to the district whose centre is nearest the middle of its 600 m cell. Roads, bridges, terrain,
characters, vehicles and sounds are shared, so they are in the base chunk.
"""
import json
import os
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
CONTENT = ROOT / "Content"
PLUGIN = ROOT / "Plugins" / "NaijaHustleGame"
CELL = 60000.0          # a building cell is 600 m, in cm
BASE_DISTRICTS = {"Oshodi"}
MB = 1048576.0


def size_of(path):
    out = subprocess.run(["du", "-sk", str(path)], capture_output=True, text=True).stdout.split()
    return int(out[0]) * 1024 if out else 0


def cell_of(name):
    """Buildings_0_0_Low__m21_m13 -> (-21, -13); None for pieces that are not cut by building cell"""
    found = re.search(r"__(m?\d+)_(m?\d+)$", name)
    if not found:
        return None
    return tuple(-int(v[1:]) if v.startswith("m") else int(v) for v in found.groups())


def main():
    print(f"project {ROOT}")
    for folder in ("Content", "Intermediate", "Saved", "DerivedDataCache", "Binaries", "Plugins/NaijaHustleGame/Intermediate", "Plugins/NaijaHustleGame/Binaries"):
        if (ROOT / folder).exists():
            print(f"  {size_of(ROOT / folder) / MB:9.0f} MB  {folder}")
    print("Content by folder:")
    for sub in sorted(CONTENT.iterdir(), key=lambda p: -size_of(p)):
        if sub.is_dir() and size_of(sub) > MB:
            print(f"  {size_of(sub) / MB:9.0f} MB  {sub.name}")

    files = [(p.stat().st_size, p) for p in CONTENT.rglob("*") if p.is_file() and p.suffix in (".uasset", ".umap", ".ubulk", ".uexp")]
    total = sum(s for s, _ in files)
    print(f"{len(files)} asset files, {total / MB:.0f} MB")
    print("20 largest assets:")
    for size, path in sorted(files, key=lambda f: -f[0])[:20]:
        print(f"  {size / MB:6.1f} MB  {path.relative_to(CONTENT)}")

    # the same thing stored again: same file name and same size in more than one folder
    again = defaultdict(list)
    for size, path in files:
        if size > 256 * 1024:
            again[(re.sub(r"_D$", "", path.stem), size)].append(path)
    waste = sorted(((size * (len(paths) - 1), name, len(paths)) for (name, size), paths in again.items() if len(paths) > 1), reverse=True)
    print(f"stored more than once (same name and size): {sum(w for w, _, _ in waste) / MB:.0f} MB could be shared")
    for wasted, name, copies in waste[:10]:
        print(f"  {wasted / MB:6.1f} MB  {name} x{copies}")

    # ---- chunks by district
    real = json.loads((PLUGIN / "Data" / "lagos_real.json").read_text())
    districts = [(d["name"], d["x"], d["y"]) for d in real["districts"]]
    tiles = CONTENT / "Lagos" / "Tiles"
    by_district = defaultdict(lambda: [0, 0])
    shared = 0
    cells = {}
    if tiles.exists():
        for size, path in files:
            if tiles not in path.parents:
                continue
            cell = cell_of(path.stem) if path.parent.name.startswith(("Buildings", "Trees")) else None
            if cell is None:
                shared += size
                continue
            x, y = (cell[0] + 0.5) * CELL, -(cell[1] + 0.5) * CELL   # Blender north is Unreal -Y
            name = min(districts, key=lambda d: (d[1] - x) ** 2 + (d[2] - y) ** 2)[0]
            by_district[name][0] += size
            by_district[name][1] += 1
            cells[f"{cell[0]},{cell[1]}"] = name
    names = sorted(by_district, key=lambda n: -by_district[n][0])
    chunk_ids, next_id = {}, 100
    for name in sorted(by_district):
        if name in BASE_DISTRICTS:
            chunk_ids[name] = 0
        else:
            chunk_ids[name] = next_id
            next_id += 1
    everything_else = total - sum(v[0] for v in by_district.values())
    base = everything_else + sum(by_district[n][0] for n in BASE_DISTRICTS if n in by_district)
    print(f"chunks: {len(set(chunk_ids.values()))} in all. Chunk 0 (base: shared + Oshodi) {base / MB:.0f} MB uncooked; the district chunks {sum(by_district[n][0] for n in names if chunk_ids[n]) / MB:.0f} MB between them")
    for name in names[:12]:
        print(f"  chunk {chunk_ids[name]:3d}  {by_district[name][0] / MB:6.1f} MB  {by_district[name][1]:4d} pieces  {name}")
    if len(names) > 12:
        print(f"  ... and {len(names) - 12} smaller")
    (PLUGIN / "Data" / "chunks.json").write_text(json.dumps({
        "note": "Pak chunk of each district (0 is the base install) and the district of each 600 m building cell. Written by Scripts/storage_audit.py; read by Scripts/setup_chunks.py.",
        "chunks": chunk_ids, "cells": dict(sorted(cells.items()))}, indent=1, ensure_ascii=False) + "\n")


main()
