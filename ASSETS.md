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

None added yet. All 16 vehicle types use blockout bodies.
After adding, run `unreal/NaijaHustle/Plugins/NaijaHustleGame/Scripts/assign_vehicle_meshes.py`.

| Game vehicle type | Fab asset name | Notes |
|---|---|---|
| not added yet | | |
