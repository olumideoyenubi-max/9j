# NaijaHustleGame: NAIJA HUSTLE as a drop-in Unreal plugin

This folder is the whole game so far, packaged so it can go into **any Unreal C++ project**, including yours.
It holds:

- **The city:** the same Lagos as the browser demo (`web/index.html`), built as a blockout level.
- **Lighting:** harsh morning, day, dusty noon, golden evening, sunset and night rain, switched by the in-game clock.
- **The game:**
  - drivable danfo, keke, okada and cars, with the browser game's handling, plus eight luxury cars (Unreal only)
  - "First Day on the Danfo" with Baba Driver
  - free conductor shifts on three routes
  - money, the clock, the save game and the HUD

It was written for Unreal Engine 5.4+ and builds against 5.8 on macOS. On a machine with 8 GB of memory or less the
lighting rig leaves out Lumen, volumetric fog and the clouds, which together run such a machine out of GPU memory.

## Put it in your project

1. **Close the Unreal Editor.**
2. **Copy the plugin.** Copy this whole `NaijaHustleGame` folder into your project's `Plugins` folder, creating
   `Plugins` next to your `.uproject` if it isn't there:
   ```
   YourProject/
     YourProject.uproject
     Source/
     Plugins/
       NaijaHustleGame/        <- this folder
         NaijaHustleGame.uplugin
         Source/  Scripts/  Data/
   ```
