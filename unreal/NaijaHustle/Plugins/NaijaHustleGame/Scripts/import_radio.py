"""Brings the radio stations' music into Unreal.

Data/radio_stations.json lists the stations and their tracks. The audio is not in the repo: put each track at

    ~/Downloads/nh-radio/<station id>/<file>.wav        (NH_RADIO overrides the folder)

and run this inside the Unreal Editor (Scripts/mac.sh script <this file>). An MP3 has to be made a WAV first:

    afconvert -f WAVE -d LEI16@44100 song.mp3 <file>.wav

It makes /Game/NaijaHustle/Audio/Radio/<station id>/<file>, one stereo Sound Wave a track (generated, so not stored
in the repo). Long music is streamed from disk in chunks by the engine, not held in memory whole. Safe to run again:
a track already in the project is imported again over itself.

Only add music the game has the right to use, and list every track in MUSIC_LICENSES.md.
"""
import json
import os
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.environ.get("NH_RADIO", os.path.join(os.path.expanduser("~"), "Downloads", "nh-radio"))
ROOT = "/Game/NaijaHustle/Audio/Radio"


def main():
    with open(os.path.join(HERE, "..", "Data", "radio_stations.json")) as fh:
        stations = json.load(fh)["stations"]
    tasks, missing = [], []
    for station in stations:
        for track in station["tracks"]:
            path = os.path.join(SRC, station["id"], track["file"] + ".wav")
            if not os.path.isfile(path):
                missing.append(path)
                continue
            task = unreal.AssetImportTask()
            task.filename = path
            task.destination_path = f"{ROOT}/{station['id']}"
            task.destination_name = track["file"]
            task.automated = True
            task.replace_existing = True
            task.save = True
            tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    made = 0
    for task in tasks:
        wave = unreal.load_asset(f"{task.destination_path}/{task.destination_name}")
        if not wave:
            unreal.log_error(f"RADIO: {task.filename} did not import")
            continue
        wave.set_editor_property("looping", False)
        unreal.EditorAssetLibrary.save_loaded_asset(wave)
        made += 1
        unreal.log(f"RADIO: {task.destination_path}/{task.destination_name}: {wave.get_editor_property('duration'):.0f} s")
    for path in missing:
        unreal.log_warning(f"RADIO: no file at {path}")
    unreal.log(f"RADIO: {made} tracks imported, {len(missing)} missing")


main()
