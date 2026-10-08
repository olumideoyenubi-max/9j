# NAIJA HUSTLE: the Unreal Engine 5.8 game

The browser demo's Lagos as a playable Unreal level: a market street, a motor park, a dusty district, a bus stop
and shops, lit by a day cycle. One protagonist, drivable danfo, keke, okada and cars, and the mission
"First Day on the Danfo". All names, brands and characters are original.

This folder is the Unreal project. The browser demo is in `web/` and the archived Unity project is in
`legacy/unity/`. Current status, frame rate and screenshots are in [`PROGRESS.md`](../../PROGRESS.md).

| Platform | Status |
|---|---|
| macOS, Apple Silicon (M1, 8 GB) | This project builds against UE 5.8 with `Scripts/mac.sh build`, and the game runs with the Mac profile: about 25 fps at 1280×720, scripted playtest passing |
| Windows + RTX | Steps and config written for it; not yet built against UE 5.8 |

| Step | Status |
|---|---|
| 1. Project setup: rendering, folders, input, source control | Done |
| 2. Street block blockout and lighting, from the browser demo's map | Done |
| Gameplay port: vehicles, First Day on the Danfo, conductor shifts, money, clock, save, HUD | Done (in the NaijaHustleGame plugin) |
| Look pass 1: per-surface materials, procedural wear, harsh-morning light, dusty-street detail, sprint camera | Done |
| Visual brief: lighting, post-process, set dressing, decals, materials, effects, camera, showcase, performance | Lighting done; the rest in order |
| Real surfaces (Megascans) and real car models from Fab | Waiting on assets, see [`ASSETS.md`](../../ASSETS.md) |
| Player character, NPCs, crowds, story | Deferred |

## The game is a plugin

Everything (the city, lighting and the gameplay) lives in [`Plugins/NaijaHustleGame`](Plugins/NaijaHustleGame/README.md),
so it can be dropped into any Unreal C++ project. That README covers installing it into your own project and how to play.
This project is a thin shell that loads the plugin.

## Step 2: the street block (blockout and lighting)

The Unreal slice and the browser demo (`web/index.html`) share one Lagos. The browser game's city (streets,
buildings, shop fronts, signs, street lamps, props, road markings, bus stops, fuel stations, park bays, the
road graph) is exported to `Plugins/NaijaHustleGame/Data/lagos_city.json`, and two editor scripts turn that file into a playable
blockout level with day, dusty-noon, sunset and night-rain lighting.

### Build the level

After the first build and first open (sections 1 and 2 below):

1. **Tools > Execute Python Script…** > `Plugins/NaijaHustleGame/Scripts/nh_blockout_materials.py`. It creates
   `MPC_NHWeather` (Wetness, Puddles, Rain, NightLights) and `M_NHBlockout`, the one material the blockout uses.
2. **Tools > Execute Python Script…** > `Plugins/NaijaHustleGame/Scripts/build_street_block.py`. It creates
   `/Game/NaijaHustle/Maps/L_Slice_Street` (World Partition) and places the city. That takes a minute or two
   and asks nothing. Running it again replaces what it made before.
3. Press **Play**. You start at home beside Oshoja Motor Park; the light follows the in-game clock. Press **F1** for the
   lighting menu, **L** to cycle the lighting presets, or type
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
`python3 Plugins/NaijaHustleGame/Scripts/tests/test_build_street_block.py` and
`python3 Plugins/NaijaHustleGame/Scripts/tests/test_blockout_materials.py`. They use a
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

### Mac (Apple Silicon)

1. **Unreal Engine 5.8** from the Epic Games Launcher (Library > Engine Versions > +).
2. **Xcode** from the App Store, opened once so it installs its command-line tools. There is no Visual Studio
   on Mac; Unreal compiles C++ with Xcode's compiler. You don't need to open Xcode to build.
3. **Git LFS** (only needed once binary content is committed): `brew install git-lfs`, then `git lfs install`.

### Windows

1. **Unreal Engine 5.8** from the Epic Games Launcher (Library > Engine Versions > +).
2. **Visual Studio 2022** (Community is fine) with these workloads: *Game development with C++*
   (tick *Unreal Engine installer* and a *Windows 10/11 SDK*), *Desktop development with C++* and
   *.NET desktop development*. JetBrains Rider works too.
3. **Git LFS**: install it from git-lfs.com, then run `git lfs install` once.

## 2. First open

### Mac

