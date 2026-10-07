# NAIJA HUSTLE: Unreal Engine 5 vertical slice

One dense Lagos street block at night in the rain: a market street, a motor park, a junction under an
overpass, a bus stop and a few shops. One protagonist, a drivable danfo and okada, and the mission
"First Day on the Danfo". All names, brands and characters are original.

This folder is the Unreal project. The Unity rules project and the browser demo live elsewhere in the
repo and are untouched.

| Step | Status |
|---|---|
| 1. Project setup: rendering, folders, input, source control | Done |
| 2. Street block blockout and lighting (night rain + day), from the browser demo's map | **This step** |
| 3. Player MetaHuman, third-person controller and camera | |
| 4. Danfo and okada (Chaos Vehicles), enter and exit | |
| 5. Mass AI crowds and traffic | |
| 6. Mission, wanted system, HUD, inventory, phone | |
| 7. Polish: rain FX, puddles, wet shaders, grading, sound | |
| 8. Performance: 60 fps at 1440p on an RTX 3060 with TSR | |

## Step 2: the street block (blockout and lighting)

The Unreal slice and the browser demo (`web/index.html`) share one Lagos. The browser game's city (streets,
buildings, shop fronts, signs, street lamps, props, road markings, bus stops, fuel stations, park bays, the
road graph) is exported to `Data/lagos_city.json`, and two editor scripts turn that file into a playable
blockout level with day, dusty-noon, sunset and night-rain lighting.

### Build the level

After the first build and first open (sections 1 and 2 below):

1. **Tools > Execute Python Script…** > `Scripts/nh_blockout_materials.py`. It creates
   `MPC_NHWeather` (Wetness, Puddles, Rain, NightLights) and `M_NHBlockout`, the one material the blockout uses.
2. **Tools > Execute Python Script…** > `Scripts/build_street_block.py`. It creates
   `/Game/NaijaHustle/Maps/L_Slice_Street` (World Partition) and places the city. That takes a minute or two
   and asks nothing. Running it again replaces what it made before.
3. Press **Play**. You start at home beside Oshoja Motor Park at night in the rain. Press **L** to cycle the lighting presets, or type
   `NHLighting Day` (or `DustyNoon`, `Sunset`, `NightRain`) in the console (the backtick key). In the editor, select
   **LightingRig** in the Outliner and use the **Set Day / Set Dusty Noon / Set Sunset / Set Night Rain** buttons in
   its Details panel.

### What gets placed

| Actor | Count | What it is |
|---|---|---|
| `NHCityTile` | 24 | 64 m × 64 m pieces of the 384 m × 256 m map: asphalt roads (laterite in Oke-Erupe), pavements with kerbs and gutters, open ground, grass, the motor park, fuel forecourts, the lagoon with embankments, Third Lagoon Bridge on piers, 1,852 road markings, 4,872 props (poles, transformers, water tanks, AC units, generators, chairs, umbrellas, drums, trees...), 295 shop fronts (roller shutters, half-open, open with counters, painted), 299 signs with their shop names, and 63 street lamps with spot lights |
| `NHBlockoutBuilding` | 545 | 530 buildings (houses with windows, burglar bars, balconies with laundry, flat roofs or rusty zinc gables, rebar stubs in Oke-Erupe; glass towers on Eko Crest; market stalls; stilt houses), 3 fuel stations, 11 bus shelters and a pedestrian footbridge |
| Walk-in shops | 6 | The open shops nearest the start have a hollow ground floor: a doorway, a tiled floor, shelves of goods, a counter and a strip light that glows at night |
| `NHLightingRig` | 1 | Sun or moon, sky atmosphere, real-time sky light, height fog, volumetric clouds, an unbound post-process volume and the weather parameters |
| `PlayerStart` | 1 | Home, next to Oshoja Motor Park |

Everything is drawn with instanced engine shapes (box, cylinder, sphere, cone) and one material, using
per-instance colour, roughness, metal, night glow and wetness. A building costs about six draw calls. Each actor
rebuilds itself from the data it holds (select one and click **Rebuild**), so edits in the Details panel show
up straight away. Swapping these shapes for the real modular kit and Megascans materials later keeps the layout.

### Lighting presets

Physical units throughout: sun in lux, lamps in lumens, auto exposure in EV100.

| Preset | Sun | Exposure | Weather |
|---|---|---|---|
| Day | 50° up, 75,000 lux | EV 13–15 | dry |
| Dusty noon | 78° up, 95,000 lux, warm beige haze | EV 14–16 | dry, desaturated and warm |
| Sunset | 7° up over the west, 6,000 lux, orange | EV 9–12 | lamps coming on (30%) |
| Night rain | moonlight through cloud (0.3 lux) | EV 1.5–5.5 | wet roads, puddles, street lamps (9,000 lm), lit windows, signs and shops |

