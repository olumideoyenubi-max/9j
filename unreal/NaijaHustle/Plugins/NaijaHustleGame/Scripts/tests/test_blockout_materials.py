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
ok("builds three materials (plain instanced, loose vehicle and people pieces, textured city surface)", len(mats) == 3 and all(hasattr(m, "graph") for m in mats), len(mats))
for m, kind, src in ((mats[0], "M_NHBlockout", "MaterialExpressionPerInstanceCustomData"), (mats[1], "M_NHBlockoutPrim", "MaterialExpressionScalarParameter"),
                     (mats[2], "M_NHSurface", "MaterialExpressionPerInstanceCustomData")):
    nodes, links, outputs = m.graph
    if src == "MaterialExpressionPerInstanceCustomData":
        idx = sorted(n.get_editor_property("data_index") for n in nodes if type(n).__name__ == src)
    else:
        idx = sorted(n.get_editor_property("primitive_data_index") for n in nodes if type(n).__name__ == src and n.get_editor_property("use_custom_primitive_data"))
    ok(f"{kind}: reads all 8 floats ({src})", idx == list(range(8)), idx)
    params = sorted(str(n.get_editor_property("parameter_name")) for n in nodes if type(n).__name__ == "MaterialExpressionCollectionParameter")
    ok(f"{kind}: reads Wetness, Puddles and NightLights", params == ["NightLights", "Puddles", "Wetness"], params)
    want = {"base", "rough", "metal", "emissive"} | ({"ao", "normal"} if kind == "M_NHSurface" else set())
    ok(f"{kind}: drives base colour, roughness, metallic and emissive" + (", occlusion and normal" if kind == "M_NHSurface" else ""), set(outputs) == want, outputs.keys())
    dangling = []
    for n in nodes:
        pins = {p for s_, d, p in links if d is n}
        need = {"MaterialExpressionLinearInterpolate": {"Alpha"}, "MaterialExpressionMultiply": {"A"}, "MaterialExpressionAppendVector": {"A", "B"},
                "MaterialExpressionSaturate": {""}, "MaterialExpressionComponentMask": {""}, "MaterialExpressionSubtract": {"A"},
                "MaterialExpressionAdd": {"A"}, "MaterialExpressionDivide": {"A"}, "MaterialExpressionDotProduct": {"A", "B"}, "MaterialExpressionAbs": {""},
                "MaterialExpressionFrac": {""}, "MaterialExpressionOneMinus": {""}, "MaterialExpressionSine": {""}, "MaterialExpressionNormalize": {"VectorInput"},
                "MaterialExpressionTextureSampleParameter2D": {"UVs"}}.get(type(n).__name__, set())
        if need - pins:
            dangling.append((type(n).__name__, need - pins))
    ok(f"{kind}: no node is missing a required input", not dangling, dangling)
    used = {id(s_) for s_, d, p in links} | {id(v) for v in outputs.values()}
    unused = [type(n).__name__ for n in nodes if id(n) not in used]
    ok(f"{kind}: every node feeds something", not unused, unused)
ok("the instanced ones allow instanced static meshes; all assets saved", mats[0].get_editor_property("used_with_instanced_static_meshes") is True
   and mats[2].get_editor_property("used_with_instanced_static_meshes") is True
   and all(any(p.endswith(n) for p in unreal.EditorAssetLibrary.saved) for n in ("M_NHBlockout", "M_NHBlockoutPrim", "M_NHSurface", "MPC_NHWeather")))

# the textured surface material: texture and wear parameters, and one instance per C++ surface type
nodes, links, outputs = mats[2].graph
tex = {}
for n in nodes:
    if type(n).__name__ == "MaterialExpressionTextureSampleParameter2D":
        tex.setdefault(str(n.get_editor_property("parameter_name")), []).append(n)
ok("M_NHSurface: BaseColor, Normal and ORM, each sampled once per axis (triplanar) with a flat default texture",
   sorted(tex) == ["BaseColor", "Normal", "ORM"] and all(len(v) == 3 and all(n.get_editor_property("texture") is not None for n in v) for v in tex.values()), {k: len(v) for k, v in tex.items()})
ok("M_NHSurface: samplers match their maps (colour, normal, masks) and the normal is output in world space",
   {k: v[0].get_editor_property("sampler_type") for k, v in tex.items()} == {"BaseColor": "MaterialSamplerType.SAMPLERTYPE_COLOR", "Normal": "MaterialSamplerType.SAMPLERTYPE_NORMAL",
                                                                              "ORM": "MaterialSamplerType.SAMPLERTYPE_MASKS"} and mats[2].get_editor_property("tangent_space_normal") is False)
scalars = sorted(str(n.get_editor_property("parameter_name")) for n in nodes if type(n).__name__ == "MaterialExpressionScalarParameter")
ok("M_NHSurface: Tiling, Grime, Splash and Variation parameters", scalars == ["Grime", "Splash", "Tiling", "Variation"], scalars)
ok("the plain materials have no textures", not any(type(n).__name__ == "MaterialExpressionTextureSampleParameter2D" for m in mats[:2] for n in m.graph[0]))
types = [v.split(".")[1].title() for v in vars(unreal.NHSurfaceType).values()]
mis = {k.rsplit("_", 1)[1]: v for k, v in unreal.EditorAssetLibrary.assets.items() if "/MI_NHSurface_" in k}
ok("one MI_NHSurface_<Type> per ENHSurfaceType, parented to M_NHSurface, with its wear values, saved",
   sorted(mis) == sorted(types) and all(mi.parent is mats[2] and set(mi.scalars) == {"Tiling", "Grime", "Splash", "Variation"} and mi.updated for mi in mis.values())
   and all(any(p.endswith("MI_NHSurface_" + t) for p in unreal.EditorAssetLibrary.saved) for t in types), (sorted(mis), sorted(types)))
ok("walls get a dirt splash band; ground, roofs and glass do not", mis["Plaster"].scalars["Splash"] > 0 and mis["Concrete"].scalars["Splash"] > 0
   and all(mis[t].scalars["Splash"] == 0 for t in ("Dirt", "Asphalt", "Zinc", "Glass")))
print("ALL PASS" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