`Scripts/mac.sh` wraps the engine's own build and run commands. It expects the engine at
`/Users/Shared/Epic Games/UE_5.8` (the Launcher's default); set `UE_ROOT` if yours is elsewhere.

```bash
cd unreal/NaijaHustle
Scripts/mac.sh build                                                       # compile (several minutes the first time)
Scripts/mac.sh script "$PWD/Plugins/NaijaHustleGame/Scripts/nh_blockout_materials.py"   # make the materials
Scripts/mac.sh script "$PWD/Plugins/NaijaHustleGame/Scripts/build_street_block.py"      # build L_Slice_Street
Scripts/mac.sh play                                                        # run the game in a 1280x720 window
```

- `Scripts/mac.sh editor` opens the project in the Unreal Editor instead. Double-clicking `NaijaHustle.uproject`
  also works, and offers to compile if the build is out of date.
- `Scripts/mac.sh play` is the launcher: it runs the street level as a standalone game, which uses less memory
  than Play inside the editor. Extra arguments are passed on, for example `Scripts/mac.sh play -NHRun=autoplay`.
- The first launch compiles shaders and can take a long while. Later launches are quick.
- The level and materials are generated by the two scripts, so they are not stored in the repo.
- On macOS the editor log is `~/Library/Logs/Unreal Engine/NaijaHustleEditor/NaijaHustle.log`.

### Windows

1. Right-click `NaijaHustle.uproject` > **Generate Visual Studio project files**.
2. Open `NaijaHustle.sln`, choose **Development Editor** and **Win64**, and build the `NaijaHustle` project
   (Ctrl+Shift+B). The first build takes several minutes.
3. Double-click `NaijaHustle.uproject`. The first launch compiles shaders for Lumen, Nanite and ray tracing,
   which can take 10–30 minutes. Later launches are quick.
4. Run the two scripts under **Build the level** above, then press **Play**.

If Play gives you a default spectator instead of the capsule, open **World Settings** (Window > World Settings)
and set *GameMode Override* to `NHGameMode`, or to None so the project default is used.

## 3. What's configured and why

Rendering settings are split into three files. Unreal reads the shared file first and then the one for the
platform it is running on, so the platform file wins.

| File | Used on | What it sets |
|---|---|---|
| `Config/DefaultEngine.ini` | every platform | The shared baseline: Lumen, Virtual Shadow Maps, Nanite, TSR, virtual textures, no baked lighting |
| `Config/Windows/WindowsEngine.ini` | Windows | The RTX profile: DirectX 12, SM6, hardware ray-traced Lumen, GPU skin cache |
| `Config/Mac/MacEngine.ini` | macOS | The M1 8 GB profile: no ray tracing, no Lumen, screen-space reflections, cascaded shadow maps, no mesh distance fields, no volumetric fog, texture pool capped at 800 MB |

| Setting | Shared / Windows | Mac | Why |
|---|---|---|---|
| Global illumination | Lumen | Off (the sky light fakes ground bounce) | Lumen's caches ran the 8 GB M1 out of GPU memory |
| Reflections | Lumen | Screen space | Same |
| Hardware ray tracing | On (Windows only) | Off | Needs DX12, SM6 and an RTX or RX 6000+ GPU |
| Shadows | Virtual Shadow Maps | Cascaded shadow maps | Virtual Shadow Maps need SM6; the M1 falls back to Metal SM5 |
| Nanite | On | Not active | Needs SM6; on Metal SM5 the engine draws fallback meshes |
| Mesh distance fields | On | Off | Only Lumen needs them |
| Volumetric fog | Per lighting preset | Off | Memory; the height fog stays |
| Texture streaming pool | Engine default | 800 MB | CPU and GPU share memory on Apple Silicon |
| Anti-aliasing | TSR | TSR | Render below the window size and upscale |
| Allow static lighting | Off | Off | No lightmap baking; the day cycle is fully dynamic |
| Virtual textures | On | On | Large Megascans textures stream instead of filling memory |
| Custom depth-stencil | Enabled with stencil | same | Interaction highlights and minimap masks |
| RHI / shader format | DirectX 12 / SM6 | Metal (SM5 on the M1) | |

Separately from these files, the lighting rig checks the machine at run time: with 8 GB of memory or less it
turns off Lumen, volumetric fog and the clouds whatever the config says. That check is what fixed the crash on
the M1, and it protects any project the plugin is dropped into.

**World Partition** is set per level, not per project: `build_street_block.py` creates `L_Slice_Street` with
World Partition streaming switched on.

## 4. Code layout

```
Source/NaijaHustle/                 the project module (a shell)
Plugins/NaijaHustleGame/            the game, as a drop-in plugin
  Source/NaijaHustleGame/
    Core/       NHGameMode, NHGameData (reads Data/*.json), NHHustleSubsystem (money, clock, heat, save)
    Gameplay/   NHGameDirector (First Day on the Danfo, conductor shifts), NHPerson
    Vehicles/   NHVehicle (arcade handling, damage, blockout bodies, chase camera)
    Player/     NHPlayerController (get in/out, E, choices, NHLighting, debug commands), NHCharacter
    Characters/ NHCharacterEffectsComponent (sweat, wetness, bruises and wounds; pooled dynamic materials)
                NHAdvancedMovementComponent (weighted sprint, heavy stop, lean, Motion Matching trajectory)
    Debug/      NHDebugPlay (scripted NHAutoplay and NHSelfTest)
    Input/      NHInputSet: every input action and mapping context, built in C++
    World/      NHBlockoutActor, NHCityTile, NHBlockoutBuilding, NHCables, NHShapes
    Lighting/   NHLightingRig
    UI/         NHHUD (canvas HUD and minimap)
  Scripts/      editor Python: materials, street block builder, Megascans and car-model assignment (+ tests/)
  Data/         lagos_city.json and naija_rules.json, exported from the browser demo
Scripts/create_content_folders.py
Scripts/mac.sh                      build, open and play on a Mac
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
| Answer a choice (change, agbero, route) | 1 2 3 4 | D-pad left / right (1, 2) | Global |
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

This repo is public, so third-party content (Megascans, Fab, MetaHumans, engine template packs) must never be
committed: their licences forbid redistribution. Those folders are in the root `.gitignore`, and
[`ASSETS.md`](../../ASSETS.md) lists what to re-add. `.gitignore` also excludes `Binaries/`, `Intermediate/`,
`Saved/` and `DerivedDataCache/`.
