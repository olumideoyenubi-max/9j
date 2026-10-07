# NAIJA HUSTLE: Unreal Engine 5 vertical slice

One dense Lagos street block at night in the rain: a market street, a motor park, a junction under an
overpass, a bus stop and a few shops. One protagonist, a drivable danfo and okada, and the mission
"First Day on the Danfo". All names, brands and characters are original.

This folder is the Unreal project. The Unity rules project and the browser demo live elsewhere in the
repo and are untouched.

| Step | Status |
|---|---|
| 1. Project setup: rendering, folders, input, source control | **This step** |
| 2. Street block blockout and lighting (night rain + day) | Next |
| 3. Player MetaHuman, third-person controller and camera | |
| 4. Danfo and okada (Chaos Vehicles), enter and exit | |
| 5. Mass AI crowds and traffic | |
| 6. Mission, wanted system, HUD, inventory, phone | |
| 7. Polish: rain FX, puddles, wet shaders, grading, sound | |
| 8. Performance: 60 fps at 1440p on an RTX 3060 with TSR | |

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
  Public|Private/Core/    NHGameMode: default pawn + player controller
  Public|Private/Input/   NHInputSet: every input action and mapping context, built in C++
  Public|Private/Player/  NHPlayerController (switches contexts), NHCharacter (step-1 test pawn)
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
