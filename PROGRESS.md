# NAIJA HUSTLE (Unreal) progress

**Last updated:** 2026-10-08 (character effects and momentum movement code added; visual brief section 2 awaiting review)
**Engine:** Unreal Engine 5.8, macOS, Metal SM5
**Test machine:** Apple M1, 8 GB RAM

## Latest frame rate

Standalone game, 1280×720 window, standing on the dusty street (8000, 14000, facing west), 15-second average.

| Lighting | Frame rate | Frame time |
|---|---|---|
| Harsh morning, 9:30 | 22.9 fps | 43.7 ms |
| Golden evening, 17:00 | 23.5 fps | 42.5 ms |
| Harsh morning, 9:30, repo project with `Config/Mac/MacEngine.ini` | 24.8 fps | 40.3 ms |

The first two rows were measured in a UE 5.8 template project with its default config (Lumen switched off at run time by the lighting rig). The third is this repo's own project with the Mac profile; the picture looks the same.

The target is 30 fps at 720p–900p; the game is not there yet.

## Done

- **Plugin on UE 5.8:** `NaijaHustleGame` compiles and runs on the Mac, inside a UE 5.8 Third Person template project. One API fix (JSON map keys).
- **8 GB memory fix:** on machines with 8 GB or less the lighting rig turns off Lumen GI, volumetric fog and clouds, and uses screen-space reflections. This stopped a Metal out-of-memory crash on Play.
- **Look pass 1:** surface-type material pipeline (`M_NHSurface` and one `MI_NHSurface_<Type>` per surface), overhead cables, ruts and rubbish on dusty streets, harsh-morning preset, over-the-shoulder sprint camera.
- **Playtest tooling:** debug commands `NHGoto`, `NHBoard`, `NHAgbero`, `NHFinish`, `NHAutoplay`, `NHSelfTest`. Autoplay passes 26 checks and the self-test passes 55, with no plugin warnings.
- **Luxury cars (blockouts):** eight Unreal-only vehicle types with made-up names and 15 parked spots. Loader for real models is in place (`assign_vehicle_meshes.py`).
- **Road look:** worn grey asphalt, faded markings, black-and-white kerb stones.
- **Visual brief, section 1 (lighting):** dynamic lighting with faked bounce from the sky light, ambient occlusion, a second haze layer, sun light-shaft bloom, a golden-evening preset (16:00–17:30), an F1 lighting menu, and 54 reflection captures over road junctions.

- **Repo project verified on the Mac (2026-10-08):** `Scripts/mac.sh build` compiles `unreal/NaijaHustle`, the material and level scripts run, the Mac profile is applied (no ray tracing, screen-space reflections, cascaded shadows, 800 MB texture pool), and `NHAutoplay` passes all 26 checks.
- **Repo:** Unity project archived under `legacy/unity/`; Mac and Windows config profiles split (`Config/Mac/MacEngine.ini`, `Config/Windows/WindowsEngine.ini`); Mac build-and-play script `unreal/NaijaHustle/Scripts/mac.sh`.

- **Visual brief, section 2 (post-process and colour):** one global post-process volume with a filmic curve, teal-lifted shadows, warm highlights, film grain, chromatic aberration, clamped slow-moving exposure, and a tweakable colour lookup table (`nh_grade_lut.py` makes `T_NHGrade_LUT`). No measurable frame-rate cost. Autoplay (26) and self-test pass.
- **Character effects (code only):** `UNHCharacterEffectsComponent` and its pooled dynamic materials are in the plugin: sweat, rain wetness, bruises and bullet wounds for skeletal-mesh characters. Lighting presets now set air temperature and humidity in `MPC_NHWeather` for it. Compiles on UE 5.8.
- **Momentum movement (code only):** `UNHAdvancedMovementComponent` is in the plugin: weighted sprint, heavy stop, lean, and a Motion Matching trajectory, with client prediction. Ported to the UE 5.8 trajectory type (`FTransformTrajectory`), so no Pose Search dependency. The self-test sprints and stops a test character on it (top speed 650 cm/s, stop in 0.8 s over 2.8 m, predicted stop location within 1 cm). Self-test now 55 checks.

## In progress

- Waiting for review of section 2 before section 5 (geometry and set dressing).

## Next

Visual brief sections, in order, each reviewed before the next:
5 geometry and set dressing, 4 decals, 3 material depth, 6 atmosphere effects, 7 camera feel, 8 showcase mode, 9 performance.

Waiting on assets (see `ASSETS.md`):

- Megascans surfaces from Fab, then the real-surface pass.
- Car models from Fab, then real vehicle bodies.

Deferred: player character, NPCs and storyline.

## Known bugs and gaps

- Opening the editor adds an `AndroidFileServer` block with a generated `SecurityToken` to `Config/DefaultEngine.ini`. Discard that change before committing (`git checkout unreal/NaijaHustle/Config/DefaultEngine.ini`).
- Nothing has been built on Windows against UE 5.8.
- Frame rate is about 22 fps at 1280×720, under the 30 fps target.
- Nanite and Virtual Shadow Maps are not active on this Mac (the engine falls back to Metal SM5), so shadows are cascaded shadow maps.
- The full grade is only on the harsh-morning and golden-evening presets; day, dusty noon, sunset and night rain keep neutral grade values.
- Chromatic aberration shows as colour fringes on poles near the frame edge; it may want turning down.
- Shaded areas are still dark; the faked bounce is a mild lift.
- At golden evening the road ruts cast long shadows that read as holes.
- Reflection captures are placed but their effect is unconfirmed; nothing in the blockout is shiny enough to show them.
- Light-shaft cards in alleys are not done.
- Fog glow toward the sun was removed because it washed out the picture.
- Play-in-Editor has not been retested since the memory fix; testing has been in the standalone game.
- Character effects are untested on a real character: there is no skeletal-mesh character or material with the sweat, wetness and damage parameters yet. Only the material pool and the weather values are checked by the self-test.
- The momentum movement component is not on the player yet (the player moves as before), and its lean, turning grip and network prediction are untested. Motion Matching itself needs a skeletal character and an animation database.
- Not covered by the scripted playtests: change prompts, free conductor shifts, agbero beg/drive-off, real driving between stops.
- All vehicles are blockout shapes; all surfaces use flat placeholder textures.

## Screenshots

Section 2, post-process and colour. Before is the section 1 "after".

| Harsh morning, graded | Golden evening, graded |
|---|---|
| ![morning](docs/screenshots/section2-after-morning.jpg) | ![evening](docs/screenshots/section2-after-golden-evening.jpg) |

Section 1, lighting. Same camera spot on the dusty street.

| Before, harsh morning | After, harsh morning |
|---|---|
| ![before](docs/screenshots/section1-before-morning.jpg) | ![after](docs/screenshots/section1-after-morning.jpg) |

| Golden evening | Golden evening, sprint camera |
|---|---|
| ![golden evening](docs/screenshots/section1-after-golden-evening.jpg) | ![sprint](docs/screenshots/section1-after-golden-evening-sprint.jpg) |

## Where things are

- `unreal/NaijaHustle/`: the Unreal project. All game code, data, editor scripts and tests are in `Plugins/NaijaHustleGame/`.
- The level `L_Slice_Street` and the materials are generated by `nh_blockout_materials.py` and `build_street_block.py`, so they are not stored in the repo.
- `web/`: the browser demo. `legacy/unity/`: the archived Unity project.
- Third-party packs are never committed; see `ASSETS.md`.
