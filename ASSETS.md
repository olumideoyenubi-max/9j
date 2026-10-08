# Third-party assets (not in this repo)

This repo is public and third-party packs may not be redistributed, so they are never committed.
Their folders are in `.gitignore`; each machine re-adds them from Fab.
After adding or removing a pack, update this file with its exact Fab name and resolution.

## In the project now

None. The game uses only engine basic shapes and its own generated materials.

## Optional stand-in body

| Asset | Source | Folder | Notes |
|---|---|---|---|
| Mannequins (Manny), with `ABP_Unarmed` | Unreal Engine 5.8 Third Person template | `Content/Characters/Mannequins/` | About 126 MB. The player wears it if it is there and is a capsule if not. To add it: create a Third Person project in UE 5.8 and copy that folder across. |

## Megascans surfaces (Fab)

None added yet. The look pass expects one surface for each row below, at 2K.
Fill in the exact Fab name when each is added.

| Used for | Surface wanted | Fab asset name | Resolution |
|---|---|---|---|
| Roads | worn asphalt | not added yet | 2K |
| Dusty streets | dry dirt / laterite | not added yet | 2K |
| Walls | painted plaster | not added yet | 2K |
| Roofs | corrugated metal | not added yet | 2K |
| Stall canopies | tarp (tinted blue in the material) | not added yet | 2K |
| Rubble piles | rubble | not added yet | 2K |

Expected folders, under `unreal/NaijaHustle/`: `Content/Megascans/`, `Content/MSPresets/`, `Content/Fab/` or `Content/Quixel/`.
After adding, run `unreal/NaijaHustle/Plugins/NaijaHustleGame/Scripts/assign_megascans.py`.

## Car models (Fab)

One test model is in the Mac project (from Sketchfab, not Fab). The other 15 vehicle types use blockout bodies.
A downloaded FBX is imported with `Scripts/import_car_fbx.py` (run twice), then
`unreal/NaijaHustle/Plugins/NaijaHustleGame/Scripts/assign_vehicle_meshes.py` fits it to its vehicle type.

| Game vehicle type | Asset | Notes |
|---|---|---|
| supersuv (Lekki Fury) | "Urus Absolut" (https://skfb.ly/osAMo) by SDC PERFORMANCE, CC Attribution-NonCommercial 4.0 | **Test only: non-commercial licence and a real make, so it cannot ship.** 1,257,573 triangles, 36 material slots, 4 levels of detail. Folder `Content/Vehicles/SuperSuv_Absolut/`. Too heavy for the M1: see `PROGRESS.md`. |
| sports (Eko Veloce) | "Porsche 911 GT3" (https://skfb.ly/o6Kp9) by ChevroletSS, CC Attribution 4.0 | Not imported: the download is a `.blend` file; needs a glTF or FBX download. Credit required. |

## Clothing (test)

| Item | Asset | Notes |
|---|---|---|
| Trainers | "Air Jordan 1 Low Dior" (https://skfb.ly/pMODv) by VTX, CC Attribution-NonCommercial-ShareAlike 4.0 | **Test only: non-commercial licence and real brands.** Two static meshes, one 2K texture. Folder `Content/Wardrobe/Trainers_LowTop/`. Not rigged. The flat shoe of the pair is worn on both of the player's feet. |
| Two pairs of jeans, two hoodies | Sketchfab and others; sources and licences not recorded yet | Not imported. Unrigged meshes. |

| not added yet | | |
