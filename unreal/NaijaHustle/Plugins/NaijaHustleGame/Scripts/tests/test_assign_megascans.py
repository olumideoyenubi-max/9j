"""Checks how assign_megascans.py reads texture names, then runs it against the stand-in `unreal` module.
    python3 unreal/NaijaHustle/Scripts/tests/test_assign_megascans.py"""
import importlib.util
import os
import sys

HERE = os.path.dirname(__file__)
sys.path.insert(0, HERE)
import unreal_stub as unreal  # noqa: E402

spec = importlib.util.spec_from_file_location("assign_megascans", os.path.join(HERE, "..", "assign_megascans.py"))
am = importlib.util.module_from_spec(spec)
spec.loader.exec_module(am)

fails = 0
def ok(name, cond, info=None):
    global fails
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  {info}"))
    fails += 0 if cond else 1

ok("map kinds from Bridge and Fab suffixes", [am.map_kind(n)[0] for n in ("T_RedDirt_xk3_2K_D", "T_RedDirt_xk3_2K_N", "T_RedDirt_xk3_2K_ORM", "Plank_BaseColor", "Plank_Normal", "T_Thing_Mask")]
   == ["BaseColor", "Normal", "ORM", "BaseColor", "Normal", None])
ok("surface types from keywords", [am.surface_type(n) for n in ("Red_Dirt_Ground", "forest_soil", "Rough_Concrete_Wall", "Painted_Plaster_Wall", "Rusty_Corrugated_Metal", "Blue_Tarp",
                                                               "Old_Wood_Planks", "Cracked_Asphalt", "Country_Road", "Brushed_Metal", "Marble")]
   == ["Dirt", "Dirt", "Concrete", "Plaster", "Zinc", "Tarp", "Wood", "Asphalt", "Asphalt", "Zinc", None])

def tex(folder, stem, kinds=("D", "N", "ORM")):
    return [(folder, f"T_{stem}_2K_{k}", f"{folder}/T_{stem}_2K_{k}.T_{stem}_2K_{k}") for k in kinds]
found = tex("/Game/Megascans/Surfaces/Red_Dirt_xk3", "Red_Dirt_xk3") + tex("/Game/Fab/Rough_Concrete", "Rough_Concrete") \
    + tex("/Game/Megascans/Surfaces/Rusty_Corrugated_Metal", "Rusty_Corrugated_Metal", ("D", "N")) + tex("/Game/Fab/Marble", "Marble") \
    + tex("/Game/Fab/Old_Planks", "Old_Planks", ("N",))
p = am.plan(found)
ok("each set goes to its type with all its maps; plaster borrows the concrete set", sorted(p) == ["Concrete", "Dirt", "Plaster", "Zinc"]
   and set(p["Dirt"]) == {"BaseColor", "Normal", "ORM"} and set(p["Zinc"]) == {"BaseColor", "Normal"} and p["Plaster"] is p["Concrete"], {k: sorted(v) for k, v in p.items()})
ok("a set with no base colour, or a name that matches nothing, is left out", "Wood" not in p and not any("Marble" in v for m in p.values() for v in m.values()))
both = am.plan(found + tex("/Game/Fab/Painted_Plaster", "Painted_Plaster"))
ok("plaster uses its own set when there is one", "Painted_Plaster" in both["Plaster"]["BaseColor"] and "Rough_Concrete" in both["Concrete"]["BaseColor"])

# the editor side, against the stub
class _Asset:
    def __init__(self, folder, name):
        self.package_path, self.asset_name, self.package_name = folder, name, f"{folder}/{name}"
        self.asset_class_path = type("P", (), {"asset_name": "Texture2D"})()
class _Registry:
    def get_assets_by_path(self, root, recursive=False):
        return [_Asset(f, n) for f, n, _ in found if f.startswith(root)]
unreal.AssetRegistryHelpers = type("AssetRegistryHelpers", (), {"get_asset_registry": staticmethod(lambda: _Registry())})
mis = {}
for t in ("Dirt", "Concrete", "Plaster", "Zinc"):
    mis[t] = unreal.MaterialInstanceConstant()
    unreal.EditorAssetLibrary.assets[f"{am.MAT_DIR}/MI_NHSurface_{t}"] = mis[t]
am.main()
ok("sets the texture parameters on the matching instances, updates and saves them",
   all(set(mis[t].textures) == set(p[t]) and mis[t].updated == 1 and f"{am.MAT_DIR}/MI_NHSurface_{t}" in unreal.EditorAssetLibrary.saved for t in mis), {t: sorted(m.textures) for t, m in mis.items()})
ok("reports what it matched", any("assigned Concrete, Dirt, Plaster, Zinc" in m for m in unreal.LOG) and any("Dirt.BaseColor <- /Game/Megascans" in m for m in unreal.LOG), unreal.LOG[-1])

print("ALL PASS" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
