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

## Vehicle models (Sketchfab)

Downloaded on 2026-10-08 as glTF through the Sketchfab API with the owner's account. All are licensed
**CC Attribution 4.0**: free to use, commercial use allowed, and the author must be credited (this table is the
credit list; the game will need a credits screen before release). Most are real makes, so badges and names need
removing or replacing before a commercial release. The game keeps its own made-up vehicle names.

To add them on another machine: download each as glTF into one folder, one sub-folder per model named as in the
Folder column, then run `Scripts/import_car_gltf.py` and `Scripts/assign_vehicle_meshes.py`
(with `NH_CAR_FLIP=coupesuv,hypercar,luxcoupe,luxsuv,sedan,royalsuv,supersuv,sports,suv`, the types whose
models face backwards). Textures wider than 2048 were shrunk to 2048 first. The floor slab was removed from the
Executive Sedan's glTF before import.

| Game vehicle type | Model | Triangles | Folder | Source |
|---|---|---|---|---|
| coupesuv (Maitama CX Coupe) | "Low Poly BMW X6M Competition" by sharkycat109 | 6,688 | `Content/Vehicles/Coupe_Suv_X6/` | https://sketchfab.com/3d-models/low-poly-bmw-x6m-competition-dbc45a151624413aac9a378b570ddd02 |
| danfo | "Lagos Danfo Bus" by arcoma330 | 40,922 | `Content/Vehicles/Minibus_Danfo/` | https://sketchfab.com/3d-models/lagos-danfo-bus-41b71827dcc94339b627fb79d9a7adb7 |
| hypercar (Zaki W16) | "Koenigsegg One:1" by OneSteven | 25,682 | `Content/Vehicles/Hypercar_One1/` | https://sketchfab.com/3d-models/koenigsegg-one1-b4e5a4c5fa5d4d45acd6715d3325254f |
| keke | "Autorikshaw - Indian Tuk Tuk" by bhagathartworks | 8,128 | `Content/Vehicles/Tricycle_Keke/` | https://sketchfab.com/3d-models/autorikshaw-indian-tuk-tuk-5775d012693741008acff9dad410e92d |
| luxcoupe (Oba Mirage) | "Bentley Continental Supersports" by JUSTGAME | 30,283 | `Content/Vehicles/Grand_Coupe_Supersports/` | https://sketchfab.com/3d-models/bentley-continental-supersports-c68a3f04b76c46c4bf9a06261b16a5d2 |
| luxsedan (Oba Royale) | "Executive Sedan" by rccdesign | 20,568 | `Content/Vehicles/Executive_Sedan/` | https://sketchfab.com/3d-models/executive-sedan-eadf0e3171d74e4999e37898f95578c8 |
| luxsuv (Zuma GX) | "Mercedes Benz G-class W263" by Lexyc16 | 12,518 | `Content/Vehicles/Luxury_Suv_GClass/` | https://sketchfab.com/3d-models/mercedes-benz-g-class-w263-1a2a52b16cad4e618af347461817895c |
| okada | "Honda CB 125 1972ish (Custom Engine)" by drcrazzie | 53,754 | `Content/Vehicles/Motorcycle_Okada/` | https://sketchfab.com/3d-models/honda-cb-125-1972ish-custom-engine-f3b98e8cac7c4480b053f20160341fc7 |
| royalsuv (Oba Monarch) | "Rolls Royce Cullinan" by newvistudios | 8,360 | `Content/Vehicles/Royal_Cullinan/` | https://sketchfab.com/3d-models/rolls-royce-cullinan-61f16a1d5be2403abcdd8fa552f730f7 |
| sedan (Kamsi LE) | "Toyota Camry 40" by tnikita | 18,410 | `Content/Vehicles/Sedan_Camry/` | https://sketchfab.com/3d-models/toyota-camry-40-560c174d073f4a10bb153ded41ada8c9 |
| sports (Eko Veloce) | "Porsche 911 GT3" by ChevroletSS | 239,145 | `Content/Vehicles/Sports_911GT3/` | https://sketchfab.com/3d-models/porsche-911-gt3-78d5c47ab2554c2592b7e499179a0792 |
| supersuv (Lekki Fury) | "Low Poly Lamborghini Urus" by sharkycat109 | 10,348 | `Content/Vehicles/Super_Suv_Urus/` | https://sketchfab.com/3d-models/low-poly-lamborghini-urus-f53a9e268c144aec9cc4b1a7b3f0fe27 |
| suv (Pathmaster V8) | "2009 Toyota Rav4" by niev | 13,748 | `Content/Vehicles/Suv_Rav4/` | https://sketchfab.com/3d-models/2009-toyota-rav4-1ea8ca4618dc4742b60dc0a69022bcc3 |
| tfpick (task force pickup) | "Toyota Hilux Low Poly" by sebodeweb | 6,646 | `Content/Vehicles/Pickup_Hilux/` | https://sketchfab.com/3d-models/toyota-hilux-low-poly-87dc5e8d45724fe6b4b68b1f432976fc |
| truck | "LCT 3000 '95 - Low poly model" by DanielZhabotinsky | 18,947 | `Content/Vehicles/Truck_LCT/` | https://sketchfab.com/3d-models/lct-3000-95-low-poly-model-663a0953c038434a918cb85725c88ffa |