Every value is editable on the rig (Details > Lighting > Presets). These are starting points for look development.
Rain particles, ripples and splashes are step 7; step 2 does the wet surfaces and the light.

### Re-export after changing the browser map

```bash
node web/tools/export-unreal.js   # needs Node + Playwright, like the browser tests
```
Then run `build_street_block.py` again. The scripts are checked without the editor by
`python3 Scripts/tests/test_build_street_block.py` and `python3 Scripts/tests/test_blockout_materials.py`. They use a
stand-in `unreal` module that also checks every property name against the C++ headers.

### Notes

- The overpass in the slice brief is a **pedestrian footbridge** over a north–south road near the start, with ramps
  down the pavements. The shared map has no vehicle flyover. One can be added here later without changing the
  browser game.
- 63 street lamps cast light but no shadows. Step 8 decides which lights earn shadows.
- Coordinates: 1 map cell = 4 m = 400 uu. X is east and Y is south. The browser's metres map to centimetres, with
  axes swapped so nothing is mirrored (see the header of `web/tools/export-unreal.js`).

---

## 1. Install (once per machine)

1. **Unreal Engine 5.4** from the Epic Games Launcher (Library > Engine Versions > +). 5.5 also works:
   it offers to convert the project the first time you open it.
2. **Visual Studio 2022** (Community is fine) with these workloads: *Game development with C++*
   (tick *Unreal Engine installer* and a *Windows 10/11 SDK*), *Desktop development with C++* and
   *.NET desktop development*. JetBrains Rider works too.
3. **Git LFS**: install it from git-lfs.com, then run `git lfs install` once.

## 2. First open

1. Right-click `NaijaHustle.uproject` > **Generate Visual Studio project files**.
2. Open `NaijaHustle.sln`, choose **Development Editor** and **Win64**, and build the `NaijaHustle` project
   (Ctrl+Shift+B). The first build takes several minutes.
3. Double-click `NaijaHustle.uproject`. The first launch compiles shaders for Lumen, Nanite and ray tracing,
   which can take 10–30 minutes. Later launches are quick.
4. **Tools > Execute Python Script…** and pick `Scripts/create_content_folders.py`. It creates the folder tree
   under `Content/NaijaHustle` (listed below).
5. Make a test level: **File > New Level > Basic**, then save it as `/Game/NaijaHustle/Maps/L_Sandbox`.
6. Press **Play**. You get a visible capsule with an over-the-shoulder camera. Check that WASD and the mouse
   work, and the left and right sticks on a gamepad. Space jumps, Shift sprints, and F / E print to the
   Output Log (Window > Output Log).

If Play gives you a default spectator instead of the capsule, open **World Settings** (Window > World Settings)
and set *GameMode Override* to `NHGameMode`, or to None so the project default is used.

## 3. What's configured and why

All in `Config/DefaultEngine.ini`. You can see each setting in **Edit > Project Settings**.

| Setting (Project Settings path) | Value | Why |
|---|---|---|
| Engine > Rendering > Global Illumination | Lumen | Fully dynamic bounce light: night and day presets swap instantly, and signs and neon light the street |
| Engine > Rendering > Reflections | Lumen | Wet roads and shop glass reflect signs and headlights |
| Engine > Rendering > Hardware Ray Tracing, plus Lumen "Use Hardware Ray Tracing when available" | On | Sharper reflections on wet surfaces. Needs DX12, SM6 and an RTX or RX 6000+ GPU; otherwise Lumen falls back to software tracing |
| Engine > Rendering > Shadow Map Method | Virtual Shadow Maps | Crisp shadows from many lights (signs, street lamps) on Nanite geometry |
| Engine > Rendering > Nanite | On | Millions of polygons for the building kit and Megascans with no manual LODs |
| Engine > Rendering > Generate Mesh Distance Fields | On | Required by software Lumen |
| Engine > Rendering > Anti-Aliasing Method | TSR | Temporal Super Resolution: render below 1440p and upscale (tuned in step 8) |
| Engine > Rendering > Allow Static Lighting | Off | No lightmap baking; everything is Lumen |
| Engine > Rendering > Support Compute Skin Cache | On | MetaHumans in ray tracing |
| Engine > Rendering > Virtual Textures | On | Large Megascans textures stream instead of filling VRAM |
| Engine > Rendering > Custom Depth-Stencil Pass | Enabled with Stencil | Interaction highlights and minimap masks |
| Platforms > Windows > Default RHI / Shader formats | DirectX 12 / SM6 | Needed by Nanite, VSM and hardware ray tracing |

**World Partition** is set per level, not per project: in step 2 we create `L_Slice_Street` from the
**Open World** template, which has World Partition streaming switched on.

## 4. Code layout

