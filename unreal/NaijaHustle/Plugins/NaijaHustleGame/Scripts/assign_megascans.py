"""Plugs downloaded Megascans / Fab textures into the city's surface materials.

Run inside the Unreal Editor (Tools > Execute Python Script...) after nh_blockout_materials.py, once you have
added surfaces from Fab or Quixel Bridge. It looks at every texture under /Game/Megascans and /Game/Fab, works
out from the asset names which surface type each set suits and which map each texture is, and sets the BaseColor,
Normal and ORM parameters of the matching /Game/NaijaHustle/Environment/Materials/MI_NHSurface_<Type>.

  dirt | ground | soil        -> Dirt
  concrete | plaster | wall   -> Concrete and Plaster (a set named "plaster" goes to Plaster first)
  corrugated | rust | metal   -> Zinc
  tarp | fabric               -> Tarp
  wood | plank                -> Wood
  asphalt | road              -> Asphalt

Safe to run again: it only changes the three texture parameters of the types it finds a set for.
"""
import re

ROOTS = ["/Game/Megascans", "/Game/Fab"]
MAT_DIR = "/Game/NaijaHustle/Environment/Materials"
# checked in this order, so "rusty corrugated metal wall" is Zinc and "plaster wall" is Plaster
KEYWORDS = [("Zinc", ("corrugated", "rust")), ("Asphalt", ("asphalt", "road")), ("Tarp", ("tarp", "fabric")), ("Wood", ("wood", "plank")),
            ("Plaster", ("plaster",)), ("Concrete", ("concrete",)), ("Dirt", ("dirt", "ground", "soil")), ("Concrete", ("wall",)), ("Zinc", ("metal",))]
SHARED = {"Plaster": "Concrete", "Concrete": "Plaster"}  # one borrows the other's set when it has none of its own
# map suffixes as Bridge and Fab name them: T_Name_2K_D / _N / _ORM, Name_BaseColor / _Normal / _ORM
MAPS = [("ORM", r"(_orm|_ordp|occlusionroughnessmetallic)$"), ("Normal", r"(_n|_nrm|_normal|_normalgl|_normaldx)$"),
        ("BaseColor", r"(_d|_bc|_basecolor|_base_color|_albedo|_diffuse|_color|_col)$")]


def map_kind(name):
    """'BaseColor', 'Normal' or 'ORM' for a texture asset name, and the name with that suffix removed."""
    low = name.lower()
    for kind, pattern in MAPS:
        m = re.search(pattern, low)
        if m:
            return kind, low[:m.start()]
    return None, low


def surface_type(text):
    low = text.lower()
    for kind, words in KEYWORDS:
        if any(w in low for w in words):
            return kind
    return None


def plan(textures):
    """textures: (folder, asset name, object path). Returns {surface type: {map kind: object path}} for the best set of each type."""
    sets = {}
    for folder, name, path in textures:
        kind, stem = map_kind(name)
        if kind:
            sets.setdefault((folder, stem), {}).setdefault(kind, path)
    best = {}
    for (folder, stem), maps in sorted(sets.items()):
        kind = surface_type(stem) or surface_type(folder.rsplit("/", 1)[-1])
        if not kind or "BaseColor" not in maps:
            continue
        if kind not in best or len(maps) > len(best[kind]):
            best[kind] = maps
    for kind, other in SHARED.items():
        if kind not in best and other in best:
            best[kind] = best[other]
    return best


def main():
    import unreal
    eal, mel = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    textures = []
    for root in ROOTS:
        for a in registry.get_assets_by_path(root, recursive=True):
            if str(a.asset_class_path.asset_name) in ("Texture2D", "VirtualTexture2D"):
                textures.append((str(a.package_path), str(a.asset_name), f"{a.package_name}.{a.asset_name}"))
    if not textures:
        unreal.log(f"NAIJA HUSTLE: no textures under {' or '.join(ROOTS)}; add surfaces from Fab or Quixel Bridge and run this again")
        return {}
    matched = plan(textures)
    done = []
    for kind, maps in sorted(matched.items()):
        mi_path = f"{MAT_DIR}/MI_NHSurface_{kind}"
        if not eal.does_asset_exist(mi_path):
            unreal.log_warning(f"NAIJA HUSTLE: {mi_path} is missing; run nh_blockout_materials.py first")
            continue
        mi = eal.load_asset(mi_path)
        for param, path in sorted(maps.items()):
            mel.set_material_instance_texture_parameter_value(mi, param, eal.load_asset(path))
            unreal.log(f"NAIJA HUSTLE: {kind}.{param} <- {path}")
        mel.update_material_instance(mi)
        eal.save_asset(mi_path)
        done.append(kind)
    unreal.log(f"NAIJA HUSTLE: looked at {len(textures)} textures; assigned {', '.join(done) if done else 'nothing'}"
               + ("" if done else " (no set's name matched a surface type)"))
    return matched


if __name__ == "__main__":
    main()