The task force bike (`tfbike`) is still a blockout.

Tried and dropped: "Urus Absolut" by SDC PERFORMANCE (1.26 million triangles, non-commercial licence), a Bugatti
Chiron and a Bentley Continental GT V8 by amogusstrikesback2 (stray parts), "Mercedes W126 Limousine" by fishermans
(a stretch limousine with no materials), "Tata Signa Cargo Truck" by kevin_k_5124 (an articulated lorry, too long
for the truck type), "1996 Toyota 4Runner" by gamegenjason (flat pink, very coarse).

## Player character (MakeHuman, built in Blender)

The default player skin, "Naija man", is generated by `Scripts/build_player_makehuman.py` in Blender 5.2 with the
MPFB (MakeHuman) extension, then brought in with `Scripts/import_player_gltf.py` (it also takes an FBX) and
`Scripts/setup_player_materials.py`. Folder: `Content/Characters/Player/Naija/`. About 21,700 faces, 9 materials.

| Part | Asset | Pack | Licence |
|---|---|---|---|
| Body, skeleton (`game_engine` rig) | MPFB base mesh, macro targets: male, African, muscular | MPFB extension | CC0 |
| Skin | young_african_male | makehuman_system_assets | CC0 |
| Hair, eyes, eyebrows, eyelashes, teeth | short01 (recoloured black), low-poly eyes (brown), eyebrow001, eyelashes01, teeth_base | makehuman_system_assets | CC0 |
| Trousers | cortu_cargo_pants by Cortu (texture recoloured olive) | pants01 | CC0 |
| Singlet | elvs_male_tankshirt1 by Elvaerwyn (texture recoloured white) | shirts03 | CC-BY (credit required) |
| Beard | grinsegold_full_beard by grinsegold | bodyparts06 | CC-BY (credit required) |

Packs come from https://static.makehumancommunity.org/assets/assetpacks.html and unzip into the extension's user
data folder. The download server was unreliable on 2026-10-08: the system assets pack never completed, so the
skin was taken from the part that arrived, the other entries were fetched by byte range, and the eyes came from
the `makehumancommunity/makehuman` GitHub repository.

## Extra player skins (MakeHuman, built in Blender)

Eleven more people for the Character line of the pause menu (`NHSkin <id>`), generated by
`Scripts/build_people_makehuman.py` on the same MPFB base mesh and `game_engine` rig as the Naija man, then brought
in one at a time with `Scripts/import_player_gltf.py` and `Scripts/setup_player_materials.py`. Folders:
`Content/Characters/Player/<Name>/`. Hair, eyebrows and moustache are recoloured by the build script.

