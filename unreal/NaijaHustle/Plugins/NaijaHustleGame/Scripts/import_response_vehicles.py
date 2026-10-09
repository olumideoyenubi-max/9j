"""Brings the Task Force's and the army's vehicles (build_response_vehicles.py) into Unreal and gives them to their types.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>). Environment variable:

  NH_RESPONSE_VEHICLES   folder with the four FBX files   (default ~/Downloads/nh-vehicles/response)

What it makes (generated, so not stored in the repo): /Game/Vehicles/<Name>/SM_<Name> with its materials, for
TaskForce_Van, TaskForce_Pickup, Army_Pickup and Army_Truck. It then adds their four types to
Data/vehicle_meshes.json (tfvan, tfblack, armypick, armytruck: see Data/unreal_vehicles.json), leaving every other
entry in that file as it is. Run it again after the models change: they are imported over.
"""
import json
import os
import sys
import unreal

PACK = os.environ.get("NH_RESPONSE_VEHICLES", os.path.join(os.path.expanduser("~"), "Downloads", "nh-vehicles", "response"))
PLUGIN = os.path.join(unreal.Paths.project_plugins_dir(), "NaijaHustleGame")
sys.path.insert(0, os.path.join(PLUGIN, "Scripts"))
from assign_vehicle_meshes import fit   # the same fitting the other car models get

# model -> (vehicle type, turned round: the models are built facing +X, which arrives in Unreal as given here)
MODELS = {"TaskForce_Van": ("tfvan", False), "TaskForce_Pickup": ("tfblack", False), "Army_Pickup": ("armypick", False), "Army_Truck": ("armytruck", False)}
eal = unreal.EditorAssetLibrary


def bring(name):
    dest = f"/Game/Vehicles/{name}"
    options = unreal.FbxImportUI()
    for key, value in {"import_mesh": True, "import_as_skeletal": False, "import_animations": False, "import_materials": True, "import_textures": False,
                       "mesh_type_to_import": unreal.FBXImportType.FBXIT_STATIC_MESH}.items():
        options.set_editor_property(key, value)
    mesh_options = options.get_editor_property("static_mesh_import_data")
    mesh_options.set_editor_property("combine_meshes", True)
    mesh_options.set_editor_property("auto_generate_collision", False)   # the vehicle has its own box
    mesh_options.set_editor_property("generate_lightmap_u_vs", False)    # lighting is dynamic
    task = unreal.AssetImportTask()
    for key, value in {"filename": os.path.join(PACK, name + ".fbx"), "destination_path": dest, "destination_name": f"SM_{name}", "automated": True,
                       "replace_existing": True, "save": False, "options": options}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    eal.save_directory(dest, only_if_is_dirty=False, recursive=True)
    mesh = eal.load_asset(f"{dest}/SM_{name}")
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"{name}.fbx did not import as the static mesh {dest}/SM_{name}")
    return mesh


def main():
    with open(os.path.join(PLUGIN, "Data", "unreal_vehicles.json"), encoding="utf-8") as fh:
        lengths = {kind: spec["len"] for kind, spec in json.load(fh)["types"].items()}
    path = os.path.join(PLUGIN, "Data", "vehicle_meshes.json")
    with open(path, encoding="utf-8") as fh:
        book = json.load(fh)
    for name, (kind, turned) in MODELS.items():
        mesh = bring(name)
        box = mesh.get_bounding_box()
        yaw, scale, offset, height = fit([((box.min.x, box.min.y, box.min.z), (box.max.x, box.max.y, box.max.z))], lengths[kind])
        if turned:
            yaw += 180.0
            offset = [-offset[0], -offset[1], offset[2]]
        book["meshes"][kind] = {"meshes": [f"/Game/Vehicles/{name}/SM_{name}.SM_{name}"], "yaw": yaw, "scale": round(scale, 5), "offset": [round(v, 2) for v in offset],
                                "height": round(height, 1), "from": f"/Game/Vehicles/{name}"}
        size = box.max - box.min
        unreal.log(f"RESPONSE VEHICLES: {kind} <- {name}: {size.x:.0f} x {size.y:.0f} x {size.z:.0f} cm, {mesh.get_num_triangles(0)} triangles, "
                   f"{len(mesh.get_editor_property('static_materials'))} materials, scale {scale:.3f}, yaw {yaw:.0f}")
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(book, fh, indent=1)
    unreal.log("RESPONSE VEHICLES: done")


if __name__ == "__main__":
    main()
