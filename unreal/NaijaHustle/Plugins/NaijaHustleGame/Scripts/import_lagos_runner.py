"""Brings the Lagos Runner, the player character made in Blender, into Unreal with his outfits and animations.

The character is exported from lagos_city_detailed.blend as Lagos_Runner.fbx (street outfit), Lagos_Runner_Dispatch.fbx
and Lagos_Runner_Suit.fbx on one skeleton (bones named like the mannequin's), Lagos_Runner_Anim_<Clip>.fbx for each
in-place animation, and three baked textures per outfit.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>). Environment variable:

  LAGOS_CHARACTER   folder with those files   (default ~/Desktop/Lagos_Unreal/Character)

What it makes, all under /Game/Characters/Player/Runner (generated, so not stored in the repo):
  Runner, Runner_Dispatch, Runner_Suit   skeletal meshes sharing Runner_Skeleton
  Anims/Runner_<Clip>                    Idle, Walk, Run, Sprint, Jump and the rest, one animation sequence each
  Textures/, Materials/                  one material per outfit (base colour, normal, roughness)

ANHCharacter finds them by those names: the skins "runner", "dispatch" and "suit" (console command NHSkin), moved by
UNHClipAnimInstance with the Idle, Walk, Run, Sprint and Jump clips. Safe to run again: what is there is kept.
"""
import os
import unreal

PACK = os.environ.get("LAGOS_CHARACTER", os.path.join(os.path.expanduser("~"), "Desktop", "Lagos_Unreal", "Character"))
DEST = "/Game/Characters/Player/Runner"
# asset name -> (FBX file, texture set)
OUTFITS = {"Runner": ("Lagos_Runner", "T_Runner"), "Runner_Dispatch": ("Lagos_Runner_Dispatch", "T_Runner_Dispatch"),
           "Runner_Suit": ("Lagos_Runner_Suit", "T_Runner_Suit")}
CLIP_PREFIX = "Lagos_Runner_Anim_"

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def say(text):
    unreal.log("RUNNER: " + text)


def assets_of(path, kind):
    registry.scan_paths_synchronous([path], True)
    return [a for a in registry.get_assets_by_path(path, recursive=False) if str(a.asset_class_path.asset_name) == kind]


def run(filename, destination, name, options=None):
    task = unreal.AssetImportTask()
    for key, value in {"filename": filename, "destination_path": destination, "destination_name": name, "automated": True,
                       "replace_existing": True, "save": False}.items():
        task.set_editor_property(key, value)
    if options:
        task.set_editor_property("options", options)
    tools.import_asset_tasks([task])


# ------------------------------------------------------------------------------------------- textures, materials
def import_textures():
    made = 0
    for f in sorted(os.listdir(PACK)):
        if f.lower().endswith(".png") and not eal.does_asset_exist(f"{DEST}/Textures/{f[:-4]}"):
            run(os.path.join(PACK, f), DEST + "/Textures", f[:-4])
            made += 1
    for _, prefix in OUTFITS.values():
        normal = eal.load_asset(f"{DEST}/Textures/{prefix}_Normal")
        normal.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        normal.set_editor_property("srgb", False)
        normal.set_editor_property("flip_green_channel", True)  # the maps are written OpenGL-style
        rough = eal.load_asset(f"{DEST}/Textures/{prefix}_Roughness")
        rough.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        rough.set_editor_property("srgb", False)
    eal.save_directory(DEST + "/Textures", only_if_is_dirty=False, recursive=False)
    return made