| Skin | Body and skin texture | Hair | Clothes |
|---|---|---|---|
| Tunde | man, African; young_african_male | afro01 | male_casualsuit01, shoes01 |
| Emeka | older, heavier man, African; middleage_african_male | short04, grinsegold_moustache | male_elegantsuit01, shoes02 |
| Dayo | man, African; young_african_male | elvs_braided_rows (cornrows) | drednicolson_short-tail_camo_tee, cortu_jeans_shorts, shoes03 |
| Mark | man, European; young_caucasian_male | short02 (brown) | male_casualsuit03, shoes01 |
| Chen | man, East Asian; young_asian_male | short03 | male_casualsuit05, shoes03 |
| Amaka | woman, African; young_african_female | braid01 | elvs_ladies_tank1, toigo_harem_pants, shoes04 |
| Zainab | woman, African; young_african_female | elvs_micky_afro (afro puffs) | female_casualsuit02, shoes05 |
| Ngozi | athletic woman, African; middleage_african_female | short01 (low cut) | female_sportsuit01, shoes06 |
| Kate | woman, European; young_caucasian_female | ponytail01 (blonde) | female_elegantsuit01, shoes04 |
| Mei | woman, East Asian; young_asian_female | bob02 | punkduck_v_neck_top, toigo_wool_pants, shoes05 |
| Priya | woman, South Asian; cutoff3d_indian_female_skin | long01 (dark brown) | drednicolson_asymmetric_tunic_and_sash, toigo_harem_pants, shoes04 |

| Pack | Used from it | Licence |
|---|---|---|
| makehuman_system_assets | the `young_*` / `middleage_*` skins, afro01, bob02, braid01, long01, ponytail01, short01 to short04, the `*suit*` outfits, shoes01 to shoes06, eyes, eyebrows, eyelashes, teeth | CC0 |
| skins01 | cutoff3d_indian_female_skin by cutoff3d | CC0 |
| hair02 | elvs_braided_rows, elvs_micky_afro by Elvaerwyn | CC-BY (credit required) |
| shirts03 | the drednicolson, elvs and punkduck tops | CC-BY (credit required) |
| pants01 | cortu_jeans_shorts by Cortu, toigo_harem_pants and toigo_wool_pants by Toigo | CC0 |
| bodyparts06 | grinsegold_moustache by grinsegold | CC-BY (credit required) |

Only the entries needed were taken from each pack, by byte range, from `files.makehumancommunity.org` (the `files2`
mirror was about 70 times slower on 2026-10-09).

## Second player skin (Sketchfab)

| Asset | Licence | Folder | Notes |
|---|---|---|---|
| "Black Boy Rigged Ready For Ue4" by MotionStudioArts, https://sketchfab.com/3d-models/black-boy-rigged-ready-for-ue4-c9ded09156c244a6b0c3a29820e7f42a | CC Attribution 4.0 (credit required, commercial use allowed) | `Content/Characters/Player/Hustler/` | 18,376 triangles, 5 materials, Unreal 4 mannequin bone names. Stand-in for the player until the real character is made: a young man in a white T-shirt and dark trousers, not the bearded man in a singlet and cargo trousers from the brief. |

Before import the glTF download was edited: bone names had Sketchfab's number suffixes removed, the six parts were merged into one mesh, the upright rotation was moved off the root bone, and vertices, bone offsets and bind matrices were resized so no node carries a scale; textures wider than 2048 were shrunk. Then `Scripts/import_player_gltf.py` imported it and retargeted the mannequin's 21 animation assets onto it (the copies sit beside the originals in `Content/Characters/Mannequins/Anims/Unarmed/`, named `*_Hustler`).

## Clothing (test)

| Item | Asset | Notes |
|---|---|---|
| Trainers | "Air Jordan 1 Low Dior" (https://skfb.ly/pMODv) by VTX, CC Attribution-NonCommercial-ShareAlike 4.0 | **Test only: non-commercial licence and real brands.** Two static meshes, one 2K texture. Folder `Content/Wardrobe/Trainers_LowTop/`. Not rigged. The flat shoe of the pair is worn on both of the player's feet. |
| Two pairs of jeans, two hoodies | Sketchfab and others; sources and licences not recorded yet | Not imported. Unrigged meshes. |

| not added yet | | |

## Surface textures

| Item | Source and licence | Where | Notes |
|---|---|---|---|
| 47 ground, wall, roof, metal and wood surfaces | Poly Haven, https://polyhaven.com, CC0 (public domain, no credit required). The exact asset for each is listed in `Scripts/fetch_surfaces_polyhaven.py`. | `Content/NaijaHustle/Surfaces/` (generated, not in the repo) | Stand-ins for Megascans surfaces. Downloaded to `~/Downloads/nh-surfaces` by the fetch script, imported by `Scripts/import_surfaces.py`. |
