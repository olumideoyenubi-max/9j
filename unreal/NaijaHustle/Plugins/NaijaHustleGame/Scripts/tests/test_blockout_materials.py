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

mats = [m for m, _ in mel.graphs]
ok("builds two materials (instanced city, loose vehicle and people pieces)", len(mats) == 2 and all(hasattr(m, "graph") for m in mats), len(mats))
for m, kind, src in ((mats[0], "M_NHBlockout", "MaterialExpressionPerInstanceCustomData"), (mats[1], "M_NHBlockoutPrim", "MaterialExpressionScalarParameter")):
    nodes, links, outputs = m.graph
    if src == "MaterialExpressionPerInstanceCustomData":
        idx = sorted(n.get_editor_property("data_index") for n in nodes if type(n).__name__ == src)
    else:
        idx = sorted(n.get_editor_property("primitive_data_index") for n in nodes if type(n).__name__ == src and n.get_editor_property("use_custom_primitive_data"))
    ok(f"{kind}: reads all 8 floats ({src})", idx == list(range(8)), idx)
    params = sorted(str(n.get_editor_property("parameter_name")) for n in nodes if type(n).__name__ == "MaterialExpressionCollectionParameter")
    ok(f"{kind}: reads Wetness, Puddles and NightLights", params == ["NightLights", "Puddles", "Wetness"], params)
    ok(f"{kind}: drives base colour, roughness, metallic and emissive", set(outputs) == {"base", "rough", "metal", "emissive"}, outputs.keys())
    dangling = []
    for n in nodes:
        pins = {p for s_, d, p in links if d is n}
        need = {"MaterialExpressionLinearInterpolate": {"Alpha"}, "MaterialExpressionMultiply": {"A"}, "MaterialExpressionAppendVector": {"A", "B"},
                "MaterialExpressionSaturate": {""}, "MaterialExpressionComponentMask": {""}, "MaterialExpressionSubtract": {"A"}}.get(type(n).__name__, set())
        if need - pins:
            dangling.append((type(n).__name__, need - pins))
    ok(f"{kind}: no node is missing a required input", not dangling, dangling)
    used = {id(s_) for s_, d, p in links} | {id(v) for v in outputs.values()}
    unused = [type(n).__name__ for n in nodes if id(n) not in used]
    ok(f"{kind}: every node feeds something", not unused, unused)
ok("the instanced one allows instanced static meshes; all assets saved", mats[0].get_editor_property("used_with_instanced_static_meshes") is True
   and all(any(p.endswith(n) for p in unreal.EditorAssetLibrary.saved) for n in ("M_NHBlockout", "M_NHBlockoutPrim", "MPC_NHWeather")))
print("ALL PASS" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
