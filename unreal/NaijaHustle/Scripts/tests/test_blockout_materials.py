"""Runs nh_blockout_materials.py against the stand-in `unreal` module and checks the graph it builds.
    python3 unreal/NaijaHustle/Scripts/tests/test_blockout_materials.py"""
import os
import runpy
import sys

HERE = os.path.dirname(__file__)
sys.path.insert(0, HERE)
import unreal_stub as unreal  # noqa: E402

runpy.run_path(os.path.join(HERE, "..", "nh_blockout_materials.py"), run_name="__main__")
mel = unreal.MaterialEditingLibrary
fails = 0
def ok(name, cond, info=None):
    global fails
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  {info}"))
    fails += 0 if cond else 1

kinds = [type(n).__name__ for n in mel.nodes]
cd = sorted(n.get_editor_property("data_index") for n in mel.nodes if type(n).__name__ == "MaterialExpressionPerInstanceCustomData")
ok("reads all 8 custom data floats", cd == list(range(8)), cd)
params = sorted(str(n.get_editor_property("parameter_name")) for n in mel.nodes if type(n).__name__ == "MaterialExpressionCollectionParameter")
ok("reads Wetness, Puddles and NightLights from the weather collection", params == ["NightLights", "Puddles", "Wetness"], params)
ok("drives base colour, roughness, metallic and emissive", set(mel.outputs) == {"base", "rough", "metal", "emissive"}, mel.outputs.keys())
# every node with inputs has its required pins connected (a Lerp needs Alpha; a Multiply needs A)
dangling = []
for n in mel.nodes:
    pins = {p for s, d, p in mel.links if d is n}
    t = type(n).__name__
    need = {"MaterialExpressionLinearInterpolate": {"Alpha"}, "MaterialExpressionMultiply": {"A"}, "MaterialExpressionAppendVector": {"A", "B"},
            "MaterialExpressionSaturate": {""}, "MaterialExpressionComponentMask": {""}, "MaterialExpressionSubtract": {"A"}}.get(t, set())
    if need - pins:
        dangling.append((t, need - pins))
ok("no node is missing a required input", not dangling, dangling)
used = {id(s) for s, d, p in mel.links} | {id(v) for v in mel.outputs.values()}
unused = [type(n).__name__ for n in mel.nodes if id(n) not in used]
ok("every node feeds something", not unused, unused)
ok("instanced static meshes allowed; both assets saved", any(p.endswith("M_NHBlockout") for p in unreal.EditorAssetLibrary.saved) and any(p.endswith("MPC_NHWeather") for p in unreal.EditorAssetLibrary.saved))
print("ALL PASS" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