```
Source/NaijaHustle/
  Public|Private/Core/      NHGameMode: default pawn + player controller
  Public|Private/Input/     NHInputSet: every input action and mapping context, built in C++
  Public|Private/Player/    NHPlayerController (switches contexts, NHLighting command), NHCharacter (step-1 test pawn)
  Public|Private/World/     NHBlockoutActor (instanced shapes + surface data), NHCityTile, NHBlockoutBuilding
  Public|Private/Lighting/  NHLightingRig: presets, weather parameters, street lamps
Scripts/                    editor Python: content folders, blockout materials, street block builder (+ tests/)
Data/lagos_city.json        the shared city, exported from the browser demo
```

Gameplay systems go into C++ (vehicles, Mass processors, the wanted system, inventory data). Blueprints
subclass them for tuning, art hookups and UI. Prefix C++ classes with `NH`.

## 5. Input (Enhanced Input)

The actions and mapping contexts are built in code (`UNHInputSet::Build`), so every binding is visible in a
diff and code review. `ANHPlayerController::SetNHInputMode` swaps contexts: **Global** is always on, and one of
**OnFoot**, **Vehicle** or **Menu** sits on top. Menu has the highest priority, so Esc means "back" there,
not "pause".

| Action | Keyboard / mouse | Gamepad | Context |
|---|---|---|---|
| Move | WASD / arrows | Left stick | On foot |
| Look | Mouse | Right stick | On foot, vehicle |
| Jump | Space | A | On foot |
| Sprint | Left Shift | L3 | On foot |
| Interact (enter or steal a vehicle, talk) | F | Y | On foot |
| Action (call passengers, buy, pick up) | E | X | On foot, vehicle |
| Throttle / Brake (reverse when stopped) | W / S | RT / LT | Vehicle |
| Steer | A / D | Left stick X | Vehicle |
| Handbrake | Space | RB | Vehicle |
| Horn | H | L3 | Vehicle |
| Exit vehicle | F | Y | Vehicle |
| Radio | R | D-pad right | Vehicle |
| Look behind | C | R3 | Vehicle |
| Phone | P | D-pad up | Global |
| Inventory | I or Tab | View | Global |
| Quick wheel (hold) | Q | LB | Global |
| Map | M | D-pad down | Global |
| Next lighting preset (look development) | L | | Global |
| Pause | Esc | Menu | Global |
| Back | Esc or Backspace | B | Menu |

## 6. Content folders and naming

Everything we author lives under `Content/NaijaHustle/`. Downloaded content stays where its importer puts
it (`/Game/MetaHumans`, `/Game/Megascans`, `/Game/Fab`), so updates don't fight our folders.

```
NaijaHustle/
  Maps/          Core/{GameModes,Data}
  Characters/{Player,NPC,Clothing}         Animation/{MotionMatching,ControlRig,Vehicles}
  Vehicles/{Danfo,Okada,Keke,Shared}
  Environment/{Kit,Props,Signage,Road,Decals,Materials}
  Lighting/Presets   FX/{Rain,Smoke,Wet}   AI/{Mass,ZoneGraph,StateTrees}
  Gameplay/{Missions,Wanted,Inventory}     UI/{HUD,Inventory,Phone,Fonts}
  Audio/{MetaSounds,Ambience,Radio}
```

Asset prefixes: `BP_` Blueprint, `SM_` static mesh, `SK_` skeletal mesh, `M_` material, `MI_` material instance,
`MF_` material function, `T_` texture (`_D` / `_N` / `_ORM` suffixes), `ABP_` animation Blueprint, `AM_` montage,
`NS_` Niagara system, `MS_` MetaSound, `WBP_` widget, `DA_` data asset, `DT_` data table, `L_` level.

## 7. Source control

**Recommended: Git + Git LFS with file locking.** `.gitattributes` sends `.uasset`, `.umap` and source art
through LFS and marks them *lockable*. Binary assets can't be merged, so lock before you edit:

```bash
git lfs lock Content/NaijaHustle/Maps/L_Slice_Street.umap
git lfs locks                 # who has what
git lfs unlock <path>
```

The editor can show lock state too: **Source Control > Connect to Source Control > Git (beta)**.

**Watch the storage.** One MetaHuman is around a gigabyte of source assets, and Megascans add up quickly.
GitHub's included LFS storage and bandwidth are small; check your plan before pushing large content. Options:

- Keep this repo for code, config and our own assets, and leave re-downloadable Fab/Megascans/MetaHuman
  source out of Git (each machine pulls it from Fab or MetaHuman Creator), or
- Host the project where LFS storage is cheap or included (for example Azure DevOps), or
- Use **Perforce (Helix Core)**, the industry standard for Unreal, with exclusive checkout built into the
  editor. Perforce offers a free tier for small teams; check the current limits.

Before step 2 brings in real content, move this folder into its own repository so Unity and Unreal
assets don't share one LFS quota. `.gitignore` already excludes `Binaries/`, `Intermediate/`, `Saved/` and
`DerivedDataCache/`.
