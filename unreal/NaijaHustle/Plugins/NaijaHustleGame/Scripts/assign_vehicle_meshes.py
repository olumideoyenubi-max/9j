"""Gives the game's vehicle types real car models.

Run inside the Unreal Editor (Tools > Execute Python Script...) after adding car models from Fab. It looks at
every static mesh under /Game/Fab, /Game/Megascans, /Game/Quixel and /Game/Vehicles, works out from the names
which vehicle type each model suits, fits it to the type's length, and writes Data/vehicle_meshes.json, which the
game reads at start. A type with no model keeps its blockout body.

  hypercar | hyper                              -> hypercar   (Zaki W16)
  super suv | sport suv                         -> supersuv   (Lekki Fury)
  coupe suv | suv coupe                         -> coupesuv   (Maitama CX Coupe)
  royal | ultra luxury suv                      -> royalsuv   (Oba Monarch)
  grand coupe | luxury coupe                    -> luxcoupe   (Oba Mirage)
  sports | supercar | coupe | roadster          -> sports     (Eko Veloce)
  limo | luxury | executive                     -> luxsedan   (Oba Royale)
  luxury suv | g-class | boxy 4x4               -> luxsuv     (Zuma GX)
  sedan | saloon | hatchback                    -> sedan      (Kamsi LE)
  suv | jeep | 4x4                              -> suv        (Pathmaster V8)
  minibus | bus | van                           -> danfo
  tricycle | rickshaw | tuktuk | keke           -> keke
  motorcycle | motorbike | scooter              -> okada
  truck | tipper | lorry                        -> truck
  pickup                                        -> tfpick

A model is all the static meshes in one folder (a body and its wheels, say), drawn together. The script turns
the model so its long side runs front to back, but cannot tell the front from the back: if a car drives
backwards, add 180 to its "yaw" in Data/vehicle_meshes.json, or run again with the type listed in FLIP below or
in NH_CAR_FLIP. The console command NHCarShow lines every type up facing east to check.
Skeletal (rigged) car models are reported but not used yet.

Models under /Game/Vehicles (from import_car_gltf.py or import_car_fbx.py) take their type from the name of
their folder there, whatever their meshes are called.
"""
import json
import os
import re

ROOTS = ["/Game/Fab", "/Game/Megascans", "/Game/Quixel", "/Game/Vehicles"]
# vehicle types whose model came out facing backwards: list them here, or in the NH_CAR_FLIP environment variable (comma-separated)
FLIP = set(t for t in os.environ.get("NH_CAR_FLIP", "").split(",") if t)
# checked in this order: the luxury and sports words first, so "luxury suv" is not just an suv
KEYWORDS = [
    ("hypercar", ("hypercar", "hyper_car", "hyper")),
    ("supersuv", ("super_suv", "supersuv", "sport_suv", "sports_suv", "performance_suv")),
    ("coupesuv", ("coupe_suv", "suv_coupe", "coupesuv")),
    ("royalsuv", ("royal", "ultra_luxury_suv", "ultra_lux_suv")),
    ("luxcoupe", ("grand_coupe", "luxury_coupe", "lux_coupe", "grand_tourer")),
    ("sports", ("sports", "sport_car", "sportcar", "supercar", "coupe", "roadster", "racing")),
    ("luxsuv", ("luxury_suv", "luxurysuv", "lux_suv", "gclass", "g_class", "gwagon", "g_wagon", "luxury_4x4", "premium_suv", "boxy_suv")),
    ("luxsedan", ("limo", "limousine", "luxury", "executive", "premium_sedan")),
    ("tfpick", ("pickup", "pick_up")),
    ("truck", ("truck", "tipper", "lorry", "dumper")),
    ("danfo", ("minibus", "mini_bus", "bus", "van")),
    ("keke", ("tricycle", "rickshaw", "tuktuk", "tuk_tuk", "keke")),
    ("okada", ("motorcycle", "motorbike", "moto", "scooter")),
    ("suv", ("suv", "jeep", "4x4", "offroad", "off_road")),
    ("sedan", ("sedan", "saloon", "hatchback", "car")),
]
NOT_A_CAR = ("wheel", "tire", "tyre", "rim", "door", "seat", "steering", "interior", "glass", "window", "light", "bumper", "mirror", "engine", "prop", "sign", "cargo", "carpet", "card", "cart")


def vehicle_type(text):
    low = re.sub(r"[^a-z0-9]+", "_", text.lower())
    for kind, words in KEYWORDS:
        for w in words:
            if re.search(r"(^|_)" + re.escape(w) + r"s?(_|$|\d)", low) or (len(w) > 5 and w in low):
                return kind
    return None


def is_part(name):
    low = name.lower()
    return any(w in low for w in NOT_A_CAR)


