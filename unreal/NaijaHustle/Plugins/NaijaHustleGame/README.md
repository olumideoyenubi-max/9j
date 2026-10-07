# NaijaHustleGame: NAIJA HUSTLE as a drop-in Unreal plugin

This folder is the whole game so far, packaged so it can go into **any Unreal C++ project**, including yours.
It holds:

- **The city:** the same Lagos as the browser demo (`web/index.html`), built as a blockout level.
- **Lighting:** day, dusty noon, sunset and night rain, switched by the in-game clock.
- **The game:**
  - drivable danfo, keke, okada and cars, with the browser game's handling
  - "First Day on the Danfo" with Baba Driver
  - free conductor shifts on three routes
  - money, the clock, the save game and the HUD

It was written for Unreal Engine 5.4+ and only uses APIs that still exist in later 5.x versions. It has not yet been
compiled against 5.8; if the first build fails, the error list tells us exactly what to adjust.

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
   `Plugins/NaijaHustleGame/Scripts/nh_blockout_materials.py`. It creates the weather parameters and the blockout
   materials under `Content/NaijaHustle`.
6. **Build the level.** **Tools > Execute Python Script…** > `Plugins/NaijaHustleGame/Scripts/build_street_block.py`.
   - It creates `Content/NaijaHustle/Maps/L_Slice_Street` and places the city (about 570 actors; it takes a minute or two).
   - It sets that level's **GameMode Override** to `NHGameMode`, so your project's default game mode and your
     other levels are untouched.
7. **Play.** Open `L_Slice_Street` and press **Play**.

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
- `NHLighting Day|DustyNoon|Sunset|NightRain` sets the lighting.
- `NHCash 50000` adds money.
- `NHReset` starts the story again; reload the level afterwards.

## What's inside

| Path | What |
|---|---|
| `Data/lagos_city.json` | The map: tiles, 530 buildings, shop fronts, signs, lamps, props, road markings, road graph, stops, bays |
| `Data/naija_rules.json` | Routes, fares, conductor numbers, vehicle handling, Baba Driver's lines, missions, outfits |
| `Source/.../Core` | `NHGameMode`, `NHGameData` (loads the JSON), `NHHustleSubsystem` (money, clock, heat, save) |
| `Source/.../Gameplay` | `NHGameDirector` (the mission and conductor shifts), `NHPerson` (passengers, Baba Driver) |
| `Source/.../Vehicles` | `NHVehicle` (arcade handling, crashes and damage, blockout bodies per type, chase camera) |
| `Source/.../Player`, `Input` | `NHPlayerController` (get in and out, E and choices, prompts), `NHCharacter`, Enhanced Input built in code |
| `Source/.../World`, `Lighting` | City tiles, buildings, `NHShapes`, `NHLightingRig` |
| `Source/.../UI` | `NHHUD`: money, stars, clock, minimap, job card, shift status, prompts, dialogue, choices |
| `Scripts/` | Editor scripts (materials, level), with tests that run without the editor |

Both JSON files are exported from the browser game with `node web/tools/export-unreal.js` (in the main repo), so
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
