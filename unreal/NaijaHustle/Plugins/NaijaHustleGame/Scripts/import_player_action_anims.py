"""Brings the player's action animations made in Blender (build_player_action_anims.py) into Unreal.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>). Environment variables:

  NH_ANIM_PACK    folder with Naija_Anim_<Clip>.fbx   (default ~/Downloads/nh-characters/Player_Naija/Anims)
  NH_ANIM_SKINS   whose skeletons get the clips: names of folders under /Game/Characters/Player, comma separated,
                  or "all" for every MakeHuman body there   (default Naija)

What it makes (generated, so not stored in the repo): /Game/Characters/Player/<Skin>/Anims/<Skin>_<Clip>, one animation
sequence per clip on that skin's own skeleton: crouching, pistol and rifle shooting, machete fighting and driving. The
MakeHuman bodies share bone names, so one set of files serves them all. Safe to run again: what is there is kept; set
NH_ANIM_REPLACE=1 to import over it after the clips change.
"""
import os
import unreal

PACK = os.environ.get("NH_ANIM_PACK", os.path.join(os.path.expanduser("~"), "Downloads", "nh-characters", "Player_Naija", "Anims"))
ROOT = "/Game/Characters/Player"
MAKEHUMAN = ["Naija", "Tunde", "Emeka", "Dayo", "Amaka", "Zainab", "Ngozi", "Mark", "Kate", "Chen", "Mei", "Priya"]
CLIP_PREFIX = "Naija_Anim_"
REPLACE = os.environ.get("NH_ANIM_REPLACE", "") not in ("", "0")
# clips that play round and round
LOOPS = {"Crouch_Idle", "Crouch_Walk", "Pistol_Aim", "Crouch_Pistol_Aim", "Rifle_Aim", "Rifle_Fire", "Crouch_Rifle_Aim", "Machete_Idle", "Drive_Idle",
         "Pistol_Carry", "Rifle_Carry", "Machete_Carry", "Dance"}

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def say(text):
    unreal.log("ACTION ANIMS: " + text)


def sequences_in(path):
    registry.scan_paths_synchronous([path], True)
    return [a for a in registry.get_assets_by_path(path, recursive=False) if str(a.asset_class_path.asset_name) == "AnimSequence"]


def clip_options(skeleton):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", False)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("skeleton", skeleton)
    return ui


def import_clips(skin):
    mesh = eal.load_asset(f"{ROOT}/{skin}/{skin}") if eal.does_asset_exist(f"{ROOT}/{skin}/{skin}") else None
    if not isinstance(mesh, unreal.SkeletalMesh):
        say(f"{skin}: no skeletal mesh at {ROOT}/{skin}/{skin}, skipped")
        return []
    skeleton = mesh.get_editor_property("skeleton")
    folder = f"{ROOT}/{skin}/Anims"
    clips = []
    for f in sorted(os.listdir(PACK)):
        if not (f.startswith(CLIP_PREFIX) and f.lower().endswith(".fbx")):
            continue
        clip_name = f[len(CLIP_PREFIX):-4]
        name = f"{skin}_{clip_name}"
        if REPLACE or not eal.does_asset_exist(f"{folder}/{name}"):
            before = {str(a.package_name) for a in sequences_in(folder)} if eal.does_directory_exist(folder) else set()
            task = unreal.AssetImportTask()
            for key, value in {"filename": os.path.join(PACK, f), "destination_path": folder, "destination_name": name, "automated": True,
                               "replace_existing": True, "save": False, "options": clip_options(skeleton)}.items():
                task.set_editor_property(key, value)
            tools.import_asset_tasks([task])
            new = [str(a.package_name) for a in sequences_in(folder) if str(a.package_name) not in before]
            if new and not eal.does_asset_exist(f"{folder}/{name}"):
                eal.rename_asset(new[0], f"{folder}/{name}")   # an importer that names the clip after the take, not the file
        clip = eal.load_asset(f"{folder}/{name}")
        if not isinstance(clip, unreal.AnimSequence):
            raise RuntimeError(f"{f} did not import as the animation {folder}/{name}")
        clip.set_editor_property("loop", clip_name in LOOPS)
        clips.append(f"{clip_name} {clip.get_play_length():.2f} s")
    eal.save_directory(folder, only_if_is_dirty=False, recursive=False)
    return clips


def main():
    if not os.path.isdir(PACK) or not any(f.startswith(CLIP_PREFIX) for f in os.listdir(PACK)):
        raise RuntimeError(f"no {CLIP_PREFIX}*.fbx in {PACK} (set NH_ANIM_PACK)")
    wanted = os.environ.get("NH_ANIM_SKINS", "Naija")
    skins = MAKEHUMAN if wanted.strip().lower() == "all" else [s.strip() for s in wanted.split(",") if s.strip()]
    for skin in skins:
        clips = import_clips(skin)
        if clips:
            say(f"{skin}: {len(clips)} clips: " + ", ".join(clips))
    say("done")


if __name__ == "__main__":
    main()
