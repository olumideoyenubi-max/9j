"""Builds the street-block blockout level from the shared city data.

Run inside the Unreal Editor (Tools > Execute Python Script...) after nh_blockout_materials.py.
Reads the plugin's Data/lagos_city.json, which web/tools/export-unreal.js writes from the browser demo, so
the Unreal game and the browser game share one Lagos: same streets, buildings, shops, signs and props.

Creates (or reopens) /Game/NaijaHustle/Maps/L_Slice_Street as a World Partition level, removes what a
previous run made (actors tagged NHBlockout), then places:
  - one NHCityTile per 64 m x 64 m (ground, roads, kerbs, lagoon and bridge, markings, props, shop fronts,
    signs, street lamps)
  - one NHBlockoutBuilding per building, plus fuel stations, bus shelters and a pedestrian footbridge
  - a few enterable shops near the start (hollow ground floor with shelves and a counter)
  - the NHLightingRig (night rain to start with) and a PlayerStart at home by Oshoja Motor Park
  - NHGameMode as this level's GameMode Override, so the game runs here without changing your
    project's default game mode
Safe to run again after re-exporting the data.
"""
import json
import math
import os

import unreal

LEVEL = "/Game/NaijaHustle/Maps/L_Slice_Street"
TAG = "NHBlockout"
TILE = 16            # cells per tile side
ENTERABLE_SHOPS = 6  # open shop fronts nearest the start become walk-in shops
try:  # the data sits next to this script, in the plugin folder, wherever the plugin is installed
    DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Data", "lagos_city.json")
except NameError:
    DATA = os.path.join(unreal.Paths.project_plugins_dir(), "NaijaHustleGame", "Data", "lagos_city.json")
QUIET = unreal.PropertyAccessChangeNotifyMode.NEVER  # set everything, then rebuild once

# per-prop look: roughness, metallic, and whether the player bumps into it
PROP_ROUGH = {"tank": 0.45, "trash": 0.3, "gascyl": 0.35, "drum": 0.5, "plank": 0.9, "crown": 0.85, "frond": 0.85, "solar": 0.18}
PROP_METAL = {"gascyl": 0.3, "arm": 0.3, "trafo": 0.4, "solar": 0.6, "ac": 0.2}
PROP_SOLID = {"pole", "trunk", "drum", "gen", "gascyl", "tyres", "sand", "blocks", "stand", "crate", "cooler", "barrow", "rubble", "basin", "rcstand", "goodsBox"}
SHAPES = {"box": "BOX", "cyl": "CYLINDER", "sphere": "SPHERE", "cone": "CONE"}
KINDS = {"house": "HOUSE", "estate": "ESTATE", "tower": "TOWER", "stall": "STALL", "stilt": "STILT"}
FACE = {"east": 1, "west": 2, "south": 4, "north": 8}
SHOP_STATE = {"shutter": "SHUTTER", "half": "HALF", "open": "OPEN", "painted": "PAINTED"}


# ------------------------------------------------------------------------------------------------ helpers
def linear(hex_colour):
    """sRGB '#rrggbb' -> unreal.LinearColor"""
    h = hex_colour.lstrip("#")
    if len(h) == 3:  # short form, as in some sign colours ('#fff')
        h = "".join(c * 2 for c in h)
    def ch(i):
        c = int(h[i:i + 2], 16) / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(ch(0), ch(2), ch(4), 1.0)


def qrot(q, v):
    """Rotates vector v by quaternion q = (x, y, z, w)."""
    x, y, z, w = q
    tx, ty, tz = 2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]), 2 * (x * v[1] - y * v[0])
    return (v[0] + w * tx + (y * tz - z * ty), v[1] + w * ty + (z * tx - x * tz), v[2] + w * tz + (x * ty - y * tx))


def surface(colour, rough, wet=0.7, metal=0.0, glow=0.0, warm=0.0):
    s = unreal.NHSurface()
    s.set_editor_property("color", colour if isinstance(colour, unreal.LinearColor) else linear(colour))
    s.set_editor_property("roughness", rough)
    s.set_editor_property("metallic", metal)
    s.set_editor_property("glow", glow)
    s.set_editor_property("wet", wet)
    s.set_editor_property("glow_warm", warm)
    return s


def clean_text(t):
    # the default text font has no naira sign or middle dot
    return t.replace("₦", "N").replace("·", "-")


