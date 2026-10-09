# Storage and partitioning (living-Lagos brief, part 1: setup and baseline)

**Date:** 2026-10-09 · **Machine:** Apple M1, 8 GB, 228 GB disk with **9.3 GB free** · **Branch:** `lagos-real-city`

Step 1 of the living-Lagos brief: the rules everything after it has to keep, what is already in place, and what
the project costs on disk today. Measure again with:

```
python3 Plugins/NaijaHustleGame/Scripts/storage_audit.py        (from unreal/NaijaHustle; no Unreal needed)
```

## Baseline

| | Size |
|---|---|
| The project folder | 3.9 GB |
| of which `Content` (11,208 asset files) | 2.25 GB |
| of which build leftovers (`Intermediate`, the plugin's `Intermediate`) | 1.7 GB |
| Unreal's shared cache (`~/Library/Application Support/Epic/.../DerivedDataCache`) | 3.1 GB |
| Raw sources in `~/Downloads` and on the Desktop (map, characters, surfaces, the Blender city) | 3.5 GB |
| The second Unreal project (`~/Documents/Unreal Projects`, the third-party packs) | 3.2 GB |
| A packaged build | never made: see "Not done" |

| Content | Size | |
|---|---|---|
| Characters | 1,333 MB | 956 MB of it the 16 playable people and their wardrobes; 251 MB the mannequin and every body's copy of its animations |
| Lagos (the city's pieces) | 549 MB | 531 MB of meshes, 15 MB of textures |
| NaijaHustle (surfaces, kit, props, radio, maps) | 196 MB | surfaces 167 MB, radio 25 MB |
| Vehicles | 112 MB | |
| The levels' actor files | 72 MB | |

**The city is not what fills the disk; the people are.** Every person carries their own fitted copy of every
piece of clothing (40 for a man, 54 for a woman, about 47 to 77 MB a person), and their own copy of 21 animations.

## The 20 largest assets, and what to do about each

| MB | Asset | Fix |
|---|---|---|
| 20.0 | `Mannequins/Textures/Manny/T_Manny_02_BN` | The mannequin is only a fallback skin. Drop to 1K, or drop the mannequin from builds. |
| 19.1 | `Audio/Radio/ragebait/bands` | Music: already streamed. Lower its quality setting from the default to about 40 for a third of the size. |
| 16.3 | `Mannequins/Rigs/CR_Mannequin_Body` | A control rig, editor only: exclude from builds. |
| 15.5 | `Player/Amaka/Amaka` | A one-piece body. Halve its textures to 1K (rule: 1K for anything that is not the hero). |
| 15.5, 15.1 | `SKM_Quinn_Simple`, `SKM_Manny_Simple` | As the mannequin above. Quinn is not used at all: delete. |
| 12.2, 11.7, 11.0 | `Player/Runner/*` | The Runner skins are extras the owner preferred not to use: leave out of builds. |
| 11.5 | `Player/Hustler/.../material_normal` | A 4K normal map: 2K. |
| 10.3, 9.5 | `Player/Dayo/Dayo`, `Player/Baba/Baba` | As Amaka. |
| 9.2 × 6 | `Player/<six women>/Wardrobe/Top_PrintTank` | The same top fitted to six bodies, 55 MB. Already kept off passers-by for its triangle count; decimate it or drop it. |
| 8.2 | `Vehicles/Minibus_Danfo/.../Material_0_normal` | 4K normal on the hero vehicle: 2K is the rule. |
| 6.8 | `Vehicles/Executive_Sedan/.../Concrete_pattern_10_diffuse` | A concrete texture that came with a car: unused by the car's visible parts, delete after checking. |

None of these fixes has been applied: they change how things look, so they wait for a go-ahead.

## The rules, and where the project stands on each

| Rule (brief) | State |
|---|---|
| **1.1 Exteriors in the World Partition grid** | In place. `L_Lagos_City` is partitioned, one file per actor: 1,329 pieces always loaded (ground, roads, bridges), 2,447 loaded by cell round the player. |
| **1.1 Interiors as separate streamed levels** | Nothing to apply it to yet: the game has no interiors. The first comes in step 4 (4.1) and will be built this way. |
| **1.1 Data layers** | `DL_Interiors`, `DL_Crowds`, `DL_Props`, `DL_Vehicles` existed; `DL_InteriorNPCs`, `DL_ShopStock`, `DL_EventCrowds` are added to the level script. They appear the next time the level is rebuilt. |
| **1.2 One template per type, a seed per building** | Already how the city's kit buildings, street clutter and passers-by work (nothing stored per building). Interiors will follow it. Clothes do not: see above. |
| **1.3 A pak chunk per district** | Planned and labelled: chunk 0 is everything shared plus Oshodi, and each of 77 other districts with buildings of its own has a chunk (`Data/chunks.json`, `Scripts/setup_chunks.py`, packaging settings in `Config/DefaultGame.ini`). **It saves little as things are:** the districts' own buildings are 353 MB between them (the largest, Odogun, 27 MB); the base chunk is 1,895 MB uncooked because the people, vehicles, roads and terrain are shared. |
| **1.4 Fake most interiors** | Already so: no building is enterable; shopfronts are kit pieces. The interior-mapping window shader is not made yet (step 4). |
| **1.4 Generate, don't store** | Already so for buildings in the rebuilt Oshodi cell, street clutter, passers-by, signs, and all audio except the radio's music. |
| **1.4 Textures: 2K hero, 1K props, 512 clutter** | Not met by the characters and some vehicles (4K maps in the table above). The city's own textures are 15 MB in all. |
| **1.4 Lighting** | All dynamic; no lightmaps at all (0 MB). |
| **1.4 Meshes** | City pieces are instanced by the kit where rebuilt; elsewhere one mesh a piece. Nanite was tested in an earlier step and left off. |
| **1.4 Audio** | The mix makes world sounds mono and generated; the radio's two songs are stereo and streamed. |
| **1.4 Animation** | One mannequin animation set, but copied per body (21 assets each): 168 MB. A shared skeleton would remove the copies; it means rebuilding the people on the mannequin's skeleton. |
| **1.4 Builds** | Settings are in: only the two maps and what they use, no editor content, Oodle compression, a Shipping build, no debug files. |
| **1.4 Hygiene** | Nothing moved or deleted yet. See below. |

## Space that can be had back today, without touching the game

| What | Size | Cost |
|---|---|---|
| Unreal's shared cache | 3.1 GB | The next load of each level recompiles shaders (minutes, once). |
| The project's and plugin's `Intermediate` | 1.7 GB | The next build compiles everything (about ten minutes). |
| Raw sources, to the external drive | 3.5 GB | The import scripts take their folders from environment variables, so they can be pointed at the drive. |
| The second Unreal project, to the external drive | 3.2 GB | It is only the source of the third-party packs already copied into this one. |

That is 11.5 GB against 9.3 GB free. **None of it has been done**: the external drive is not plugged in, and
emptying caches while another session is building would cost it its build.

## Not done in this step

- **No packaged build, so no real chunk sizes.** Cooking needs roughly the size of `Content` again in temporary
  files, on a disk with 9.3 GB free and another session building. The chunk figures above are uncooked sizes.
- **The packaging settings are unproven** for the same reason.
- **No fps, memory or NPC and vehicle counts** for this step: nothing in the running game changed. The last
  measurement, earlier today at Oshodi in a 1280×720 window: 35 ms a frame (28 fps), 5.1 GB for the whole process.
- The texture, mesh and animation fixes in the tables.