3. **Enable it.** Open `YourProject.uproject` in a text editor and add the plugin to the `"Plugins"` list (create the
   list if there isn't one):
   ```json
   "Plugins": [
       { "Name": "NaijaHustleGame", "Enabled": true }
   ]
   ```
   It turns on what it needs by itself: Enhanced Input, Python Editor Script Plugin and Editor Scripting Utilities.
4. **Build.**
   1. Right-click `YourProject.uproject` > **Generate Visual Studio project files**.
   2. Open the `.sln` and build **Development Editor | Win64**.
   3. Open the project.
   If the build stops with errors, copy the first few from Visual Studio's **Error List** and send them over.
5. **Make the materials.** **Tools > Execute Python Script…** >
   `Plugins/NaijaHustleGame/Scripts/nh_blockout_materials.py`. It creates the weather parameters, the surface
   material `M_NHSurface` and one instance per surface type (`MI_NHSurface_Plaster`, `_Concrete`, `_Dirt`, `_Asphalt`,
   `_Zinc`, `_Tarp`, `_Wood`, `_Fabric`, `_Metal`, `_Glass`, `_Generic`) under `Content/NaijaHustle`.
   Then run `Plugins/NaijaHustleGame/Scripts/nh_grade_lut.py` the same way. It makes `T_NHGrade_LUT`, the colour
   lookup table for the film grade. Without it the game still runs, with the rest of the grade.
6. **Build the level.** **Tools > Execute Python Script…** > `Plugins/NaijaHustleGame/Scripts/build_street_block.py`.
   - It creates `Content/NaijaHustle/Maps/L_Slice_Street` and places the city (about 580 actors, with 250 overhead
     cables; it takes a minute or two).
   - It sets that level's **GameMode Override** to `NHGameMode`, so your project's default game mode and your
     other levels are untouched.
7. **Play.** Open `L_Slice_Street` and press **Play**.
8. **Real car models (optional).** Add car models from Fab (static meshes; they land under `/Game/Fab`), then run
   `Plugins/NaijaHustleGame/Scripts/assign_vehicle_meshes.py`. It matches models to vehicle types by name (hypercar,
   sports, luxury, limo, royal, coupe suv, super suv, sedan, suv, minibus, tricycle, motorcycle, truck, pickup), scales each to its type's length and writes
   `Data/vehicle_meshes.json`. A type with no model keeps its blockout body. If a car drives backwards, add 180 to its
   `yaw` in that file.
9. **Real textures (optional).** Add surfaces from Fab or Quixel Bridge (they land under `/Game/Fab` or
   `/Game/Megascans`), then run `Plugins/NaijaHustleGame/Scripts/assign_megascans.py`. It matches texture sets to
   surface types by name (dirt, concrete, plaster, corrugated, tarp, wood, asphalt...) and plugs their BaseColor,
   Normal and ORM maps into the matching `MI_NHSurface_<Type>`. Until then the instances use flat textures plus
   procedural wear: grime patches, a dirt splash band low on walls and a slight tint per building. Each instance has
   `Tiling`, `Grime`, `Splash` and `Variation` values to tune.

## How to play

You start at home beside Oshoja Motor Park, at 8:00 on day 1, with N5,000.

| Key | On foot | In a vehicle |
|---|---|---|
| WASD / mouse | walk, look | W/S throttle and brake (reverse when stopped), A/D steer, mouse to look |
| Shift | sprint | |
| Space | jump | handbrake |
| F | get in the nearest vehicle | get out (when slow) |
| E | talk to Baba Driver | call passengers at a stop, pick a route, shift menu |
| 1–4 | | answer a choice: change, agbero, route, summary |
| H | | horn |
| L | next lighting preset (stops following the clock) | |
| F1 | lighting menu: harsh morning, golden evening, follow the clock | |

1. Walk to **Baba Driver** at the motor park and hear him out.
2. Get in the yellow danfo in **bay 1** (the board says BALO).
3. Drive the route **Oshoja Park → Balo Market (Balo Gate) → Mainland Junction (Iya Basira)**, stopping at each kerb
   (the yellow squares on the minimap). Press **E** to call passengers, and give the right change with **1–3**.
   - At Balo Gate the agbero wants N500: pay, beg, or drive off (one wanted star).
   - Hard braking and fast corners upset passengers, and a smooth ride earns tips.
   - Drive past a stop and its passengers take their money back.
4. Bring the danfo back to the park within two in-game hours (the clock runs at half speed while it counts).
   Baba takes his 40% and you settle up. Wreck the bus or run late and he lets you try again.
5. After that, any danfo in the park is yours to work. Press **E** in one to pick a route:
   - Oshoja – Balo Market
   - Oshoja – Eko Crest Express
   - Mainland Loop

   The owner takes 35%. Step out to end the shift.

Progress (cash, cred, integrity, the clock and finished jobs) saves to the `NaijaHustle` slot after every job.

**Console commands** (open the console with the backtick key):
- `NHLighting Day|DustyNoon|Sunset|NightRain|HarshMorning|GoldenEvening` sets the lighting; `NHLightMenu` (or **F1**)
  opens a small menu: harsh morning, golden evening, or follow the clock.
- `NHTime 9.5` sets the clock (here to 9:30).
- `NHLookShots X Y Yaw [Folder]` measures the frame rate standing still at that spot at 9:30 (logged), then saves two
  look-development screenshots: the street, then a sprint.

**Playtest commands** (not available in Shipping builds; every step is logged with the `NAIJA HUSTLE:` prefix):
- `NHAutoplay` plays "First Day on the Danfo" by script and checks the money, the summary and the save. It starts the
  story again first if the job is already done, which wipes the save.
- `NHSelfTest` checks getting in and out of every parked vehicle, a missed stop, a wrecked bus and the deadline.
- `NHGoto <stop id|park|bay1>`, `NHBoard`, `NHAgbero pay|beg|drive` and `NHFinish` do one step each.
- To run one without the editor, add `-ExecCmds="NHAutoplay"` to the game's command line, or `-NHRun=autoplay` /
  `-NHRun=selftest` to also quit when it finishes.
- `NHCash 50000` adds money.
- `NHReset` starts the story again; reload the level afterwards.

## What's inside

| Path | What |
|---|---|
| `Data/lagos_city.json` | The map: tiles, 530 buildings, shop fronts, signs, lamps, props, road markings, road graph, stops, bays |
| `Data/naija_rules.json` | Routes, fares, conductor numbers, vehicle handling, Baba Driver's lines, missions, outfits |
| `Data/unreal_vehicles.json` | Unreal-only vehicles: eight luxury types with made-up names (Oba Royale saloon, Oba Mirage grand coupe, Oba Monarch and Zuma GX luxury SUVs, Maitama CX Coupe, Lekki Fury super SUV, Eko Veloce sports car, Zaki W16 hypercar) and where they are parked: four at Oshoja Motor Park (bays 8 to 11), eleven on Eko Crest |
| `Source/.../Core` | `NHGameMode`, `NHGameData` (loads the JSON), `NHHustleSubsystem` (money, clock, heat, save) |
| `Source/.../Gameplay` | `NHGameDirector` (the mission and conductor shifts), `NHPerson` (passengers, Baba Driver) |
| `Source/.../Vehicles` | `NHVehicle` (arcade handling, crashes and damage, blockout bodies per type, chase camera) |
| `Source/.../Player`, `Input` | `NHPlayerController` (get in and out, E and choices, prompts), `NHCharacter`, Enhanced Input built in code |
| `Source/.../World`, `Lighting` | City tiles, buildings, `NHCables` (overhead cables), `NHShapes`, `NHLightingRig` |
| `Source/.../UI` | `NHHUD`: money, stars, clock, minimap, job card, shift status, prompts, dialogue, choices |
| `Source/.../Debug` | `NHDebugPlay`: the scripted playtests behind the `NH` console commands |
| `Scripts/` | Editor scripts (materials, level, Megascans textures), with tests that run without the editor |

The first two JSON files are exported from the browser game with `node web/tools/export-unreal.js` (in the main repo), so
rule changes there carry over. They're staged as loose files when you package the game.

## Fitting it to your project

- **Your own character.** Make a Blueprint child of `NHGameMode` and set its Default Pawn Class to your character,
  then use that Blueprint as the level's GameMode Override. Getting in and out of vehicles needs the character to be an
  `NHCharacter` (or a child of it) for now.
- **Name clashes.** All classes start with `NH` and the log category is `LogNHGame`. If your project already has a class
  with the same name (for example `ANHCharacter`), rename one of them.
- **Input.** Your project needs Enhanced Input as its input system, which is the default since UE 5.1. The plugin builds
  its own actions and mapping contexts in code, so there are no input assets to import.

## Not in yet

Ported so far: the city, lighting, vehicles, the first mission and conductor shifts. The browser game has more, and it
comes over in these steps (each waits for your go-ahead):

1. **Traffic and crowds:** cars, danfos, kekes and okadas on the road graph, pedestrians on the pavements, go-slow at rush
   hour, hawkers.
2. **Task Force and the wanted system:** pickups, bikes and the helicopter chasing you, getting busted, checkpoints.
   Until then, stars just cool off on their own.
3. **Missions 2–5:** Go-Slow Getaway, the warehouse job, Kemi's run and Night Bus East.
4. **Inventory and shops:** quick wheel, items, documents, outfits from the tailor, kiosks and the medicine store.
5. **Phone:** Yarns, KoboPay, MapAm, and the radio.
6. **Look:** MetaHumans, Chaos Vehicles with real meshes, rain FX, sound.

## The colour grade

One unbound post-process volume on the lighting rig grades the whole game. Each lighting preset carries its own
values (select **LightingRig** > Details > Lighting > Presets > Grade):

| Setting | What it does |
|---|---|
| Film Toe, Film Shoulder | The filmic curve: denser shadows, softer highlight roll-off |
| Shadow Tint, Shadow Lift | Lifts the shadows towards a colour (teal by default) |
| Highlight Tint | Tints the highlights (warm by default) |
| Film Grain, Chromatic Aberration | Grain and colour fringing at the frame edges |
| Lut Intensity | How much of the rig's **Grade LUT** is mixed in |
| Saturation, Contrast, White Temp, Bloom, Vignette, Sharpen | As before |
| Exposure Min/Max EV100, and **Exposure Speed** on the rig | The exposure clamp and how fast it adapts |

The lookup table is made by `Scripts/nh_grade_lut.py`. To change it, edit `GRADE` at the top of that script and run it
again, or grade the PNG it writes (`Saved/NaijaHustle/T_NHGrade_LUT.png`) in an image editor and import it over the
asset. Harsh morning and golden evening use the full grade; the other presets keep neutral values for now.
