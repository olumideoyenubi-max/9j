"""Creates the NAIJA HUSTLE content folder tree.

Run inside the Unreal Editor: Tools > Execute Python Script... and pick this file
(or type `py "<path>/create_content_folders.py"` in the Output Log's Cmd box).
Needs the Python Editor Script Plugin, which the .uproject enables. Safe to run again.
"""
import unreal

ROOT = "/Game/NaijaHustle"
FOLDERS = [
    "Maps",                      # L_Slice_Street (World Partition), L_Sandbox, L_LookDev
    "Core/GameModes",            # BP_ subclasses of the C++ game mode, game instance
    "Core/Data",                 # data assets and tables: fares, routes, prices, wanted levels
    "Characters/Player",         # protagonist blueprint, outfits
    "Characters/NPC",            # NPC blueprints and presets (MetaHumans themselves import to /Game/MetaHumans)
    "Characters/Clothing",       # retextured garments; ankara and aso-oke pattern textures
    "Animation/MotionMatching",  # pose search databases, choosers
    "Animation/ControlRig",      # look-at rigs
    "Animation/Vehicles",        # enter/exit, driving and conductor poses
    "Vehicles/Danfo",
    "Vehicles/Okada",
    "Vehicles/Keke",
    "Vehicles/Shared",           # wheels, glass, damage blend materials
    "Environment/Kit",           # modular building kit: walls, windows with burglar bars, balconies, roofs
    "Environment/Props",         # water tanks, AC units, generators, kiosks, umbrellas, poles and wires
    "Environment/Signage",       # hand-painted and neon signs (original business names only)
    "Environment/Road",          # asphalt, kerbs, gutters, overpass, bus stop
    "Environment/Decals",        # posters, graffiti, stains, puddles
    "Environment/Materials",     # master materials and instances (Megascans land in /Game/Megascans or /Game/Fab)
    "Lighting/Presets",          # Night_Rain and Day: sky, fog, post-process, exposure
    "FX/Rain",
    "FX/Smoke",
    "FX/Wet",
    "AI/Mass",                   # Mass entity configs for crowds and traffic
    "AI/ZoneGraph",              # lane profiles for pavements and roads
    "AI/StateTrees",
    "Gameplay/Missions",         # "First Day on the Danfo"
    "Gameplay/Wanted",           # Eko Task Force units and response tables
    "Gameplay/Inventory",        # item definitions and icons
    "UI/HUD",
    "UI/Inventory",
    "UI/Phone",                  # Yarns and other phone apps
    "UI/Fonts",
    "Audio/MetaSounds",          # engines, horns, generators
    "Audio/Ambience",            # rain, crowd beds, market
    "Audio/Radio",               # original music only
]

made = 0
for rel in FOLDERS:
    path = f"{ROOT}/{rel}"
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        made += 1
unreal.log(f"NAIJA HUSTLE: {made} folders created, {len(FOLDERS) - made} already there.")
