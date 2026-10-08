# NAIJA HUSTLE (Unreal) progress

**Last updated:** 2026-10-08 (real models on 15 of 16 vehicle types; shoes on the player; visual brief section 2 awaiting review)
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
- **Playtest tooling:** debug commands `NHGoto`, `NHBoard`, `NHAgbero`, `NHFinish`, `NHAutoplay`, `NHSelfTest`. Autoplay passes 26 checks and the self-test has 75 checks, with no plugin warnings.
- **Luxury cars (blockouts):** eight Unreal-only vehicle types with made-up names and 16 parked spots (a second super SUV at Oshodi Motor Park, bay 12). Loader for real models is in place (`assign_vehicle_meshes.py`).
- **Road look:** worn grey asphalt, faded markings, black-and-white kerb stones.
- **Visual brief, section 1 (lighting):** dynamic lighting with faked bounce from the sky light, ambient occlusion, a second haze layer, sun light-shaft bloom, a golden-evening preset (16:00–17:30), an F1 lighting menu, and 54 reflection captures over road junctions.

- **Repo project verified on the Mac (2026-10-08):** `Scripts/mac.sh build` compiles `unreal/NaijaHustle`, the material and level scripts run, the Mac profile is applied (no ray tracing, screen-space reflections, cascaded shadows, 800 MB texture pool), and `NHAutoplay` passes all 26 checks.
- **Repo:** Unity project archived under `legacy/unity/`; Mac and Windows config profiles split (`Config/Mac/MacEngine.ini`, `Config/Windows/WindowsEngine.ini`); Mac build-and-play script `unreal/NaijaHustle/Scripts/mac.sh`.

- **Visual brief, section 2 (post-process and colour):** one global post-process volume with a filmic curve, teal-lifted shadows, warm highlights, film grain, chromatic aberration, clamped slow-moving exposure, and a tweakable colour lookup table (`nh_grade_lut.py` makes `T_NHGrade_LUT`). No measurable frame-rate cost. Autoplay (26) and self-test pass.
- **Character effects (code only):** `UNHCharacterEffectsComponent` and its pooled dynamic materials are in the plugin: sweat, rain wetness, bruises and bullet wounds for skeletal-mesh characters. Lighting presets now set air temperature and humidity in `MPC_NHWeather` for it. Compiles on UE 5.8.
- **Momentum movement (code only):** `UNHAdvancedMovementComponent` is in the plugin: weighted sprint, heavy stop, lean, and a Motion Matching trajectory, with client prediction. Ported to the UE 5.8 trajectory type (`FTransformTrajectory`), so no Pose Search dependency. The self-test sprints and stops a test character on it (top speed 650 cm/s, stop in 0.8 s over 2.8 m, predicted stop location within 1 cm). Self-test now 55 checks.
- **Player stand-in body:** the player now wears the Third Person template mannequin with its walk and run animation, where a project has that content (22.6 fps at the usual spot, no change). Without it the player is still the capsule; the repo's own project was checked running that way. Both playtests pass either way.
- **Real Lagos map names:** the stops, routes, districts and main roads now use real Lagos places (Oshodi, Charity, Anthony, Gbagada, Iyana Oworo, CMS, Fadeyi, Yaba, Tejuosho Market, Olosha, Idi-Oro; Third Mainland Bridge, Makoko, Lagos Island, Lekki, Mushin). Stop ids are unchanged. Applied to the Unreal data, level signs, code and the browser demo; mapping in `docs/LAGOS_MAP.md`.
- **Vehicle paint system:** `UNHVehicleMaterialComponent` on every vehicle (rain wetness with ripples, clear coat paint, up to six crash marks that scrape paint to primer or crack glass) and the matching textureless material `M_NHCarPaint` from `nh_car_paint.py`. Crashes and `Repair()` are wired in. Self-test now 62 checks, including impacts in mesh space, respray, rain and pooling.
- **Vehicle weight, suspension and grip:** `UNHVehicleDynamicsComponent` on every vehicle: slides past the grip limit (speed, handbrake, dirt, wet road) with recovery, sprung body that dives, squats and rolls, wheels traced to the ground, kerb and landing compression, all in fixed sub-steps. Self-test grew to 74 checks.
- **Clean playtests on the final build (2026-10-08, after a restart):** self-test 74 of 74 and autoplay 26 of 26, at normal speed, with no freeze and with GPU occlusion queries left on.
- **Car import route:** `Scripts/import_car_fbx.py` brings a downloaded FBX in as one static mesh and, on a second run, makes three lower levels of detail. Used on a 1.26 million triangle test SUV, which `assign_vehicle_meshes.py` then fitted to the super SUV type.
- **Shoes on the player:** `ANHCharacter` wears one unrigged shoe mesh on both feet (mirrored for the left), fixed to the foot bones from the skeleton's reference pose. In the Mac project it is the test trainers (see `ASSETS.md`); without that mesh nothing is worn. 22.3 fps at the usual spot, self-test 74 and autoplay 26 pass. No screenshot here because the test shoe is a real brand.
- **Real vehicle models (2026-10-08):** 15 of the 16 vehicle types now use downloaded models (danfo, keke, okada, sedan, SUV, pickup, truck and the eight luxury types); only the task force bike is a blockout. `Scripts/import_car_gltf.py` imports a folder of glTF downloads, one combined mesh per car; `assign_vehicle_meshes.py` fits them, taking the type from the import folder's name and the backwards ones from `NH_CAR_FLIP`. The console command `NHCarShow X Y [Folder]` lines up one of every type and photographs them. Real-model vehicles get unseen wheel hubs so suspension and road-surface grip still work. Self-test 75 and autoplay 26 pass. Credits in `ASSETS.md`.

