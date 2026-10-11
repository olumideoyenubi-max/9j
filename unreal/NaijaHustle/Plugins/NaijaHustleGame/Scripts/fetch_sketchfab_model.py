"""Downloads one Sketchfab model as glTF into a folder, for any of the import scripts (cars, boats, props).

  SKETCHFAB_TOKEN=<your API token> python3 fetch_sketchfab_model.py <uid> <out folder>

The token is on sketchfab.com under Settings, Password & API; it is read from the environment and written nowhere.
The folder gets scene.gltf with its textures, the download's own license.txt, and model.json (name, author, licence,
link, face count) for the credit line in ASSETS.md. A folder that already holds a scene.gltf is left alone.

The boat: 4d0e69bb45a34f669709c416eec37b15 ("Progress" motorboat by isiosiusin, CC Attribution) into
project-files/downloads/nh-vehicles/Boat_Speedboat, then prep_car_gltf.py, import_car_gltf.py, assign_vehicle_meshes.py.
"""
import io
import json
import os
import sys
import urllib.request
import zipfile

API = "https://api.sketchfab.com/v3/models/"


def get(url, token=None):
    headers = {"User-Agent": "naija-hustle-assets"}
    if token:
        headers["Authorization"] = f"Token {token}"
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=300) as reply:
        return reply.read()


def main():
    token = os.environ.get("SKETCHFAB_TOKEN", "")
    if len(sys.argv) != 3 or not token:
        raise SystemExit("usage: SKETCHFAB_TOKEN=<token> fetch_sketchfab_model.py <uid> <out folder>")
    uid, folder = sys.argv[1], sys.argv[2]
    if os.path.isfile(os.path.join(folder, "scene.gltf")):
        print("already there:", folder)
        return
    model = json.loads(get(API + uid))
    archive = json.loads(get(API + uid + "/download", token))["gltf"]
    os.makedirs(folder, exist_ok=True)
    zipfile.ZipFile(io.BytesIO(get(archive["url"]))).extractall(folder)
    record = {"uid": uid, "name": model["name"], "author": model["user"]["username"], "url": model["viewerUrl"],
              "license": model["license"]["label"], "faces": model["faceCount"]}
    json.dump(record, open(os.path.join(folder, "model.json"), "w"), indent=1)
    print(f"{model['name']} by {record['author']} ({record['license']}), {record['faces']} faces, {archive['size'] // 1000} KB -> {folder}")


if __name__ == "__main__":
    main()