def outfit_material(name, prefix):
    path = f"{DEST}/Materials/M_{name}"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    mat = tools.create_asset("M_" + name, DEST + "/Materials", unreal.Material, unreal.MaterialFactoryNew())

    def sample(suffix, kind, y):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, y)
        node.set_editor_property("texture", eal.load_asset(f"{DEST}/Textures/{prefix}_{suffix}"))
        node.set_editor_property("sampler_type", kind)
        return node

    mel.connect_material_property(sample("BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -300), "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(sample("Roughness", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 0), "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(sample("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 300), "RGB", unreal.MaterialProperty.MP_NORMAL)
    mat.set_editor_property("used_with_skeletal_mesh", True)  # without this the game falls back to the grey checker material
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


# ----------------------------------------------------------------------------------------------- meshes, clips
def mesh_options(skeleton):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_animations", False)   # each outfit file also carries the idle; the clips have their own files
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("create_physics_asset", False)
    if skeleton:
        ui.set_editor_property("skeleton", skeleton)
    data = ui.get_editor_property("skeletal_mesh_import_data")
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


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


def import_outfits():
    skeleton, sizes = None, []
    for name, (fbx, prefix) in OUTFITS.items():
        path = f"{DEST}/{name}"
        if not eal.does_asset_exist(path):
            run(os.path.join(PACK, fbx + ".fbx"), DEST, name, mesh_options(skeleton))
        mesh = eal.load_asset(path)
        if not isinstance(mesh, unreal.SkeletalMesh):
            raise RuntimeError(f"{fbx}.fbx did not import as the skeletal mesh {path}")
        skeleton = skeleton or mesh.get_editor_property("skeleton")
        material = outfit_material(name, prefix)
        slots = mesh.get_editor_property("materials")
        for i in range(len(slots)):                      # a slot read from the array is a copy: change it and put it back
            slot = slots[i]
            slot.set_editor_property("material_interface", material)
            slots[i] = slot
        mesh.set_editor_property("materials", slots)
        if any(slot.get_editor_property("material_interface") != material for slot in mesh.get_editor_property("materials")):
            raise RuntimeError(f"{name}: the material {material.get_name()} did not go onto its slot")
        eal.save_loaded_asset(mesh, only_if_is_dirty=False)   # a property set from Python does not always mark the asset as changed
        box = mesh.get_bounds().box_extent
        bones = len(skeleton.get_reference_pose().get_bone_names())
        sizes.append(f"{name} {box.x * 2:.0f} x {box.y * 2:.0f} x {box.z * 2:.0f} cm, {len(slots)} slot, {bones} bones")
    eal.save_directory(DEST, only_if_is_dirty=True, recursive=False)
    return skeleton, sizes


def import_clips(skeleton):
    folder = DEST + "/Anims"
    clips = []
    for f in sorted(os.listdir(PACK)):
        if not (f.startswith(CLIP_PREFIX) and f.lower().endswith(".fbx")):
            continue
        name = "Runner_" + f[len(CLIP_PREFIX):-4]
        if not eal.does_asset_exist(f"{folder}/{name}"):
            before = {str(a.package_name) for a in assets_of(folder, "AnimSequence")} if eal.does_directory_exist(folder) else set()
            run(os.path.join(PACK, f), folder, name, clip_options(skeleton))
            new = [str(a.package_name) for a in assets_of(folder, "AnimSequence") if str(a.package_name) not in before]
            if new and not eal.does_asset_exist(f"{folder}/{name}"):
                eal.rename_asset(new[0], f"{folder}/{name}")   # an importer that names the clip after the take, not the file
        clip = eal.load_asset(f"{folder}/{name}")
        if not isinstance(clip, unreal.AnimSequence):
            raise RuntimeError(f"{f} did not import as the animation {folder}/{name}")
        clips.append(f"{name[7:]} {clip.get_play_length():.2f} s")
    eal.save_directory(folder, only_if_is_dirty=True, recursive=False)
    return clips


def main():
    if not os.path.isfile(os.path.join(PACK, "Lagos_Runner.fbx")):
        raise RuntimeError(f"Lagos Runner files not found in {PACK} (set LAGOS_CHARACTER)")
    say(f"{import_textures()} textures imported")
    skeleton, sizes = import_outfits()
    say("outfits: " + "; ".join(sizes))
    say(f"skeleton {skeleton.get_path_name()}; clips: " + ", ".join(import_clips(skeleton)))
    say("done")


if __name__ == "__main__":
    main()
