"""Downloads the extra player skins from Sketchfab as glTF, one folder per character.

  SKETCHFAB_TOKEN=<your API token> python3 fetch_sketchfab_characters.py <out folder> [name ...]

The token is on sketchfab.com under Settings, Password & API. With no names it fetches every character in
CHARACTERS; folders that already hold a scene.gltf are left alone. Each folder gets the download's own
license.txt, and models.json in the out folder records what was fetched.

All of these are CC Attribution 4.0 (credit required, commercial use allowed): list each one that goes into
the game in ASSETS.md. Next step for each folder: prep_character_gltf.py in Blender, then import_player_gltf.py.
"""
import io
import json
import os
import sys
import urllib.error
import urllib.request
import zipfile

API = "https://api.sketchfab.com/v3/models/"
# folder name: (Sketchfab uid, what the character is)
CHARACTERS = {
    "Player_Eric": ("a46bc9f67aaa415bb4f3241eef900e7f", "man, short dark hair, waistcoat and tie"),
    "Player_Nathan": ("143a2b1ea5eb4385ae90a73657aca3bc", "man, short hair and beard, grey T-shirt and jeans"),
    "Player_Tarzan": ("3908f5bfd2d944098fe8598d724bd9a8", "man, blond hair, green polo and shorts"),
    "Player_AfricanMan": ("6507aba4e704464086e3ff528e3635e2", "Black man, sleeveless top and cargo trousers"),
    "Player_IndianMan": ("54b9e050d0d6424b848a04bec02a85d0", "South Asian man, side-parted hair, shirt and dhoti"),
    "Player_Kuratchi": ("0bd169adb13346e08dd1c88ca0e46ed3", "East Asian man, cropped hair, check shirt"),
    "Player_Carla": ("acf520f450d14dd799f98a6fede3edf5", "woman, dark curly hair tied back, jacket and trousers"),
    "Player_Claudia": ("c659bd0accab47c6bbe390cf822a2b92", "woman, blonde ponytail, white blouse"),
    "Player_Sophia": ("dc448c3be0e74f96a55fb475a13433cf", "woman, long loose brown hair, top and jeans"),
    "Player_Woman3": ("9ba9fd66bcb8406aa9bffe2588e5660b", "Black woman, beanie, jumper and jeans"),
    "Player_TeenBlack": ("058c9393f51648a0861c732d32a90065", "woman, long straight black hair, jacket and shorts"),
}


def get(url, token=None):
    headers = {"User-Agent": "naija-hustle-assets"}
    if token:
        headers["Authorization"] = f"Token {token}"
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=120) as reply:
        return reply.read()


def main():
    token = os.environ.get("SKETCHFAB_TOKEN", "")
    if len(sys.argv) < 2 or not token:
        raise SystemExit("usage: SKETCHFAB_TOKEN=<token> fetch_sketchfab_characters.py <out folder> [name ...]")
    out = sys.argv[1]
    names = sys.argv[2:] or list(CHARACTERS)
    os.makedirs(out, exist_ok=True)
    record_path = os.path.join(out, "models.json")
    record = json.load(open(record_path)) if os.path.isfile(record_path) else {}
    for name in names:
        uid, look = CHARACTERS[name]
        folder = os.path.join(out, name)
        if os.path.isfile(os.path.join(folder, "scene.gltf")):
            print(f"{name}: already there")
            continue
        try:
            model = json.loads(get(API + uid))
            archive = json.loads(get(API + uid + "/download", token))["gltf"]
            zipfile.ZipFile(io.BytesIO(get(archive["url"]))).extractall(folder)
        except (urllib.error.URLError, KeyError, zipfile.BadZipFile) as error:
            print(f"{name}: FAILED {error!r}")
            continue
        record[name] = {"uid": uid, "name": model["name"], "author": model["user"]["username"], "url": model["viewerUrl"],
                        "license": model["license"]["label"], "faces": model["faceCount"], "animations": model["animationCount"], "look": look}
        print(f"{name}: {model['name']} by {model['user']['username']}, {model['faceCount']} faces, {archive['size'] // 1000} KB")
        json.dump(record, open(record_path, "w"), indent=1)


if __name__ == "__main__":
    main()