def fit(boxes, length):
    """boxes: (min xyz, max xyz) per mesh. Returns yaw, scale, offset and height so the model is `length` cm long, faces +X, sits on z = 0 and is centred."""
    lo = [min(b[0][i] for b in boxes) for i in range(3)]
    hi = [max(b[1][i] for b in boxes) for i in range(3)]
    size = [hi[i] - lo[i] for i in range(3)]
    yaw = 0.0 if size[0] >= size[1] else 90.0  # long side to +X
    scale = length / max(size[0], size[1], 1.0)
    cx, cy = (lo[0] + hi[0]) / 2 * scale, (lo[1] + hi[1]) / 2 * scale
    if yaw == 90.0:  # a point (x, y) turned by +90 lands on (-y, x)
        cx, cy = -cy, cx
    return yaw, scale, [-cx, -cy, -lo[2] * scale], size[2] * scale


def plan(meshes, lengths, flip=()):
    """meshes: (folder, asset name, object path, (min, max)). lengths: {vehicle type: cm}. Returns {type: entry} with the biggest model of each type."""
    folders = {}
    for folder, name, path, box in meshes:
        folders.setdefault(folder, []).append((name, path, box))
    best = {}
    for folder, items in sorted(folders.items()):
        bodies = [i for i in items if not is_part(i[0])]
        # a model imported by import_car_gltf.py or import_car_fbx.py is named by its folder under /Game/Vehicles
        kind = vehicle_type(folder.split("/")[3]) if folder.startswith("/Game/Vehicles/") else None
        for name, _, _ in bodies:
            kind = kind or vehicle_type(name)
        kind = kind or vehicle_type(folder.rsplit("/", 1)[-1])
        if not kind or kind not in lengths or not bodies:
            continue
        boxes = [i[2] for i in items]
        yaw, scale, offset, height = fit(boxes, lengths[kind])
        volume = max(b[1][0] - b[0][0] for b in boxes) * max(b[1][1] - b[0][1] for b in boxes)
        if kind in flip:
            yaw += 180.0
            offset = [-offset[0], -offset[1], offset[2]]
        entry = {"meshes": sorted(i[1] for i in items), "yaw": yaw, "scale": round(scale, 5), "offset": [round(v, 2) for v in offset], "height": round(height, 1), "from": folder}
        if kind not in best or volume > best[kind][0]:
            best[kind] = (volume, entry)
    return {k: v[1] for k, v in best.items()}


def main():
    import unreal
    plugin = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..") if "__file__" in globals() else os.path.join(unreal.Paths.project_plugins_dir(), "NaijaHustleGame")
    data_dir = os.path.join(plugin, "Data")
    lengths = {}
    for name, key in (("naija_rules.json", "vehicles"), ("unreal_vehicles.json", "types")):
        with open(os.path.join(data_dir, name), encoding="utf-8") as fh:
            for kind, spec in json.load(fh)[key].items():
                lengths[kind] = spec["len"]
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    meshes, rigged = [], []
    for root in ROOTS:
        for a in registry.get_assets_by_path(root, recursive=True):
            cls = str(a.asset_class_path.asset_name)
            path = f"{a.package_name}.{a.asset_name}"
            if cls == "SkeletalMesh":
                rigged.append(path)
            elif cls == "StaticMesh":
                box = unreal.EditorAssetLibrary.load_asset(path).get_bounding_box()
                meshes.append((str(a.package_path), str(a.asset_name), path, ((box.min.x, box.min.y, box.min.z), (box.max.x, box.max.y, box.max.z))))
    matched = plan(meshes, lengths, FLIP)
    out = os.path.join(data_dir, "vehicle_meshes.json")
    if matched:
        with open(out, "w", encoding="utf-8") as fh:
            json.dump({"format": "naija-hustle-vehicle-meshes", "note": "Written by Scripts/assign_vehicle_meshes.py. Add 180 to a yaw if that car drives backwards.", "meshes": matched}, fh, indent=1)
    for kind, e in sorted(matched.items()):
        unreal.log(f"NAIJA HUSTLE: {kind} <- {e['from']} ({len(e['meshes'])} meshes, scale {e['scale']}, yaw {e['yaw']:.0f}, {e['height']:.0f} cm high)")
    missing = sorted(set(lengths) - set(matched))
    unreal.log(f"NAIJA HUSTLE: looked at {len(meshes)} static meshes; models for {', '.join(sorted(matched)) if matched else 'nothing'}; still blockout: {', '.join(missing)}"
               + (f"; {len(rigged)} skeletal meshes not used (e.g. {rigged[0]})" if rigged else ""))
    return matched


if __name__ == "__main__":
    main()