## In progress


- **Player character (MetaHuman):** to be created on a rented cloud PC, because MetaHuman Creator recommends 32 GB of memory and the Mac has 8 GB. Plan: assemble with the UE Optimized pipeline at Low, bring the result to the Mac, force card hair, and measure. No assets will be bought; clothing starts from free items and tinting.

- Waiting for review of section 2 before section 5 (geometry and set dressing).

## Next

Visual brief sections, in order, each reviewed before the next:
5 geometry and set dressing, 4 decals, 3 material depth, 6 atmosphere effects, 7 camera feel, 8 showcase mode, 9 performance.

Waiting on assets (see `ASSETS.md`):

- Megascans surfaces from Fab, then the real-surface pass.
- Car models from Fab, then real vehicle bodies.

Deferred: NPCs and storyline.

## Known bugs and gaps

- **Frame rate fell with the real models:** 14.9 fps at Oshodi Motor Park (2650, 4600), 17.8 fps on the dusty street (was 22.3), 20.1 fps on Lagos Island (was 29.9 with blockouts), one 15-second run each at 1280×720 while the Mac was still busy. The models have one level of detail and up to 56 material slots each (the sports car is 239,000 triangles); levels of detail and merged materials are the next step.
- Real-model cars keep their own colours and materials: the game's paint colours, wetness and crash marks do not show on them, the wheels do not turn, and no driver is shown.
- The models' sizes come from fitting each to its type's length, so a few are too tall (SUV 258 cm, danfo 257 cm, luxury 4x4 226 cm).

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
- The browser demo's own tests were not run after the place renames (Playwright is not installed on the Mac); its script was only syntax-checked. The Unreal playtests were run and pass.
- The map is still the small compressed grid: many real stops and areas are missing (see `docs/LAGOS_MAP.md`).
- The car paint is not on any of the game's cars yet: the blockout bodies use the surface materials, so paint, wetness and crash marks only show on bodies given `MI_NHCarPaint_Body` / `MI_NHCarPaint_Glass` (planned for the real car models).
- The look of the car paint is only partly checked: clean and wet paint were seen on a test body in an earlier, costlier version of the material; the current version's scratches, cracks and ripples have not been seen on screen. Frame-rate cost on a real car is not measured.
- The shoes sit over the stand-in body's own feet, which can poke through; it is not known which foot the shoe model was made for. Trousers and tops cannot be worn this way: they need rigging to the skeleton.
- `NHLookShots` with a relative folder name saves into the engine's `Binaries/Mac` folder; pass a full path.
- **Engine freeze seen twice on 2026-10-08 (not seen in five runs after a restart):** the standalone game stopped inside Unreal's Metal renderer waiting for a GPU occlusion-query result (`FGPUOcclusion::WaitForLastOcclusionQuery`), once before any vehicle moved. Cause not found; it coincided with the Mac being overloaded. Running with `-dpcvars=r.AllowOcclusionQueries=0` avoided it.
- Vehicle dynamics are untested by hand: how the slides, the handbrake and the body movement feel when driving has not been judged, and Chaos Vehicles is not used (the cars are kinematic blockouts with no physics body).
- Not covered by the scripted playtests: change prompts, free conductor shifts, agbero beg/drive-off, real driving between stops.
- All surfaces use flat placeholder textures.

## Screenshots

Player stand-in body (template mannequin), sprint camera.

![stand-in](docs/screenshots/player-standin-sprint.jpg)

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