class City:
    def __init__(self, data):
        self.d = data
        self.cols, self.rows, self.cell = data["cols"], data["rows"], data["cellSize"]
        self.tiles = data["tiles"]
        du = data["dusty"]
        self.dusty = (du["x"] // self.cell, du["y"] // self.cell, (du["x"] + du["w"]) // self.cell, (du["y"] + du["d"]) // self.cell)

    def at(self, c, r):
        if c < 0 or r < 0 or c >= self.cols or r >= self.rows:
            return "#"
        return self.tiles[r][c]

    def is_dusty(self, c, r):
        c0, r0, c1, r1 = self.dusty
        return c0 <= c < c1 and r0 <= r < r1

    def tile_of(self, x, y):
        return (min(self.cols - 1, max(0, int(x // self.cell))) // TILE, min(self.rows - 1, max(0, int(y // self.cell))) // TILE)


def street_faces(city, b):
    """Bit flags for the sides of a footprint that look onto a street or open ground (where shop fronts go)."""
    c0, r0 = b["x"] // city.cell, b["y"] // city.cell
    c1, r1 = (b["x"] + b["w"]) // city.cell - 1, (b["y"] + b["d"]) // city.cell - 1
    def open_any(cells):
        return any(city.at(c, r) in "RKFGP" for c, r in cells)  # road, pavement, forecourt, open ground, park
    flags = 0
    if open_any([(c1 + 1, r) for r in range(r0, r1 + 1)]):
        flags |= FACE["east"]
    if open_any([(c0 - 1, r) for r in range(r0, r1 + 1)]):
        flags |= FACE["west"]
    if open_any([(c, r1 + 1) for c in range(c0, c1 + 1)]):
        flags |= FACE["south"]
    if open_any([(c, r0 - 1) for c in range(c0, c1 + 1)]):
        flags |= FACE["north"]
    return flags


def shop_owner(buildings, front):
    """The house whose wall a shop front sits on: (building index, face flag, door offset along the face)."""
    yaw = round(front["yaw"]) % 360
    face = {0: "east", 180: "west", 90: "south", 270: "north"}.get(yaw)
    if face is None:
        return None
    for i, b in enumerate(buildings):
        if b["kind"] not in ("house", "estate"):
            continue
        x0, y0, x1, y1 = b["x"], b["y"], b["x"] + b["w"], b["y"] + b["d"]
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        fx, fy = front["x"], front["y"]
        if face == "east" and abs(fx - x1) < 40 and y0 < fy < y1:
            return i, FACE[face], fy - cy
        if face == "west" and abs(fx - x0) < 40 and y0 < fy < y1:
            return i, FACE[face], -(fy - cy)
        if face == "south" and abs(fy - y1) < 40 and x0 < fx < x1:
            return i, FACE[face], -(fx - cx)
        if face == "north" and abs(fy - y0) < 40 and x0 < fx < x1:
            return i, FACE[face], fx - cx
    return None


def footbridge_site(city, start):
    """A crossing over a north-south road, with pavement on both sides for the ramps, nearest the start."""
    best = None
    for v in (2, 12, 22, 32, 42):
        for r in range(1, city.rows - 4):
            cells = [(v - 1, r + k) for k in range(4)] + [(v + 2, r + k) for k in range(4)]
            if all(city.at(c, rr) == "K" for c, rr in cells) and city.at(v, r) == "R" and city.at(v + 1, r) == "R":
                x, y = (v + 1) * city.cell, r * city.cell + city.cell / 2
                d = math.hypot(x - start[0], y - start[1])
                if best is None or d < best[0]:
                    best = (d, x, y)
    return best


# ------------------------------------------------------------------------------------------------ editor side
class Builder:
    def __init__(self, city):
        self.city = city
        self.eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.count = {}

    def open_level(self):
        if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
            self.les.load_level(LEVEL)
            return "opened"
        try:
            self.les.new_level(LEVEL, is_partitioned_world=True)
            return "created (World Partition)"
        except TypeError:  # older Python API without the World Partition flag
            self.les.new_level(LEVEL)
            return "created (convert it with Tools > Convert Level for World Partition)"

    def clear(self):
        n = 0
        for a in self.eas.get_all_level_actors():
            if a.actor_has_tag(TAG):
                self.eas.destroy_actor(a)
                n += 1
        return n

    def spawn(self, cls, x, y, z, yaw=0.0, label=None, folder="NaijaHustle"):
        a = self.eas.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        a.set_editor_property("tags", [unreal.Name(TAG)])
        if label:
            a.set_actor_label(label)
        a.set_folder_path(folder)
        key = cls.__name__
        self.count[key] = self.count.get(key, 0) + 1
        return a

    @staticmethod
    def props(a, values):
        for k, v in values.items():
            a.set_editor_property(k, v, QUIET)
        a.rebuild()

    # ---- tiles
    def tiles(self):
        city, d = self.city, self.city.d
        buckets = {}
        def bucket(x, y):
            return buckets.setdefault(city.tile_of(x, y), {"markings": [], "props": [], "signs": [], "fronts": [], "lamps": []})

        for x0, y0, x1, y1, w, colour in d["markings"]:
            m = unreal.NHMarking()
            m.set_editor_property("a", unreal.Vector2D(x0, y0))
            m.set_editor_property("b", unreal.Vector2D(x1, y1))
            m.set_editor_property("width", w)
            m.set_editor_property("color", linear(colour))
            bucket((x0 + x1) / 2, (y0 + y1) / 2)["markings"].append(m)

        for kind, spec in d["props"].items():
            shape = getattr(unreal.NHShape, SHAPES[spec["shape"]])
            c, s = spec["center"], spec["size"]
            for x, y, z, qx, qy, qz, qw, sx, sy, sz, colour in spec["items"]:
                q = (qx, qy, qz, qw)
                off = qrot(q, (c[0] * sx, c[1] * sy, c[2] * sz))
                t = unreal.Transform(location=unreal.Vector(x + off[0], y + off[1], z + off[2]),
                                     rotation=unreal.Quat(qx, qy, qz, qw).rotator(),
                                     scale=unreal.Vector(max(s[0] * sx, 0.5) / 100.0, max(s[1] * sy, 0.5) / 100.0, max(s[2] * sz, 0.5) / 100.0))
                p = unreal.NHPropInstance()
                p.set_editor_property("kind", kind)
                p.set_editor_property("shape", shape)
                p.set_editor_property("transform", t)
                p.set_editor_property("surface", surface(colour, PROP_ROUGH.get(kind, 0.75), 0.7, PROP_METAL.get(kind, 0.0)))
                p.set_editor_property("solid", kind in PROP_SOLID)
                bucket(x, y)["props"].append(p)

        for sg in d["signs"]:
            s = unreal.NHSign()
            s.set_editor_property("location", unreal.Vector(sg["x"], sg["y"], sg["z"]))
            s.set_editor_property("yaw", sg["yaw"])
            s.set_editor_property("width", sg["w"])
            s.set_editor_property("height", sg["h"])
            s.set_editor_property("title", clean_text(sg["title"]))
            s.set_editor_property("sub", clean_text(sg["sub"]))
            s.set_editor_property("background", linear(sg["bg"]))
            s.set_editor_property("foreground", linear(sg["fg"]))
            bucket(sg["x"], sg["y"])["signs"].append(s)

        for i, f in enumerate(d["shopfronts"]):
            s = unreal.NHShopfront()
            s.set_editor_property("location", unreal.Vector(f["x"], f["y"], f["z"]))
            s.set_editor_property("yaw", f["yaw"])
            s.set_editor_property("width", f["w"])
            s.set_editor_property("height", f["h"])
            s.set_editor_property("state", getattr(unreal.NHShopfrontState, SHOP_STATE[f["state"]]))
            s.set_editor_property("enterable", bool(f.get("enterable")))
            bucket(f["x"], f["y"])["fronts"].append(s)

        for x, y, z in d["lamps"]:
            bucket(x, y)["lamps"].append(unreal.Vector(x, y, z))

        for ty in range((city.rows + TILE - 1) // TILE):
            for tx in range((city.cols + TILE - 1) // TILE):
                c0, r0 = tx * TILE, ty * TILE
                cells = "".join(city.at(c, r) for r in range(r0 - 1, r0 + TILE + 1) for c in range(c0 - 1, c0 + TILE + 1))
                dusty = "".join("1" if city.is_dusty(c, r) else "0" for r in range(r0 - 1, r0 + TILE + 1) for c in range(c0 - 1, c0 + TILE + 1))
                b = buckets.get((tx, ty), {"markings": [], "props": [], "signs": [], "fronts": [], "lamps": []})
                cx, cy = (c0 + TILE / 2) * city.cell, (r0 + TILE / 2) * city.cell
                a = self.spawn(unreal.NHCityTile, cx, cy, 0.0, label=f"Tile_{tx}_{ty}", folder="NaijaHustle/Tiles")
                self.props(a, {"col0": c0, "row0": r0, "cols": TILE, "rows": TILE, "cells": cells, "dusty_mask": dusty,
                               "markings": b["markings"], "props": b["props"], "signs": b["signs"], "shopfronts": b["fronts"], "lamp_heads": b["lamps"]})

    # ---- buildings
    def buildings(self, enterable):
        city = self.city
        for i, b in enumerate(city.d["buildings"]):
            kind = KINDS.get(b["kind"], "HOUSE")
            z = city.d["heights"]["water"] if kind == "STILT" else city.d["heights"]["kerb"]
            cx, cy = b["x"] + b["w"] / 2, b["y"] + b["d"] / 2
            a = self.spawn(unreal.NHBlockoutBuilding, cx, cy, z, label=f"{b['kind'].title()}_{i:03d}", folder=f"NaijaHustle/Buildings/{b['kind'].title()}")
            values = {"kind": getattr(unreal.NHBuildingKind, kind), "size": unreal.Vector2D(b["w"], b["d"]), "height": float(b["h"]),
                      "roof": unreal.NHRoofStyle.ZINC if b["roof"] == "zinc" else unreal.NHRoofStyle.FLAT, "dusty": b["dusty"],
                      "wall_color": linear(b["color"]), "seed": b["seed"], "street_faces": street_faces(city, b)}
            if i in enterable:
                face, offset, width = enterable[i]
                values.update({"enterable_shop": True, "shop_face": face, "shop_door_offset": offset, "shop_door_width": width})
            self.props(a, values)

    def landmarks(self):
        city, d = self.city, self.city.d
        kerb = d["heights"]["kerb"]
        for s in d["stations"]:
            a = self.spawn(unreal.NHBlockoutBuilding, s["x"] + s["w"] / 2, s["y"] + s["d"] / 2, kerb, label=s["name"].replace(" ", "_"), folder="NaijaHustle/Landmarks")
            self.props(a, {"kind": unreal.NHBuildingKind.FUEL_STATION, "size": unreal.Vector2D(s["w"], s["d"]), "height": 500.0, "seed": int(s["x"])})
        for st in d["busStops"]:
            (kx, ky), (wx, wy) = st["kerb"], st["wait"]
            yaw = math.degrees(math.atan2(ky - wy, kx - wx))  # the open side faces the kerb
            a = self.spawn(unreal.NHBlockoutBuilding, wx, wy, kerb, yaw=yaw, label="BusStop_" + st["id"], folder="NaijaHustle/Landmarks")
            self.props(a, {"kind": unreal.NHBuildingKind.BUS_SHELTER, "size": unreal.Vector2D(170, 380), "height": 270.0})
        site = footbridge_site(city, (d["playerStart"]["x"], d["playerStart"]["y"]))
        if site:
            _, x, y = site
            a = self.spawn(unreal.NHBlockoutBuilding, x, y, kerb, label="Footbridge", folder="NaijaHustle/Landmarks")
            self.props(a, {"kind": unreal.NHBuildingKind.FOOTBRIDGE, "size": unreal.Vector2D(4 * city.cell, 250), "height": 550.0})
        return site

    def rig_and_start(self):
        d, city = self.city.d, self.city
        rig = self.spawn(unreal.NHLightingRig, city.cols * city.cell / 2, city.rows * city.cell / 2, 0.0, label="LightingRig", folder="NaijaHustle")
        rig.set_editor_property("preset", unreal.NHLightingPreset.NIGHT_RAIN)
        rig.apply_preset(unreal.NHLightingPreset.NIGHT_RAIN)
        ps = d["playerStart"]
        self.spawn(unreal.PlayerStart, ps["x"], ps["y"], ps["z"] + 100.0, yaw=ps["yaw"], label="PlayerStart_Home", folder="NaijaHustle")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", unreal.NHGameMode.static_class())


def pick_enterable(d, city):
    """The open shop fronts nearest the start whose wall belongs to a house: index -> (face, offset, width)."""
    sx, sy = d["playerStart"]["x"], d["playerStart"]["y"]
    fronts = sorted((f for f in d["shopfronts"] if f["state"] == "open"), key=lambda f: math.hypot(f["x"] - sx, f["y"] - sy))
    chosen = {}
    for f in fronts:
        owner = shop_owner(d["buildings"], f)
        if owner and owner[0] not in chosen and d["buildings"][owner[0]]["h"] >= 600 and owner[1] & street_faces(city, d["buildings"][owner[0]]):
            chosen[owner[0]] = (owner[1], owner[2], f["w"])
            f["enterable"] = True
        if len(chosen) >= ENTERABLE_SHOPS:
            break
    return chosen


def main():
    with open(DATA, encoding="utf-8") as fh:
        data = json.load(fh)
    city = City(data)
    enterable = pick_enterable(data, city)
    b = Builder(city)
    how = b.open_level()
    removed = b.clear()
    with unreal.ScopedSlowTask(4, "Building the NAIJA HUSTLE street block") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "City tiles")
        b.tiles()
        task.enter_progress_frame(1, "Buildings")
        b.buildings(enterable)
        task.enter_progress_frame(1, "Landmarks")
        site = b.landmarks()
        task.enter_progress_frame(1, "Lighting")
        b.rig_and_start()
    b.les.save_current_level()
    unreal.log(f"NAIJA HUSTLE: {LEVEL} {how}; removed {removed} old actors; placed {b.count}; "
               f"{len(enterable)} walk-in shops; footbridge {'at %.0f, %.0f' % site[1:] if site else 'not placed'}")
    return b.count


if __name__ == "__main__":
    main()
