# NAIJA HUSTLE

An original open-world action-adventure set across fictionalized **Lagos**, **Port Harcourt**
and **Abuja**. Built in Unity 6 for Android first (3 GB phones), then PC.

> Rise from danfo conductor on the Lagos mainland, through Port Harcourt's oil money and creeks,
> to the power circles of the capital. Hustle, chase, scheme, and decide who you become.

- **Design:** [`docs/GDD.md`](docs/GDD.md): cities, cast, all 27 story missions, hustles, wanted
  system, checkpoint satire, economy, phone, travel, radio.
- **Tech:** [`docs/TECH.md`](docs/TECH.md): streaming one city at a time, the <200 MB base
  install, memory budgets, device tiers, layers, saves, build settings.

## What's in the repo

| Area | Status |
|---|---|
| Game rules (`Scripts/Core`, engine-free C#) | Done and unit-tested: economy, businesses, garage & mods, outfits, 5-star wanted with city-specific responses, checkpoints, missions, side-hustle shifts, interstate travel, phone apps (chat / transfers / social), weather, ambient events, saves |
| Content (`Resources/Content/*.json`) | 17 districts across 3 cities, 27 story + 3 side missions, 7 hustles, 17 vehicles, 11 mods, 10 outfits, 9 businesses, 8 safehouses, 12 contacts, 7 radio stations, bus mini-events, social posts |
| Unity runtime (`Scripts/Runtime`) | Written, not yet compiled in the Unity editor (no editor in the authoring environment). Covers city streaming (Addressables), touch and PC/gamepad input, third-person camera, car/okada, boat, helicopter and crane physics, traffic, responders, mission hooks, phone UI, radio, HUD, checkpoints, travel flow, device tiers |
| Scenes, art, audio | Not started. Needs greyboxed city scenes and prefabs wired to the scripts |

## Play the browser demo

`web/index.html` is a self-contained, playable **3D** demo of **Act 1: Lagos**, built with Three.js
(loaded from jsDelivr, so it needs a connection the first time). The whole city is generated in code
from the same grid the game logic uses: no model files. It has the danfo conductor hustle ("First Day
on the Danfo" and conductor shifts), Oshoja Motor Park with numbered bays and destination danfos, bus
stops, Gidi Fuel stations, 5 story missions, Task Force chases with search and decay, the checkpoint
"settle" choice, a tailor, a car wash business, day and night with real-time sun shadows, and the
phone (Gist, KoboPay, Yarns, MapAm). Open it in any modern browser on a desktop or phone. Its rules
mirror `Scripts/Core`.

Rendering: physically based materials, a sky that follows the in-game clock, fog that shifts from
afternoon haze to sunset orange to night blue, soft sun shadows, contact shadows, and street detail
(shop fronts, awnings, graffiti and posters, gutters, puddles, street clutter, banners, birds).

Vehicles and people are procedural too. The danfo is a rounded bus shell with real window openings, wheel
arches, its sliding door open, a conductor hanging from the doorway, a loaded roof rack, a destination board
behind the windscreen and a painted slogan on the back. The keke has one steering front wheel with its fork
and handlebar, an arched canopy with rolled-up curtains, a rear bench, and stickers and a plate on the back;
both carry dents, rust and grime. People are jointed rigs (hips, knees, ankles, shoulders, elbows) with
lathe-turned bodies, faces that blink, a range of skin tones, heights and builds, and clothes from tees,
jeans and school uniforms to wrappers, ankara, aso-oke agbada with fila, hijab and gele. They walk, jog,
sprint, idle, sit on the plastic chairs along the streets, wave down danfos and carry trays on their heads.
Near the camera they get the full rig, further out a simpler one, and in the distance a flat sprite. A
**Graphics** setting (Low / Medium / High) is picked automatically for the device and can be changed on
the title screen, in the phone's MapAm app, or with G:

| | Low | Medium | High |
|---|---|---|---|
| Draw distance | 150 m | 190 m | 240 m |
| Anti-aliasing | FXAA | FXAA | 4× MSAA |
| Reflections | none | studio light | live sky |
| Bloom | no | yes | yes |
| Ambient occlusion (GTAO) | no | no | yes |

Controls: WASD or arrows to walk or drive (on foot, movement is relative to the camera), drag the
screen to look around (the camera recentres behind you after 2 seconds), E for actions, F to get in or
out, Space to brake, Tab or I for the inventory (hold Tab for the quick wheel), P for the phone, G for
graphics quality. On a phone, use the joystick and buttons (tap BAG for the inventory, hold it for the wheel).

Inventory: a GTA-style bag with six tabs (Items, Cash, Documents, Clothing, Vehicles, Mission) and an
8-slot quick wheel; the game slows to 30% while either is open. You start with pure water, gala,
puff-puff, an energy drink, a first-aid kit, a toolkit, a jerrycan and your phone, in a 12-slot nylon
bag (a backpack holds 20, a Ghana-Must-Go bag 30). Kiosks, market stalls and Trust In God Medicine Store
sell supplies; the Oshoja Motor Park ticket booth sells a driver's licence and vehicle papers, which Task
Force checkpoints now ask for. Cash on hand, the bank, documents, outfits and the vehicles you've driven
all save with the game.

`web/legacy-2d.html` keeps the earlier top-down 2D version.

## Unreal Engine 5 vertical slice

`unreal/NaijaHustle/` is the UE5 project for a photoreal vertical slice: one Lagos street block at night in the
rain, with the danfo conductor mission. See [`unreal/NaijaHustle/README.md`](unreal/NaijaHustle/README.md) for setup.

## Getting started

1. Install **Unity 6000.0 LTS** with Android Build Support, then open this folder. Unity generates
   `ProjectSettings` and resolves the packages in `Packages/manifest.json`.
2. Run **Naija Hustle → Validate Content**, then **Naija Hustle → Setup Addressables Groups**.
3. Create the `Boot` scene with `GameBootstrap`, `CityStreamer`, `PlayerInputHub`, the player rig
   (`PlayerMotor`, `VehicleInteractor`, `ThirdPersonCamera`), HUD and phone canvases. Create a
   `city_lagos` scene in the `City_Lagos` group with `DistrictZone`s, `TrafficLane`s,
   `TrafficSpawner`, `ResponseDirector`, `AtmosphereController` and `MissionMarker`s.
4. Set up the layers and tags listed in `docs/TECH.md §6`.

## Tests

```bash
dotnet test Tools/CoreTests     # 37 tests: rules, content validation, full-story playthrough
```

The same tests run in Unity's Test Runner (EditMode), and CI runs them on every push.

For a narrated "test run" of the whole story using the real game rules (missions, hustles,
checkpoints, travel, wanted levels, social posts, save/reload):

```bash
dotnet run --project Tools/Playthrough [seed]
```

## Originality

All characters, brands, organisations, police units, apps and landmarks are fictional. The
content test fails on known real-world brand names. See `docs/GDD.md §9`.
