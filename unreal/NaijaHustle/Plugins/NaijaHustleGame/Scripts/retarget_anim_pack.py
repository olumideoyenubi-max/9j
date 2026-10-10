"""Moves the clips of a downloaded animation pack onto the player's own skeleton and exports one FBX a clip for Unreal.

  blender -b --factory-startup --python retarget_anim_pack.py -- <Naija.fbx> <pack.glb> <out folder> [Clip=Name,Clip=Name,...]

For a pack whose skeleton has the mannequin's bone names (with or without a "_012" number on the end of each), like
Sketchfab's "Third Person Animations" by Artem_Dubinin. Writes <out>/Naija_Anim_<Name>.fbx at 30 frames a second, the
same kind of file build_player_action_anims.py writes, so import_player_action_anims.py brings them in for every body.
The last argument renames clips and chooses which are taken (default: all, under their own names).

How: the pack's reference pose is the first frame of its Idle. On every frame each bone of the body is turned by what
the pack's bone of the same name has turned from that reference, after first lining the body's rest pose up with the
reference (so a T-pose body stands at ease when the pack does). The hips are moved by the pack's hips, scaled by leg length.
"""
import bpy, os, re, sys
from mathutils import Matrix, Quaternion, Vector

args = sys.argv[sys.argv.index("--") + 1:]
BODY, PACK, OUT = args[:3]
NAMES = dict(pair.split("=") for pair in args[3].split(",")) if len(args) > 3 else None
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=BODY, automatic_bone_orientation=False, ignore_leaf_bones=False)
rig = bpy.data.objects["Armature"]
bpy.ops.import_scene.gltf(filepath=PACK)
src = next(o for o in bpy.data.objects if o.type == "ARMATURE" and o is not rig)
scene = bpy.context.scene
scene.render.fps = 30
pack_fps = 24.0

# the pack's bone for each of the body's: the same name, or the same name with a number after it
by_base = {}
for b in src.data.bones:
    base = re.sub(r"_\d+$", "", b.name)
    by_base.setdefault(base, b.name)
PAIR = {b.name: by_base[b.name] for b in rig.data.bones if b.name in by_base}
NEXT = {"spine_01": "spine_02", "spine_02": "spine_03", "spine_03": "neck_01", "neck_01": "head"}
for s in "lr":
    NEXT.update({f"clavicle_{s}": f"upperarm_{s}", f"upperarm_{s}": f"lowerarm_{s}", f"lowerarm_{s}": f"hand_{s}", f"hand_{s}": f"middle_01_{s}",
                 f"thigh_{s}": f"calf_{s}", f"calf_{s}": f"foot_{s}", f"foot_{s}": f"ball_{s}"})
    for finger in ("thumb", "index", "middle", "ring", "pinky"):
        NEXT.update({f"{finger}_01_{s}": f"{finger}_02_{s}", f"{finger}_02_{s}": f"{finger}_03_{s}"})


def turn(m):
    return m.to_3x3().normalized().to_quaternion()


rig_w, src_w = rig.matrix_world, src.matrix_world
rest_t = {b.name: turn(rig_w @ b.matrix_local) for b in rig.data.bones}
head_t = {b.name: rig_w @ b.head_local for b in rig.data.bones}
# The pack's own rest pose is no use as a reference (in this one the bones are folded together, in other units, and the
# clips stand the root up and scale it): the reference is the first frame of its Idle, a body standing at ease.
stand = bpy.data.actions.get("Idle") or bpy.data.actions[0]
src.animation_data_create()
src.animation_data.action = stand
if getattr(stand, "slots", None):
    src.animation_data.action_slot = stand.slots[0]
scene.frame_set(int(stand.frame_range[0]))
rest_s = {pb.name: turn(src_w @ pb.matrix) for pb in src.pose.bones}
head_s = {pb.name: src_w @ pb.head for pb in src.pose.bones}
# does the pack face the way the body does? By the feet: the toes are in front of the ankles. (Not by the hands, which
# a standing pose may hold anywhere.)
toes_s, toes_t = head_s[PAIR["ball_l"]] - head_s[PAIR["foot_l"]], head_t["ball_l"] - head_t["foot_l"]
flip = Quaternion((0, 0, 1), 3.14159265) if toes_s.x * toes_t.x + toes_s.y * toes_t.y < 0 else Quaternion()
line_up = {}
for name, child in NEXT.items():
    if name in PAIR and child in PAIR:
        want = flip @ (head_s[PAIR[child]] - head_s[PAIR[name]])
        have = head_t[child] - head_t[name]
        if want.length > 1e-6 and have.length > 1e-6:
            line_up[name] = have.rotation_difference(want)
legs = (head_t["thigh_l"].z - head_t["foot_l"].z) / max(head_s[PAIR["thigh_l"]].z - head_s[PAIR["foot_l"]].z, 1e-6)
order = [b.name for b in rig.data.bones]
parent = {b.name: (b.parent.name if b.parent else None) for b in rig.data.bones}
pelvis_rest = rig.data.bones["pelvis"].matrix_local.to_3x3()
to_rig = rig_w.inverted().to_3x3()
print(f"RETARGET: {len(PAIR)} of the body's {len(order)} bones have a bone in the pack; rest pose lined up on {len(line_up)}; pack is {'turned round' if flip.angle > 1 else 'facing the same way'}; legs {legs:.3f}")

os.makedirs(OUT, exist_ok=True)
made = []
for act in list(bpy.data.actions):
    if NAMES is not None and act.name not in NAMES:
        continue
    name = NAMES[act.name] if NAMES else act.name
    src.animation_data_create()
    src.animation_data.action = act
    if getattr(act, "slots", None):
        src.animation_data.action_slot = act.slots[0]
    first, last = act.frame_range
    frames = max(1, round((last - first) * 30.0 / pack_fps))
    tracks, before = {}, {}
    hips_rest = head_s[PAIR["pelvis"]]
    for f in range(frames + 1):
        at = first + f * pack_fps / 30.0
        scene.frame_set(int(at), subframe=at - int(at))
        world = {}
        for n in order:
            p = parent[n]
            carried = world[p] @ rest_t[p].inverted() @ rest_t[n] if p else rest_t[n]      # where it is if it does not turn by itself
            if n in PAIR:
                delta = flip @ turn(src_w @ src.pose.bones[PAIR[n]].matrix) @ rest_s[PAIR[n]].inverted() @ flip.inverted()
                world[n] = delta @ line_up.get(n, Quaternion()) @ rest_t[n]
            else:
                world[n] = carried
            # a pose bone's rotation: the turn, in the bone's own frame, from where its parent carries it to where it is
            q = carried.inverted() @ world[n]
            if n in before and before[n].dot(q) < 0:
                q.negate()
            before[n] = q
            if n != "Root":
                tracks.setdefault((n, "rotation_quaternion"), []).append(tuple(q))
        hips = src_w @ src.pose.bones[PAIR["pelvis"]].head
        moved = flip @ (hips - hips_rest) * legs
        if name.split("_")[0] in ("Walk", "Run", "Sprint", "Sneak"):
            moved.x = moved.y = 0.0                                              # a loop stays on its spot
        tracks.setdefault(("pelvis", "location"), []).append(tuple(pelvis_rest.inverted() @ (to_rig @ moved)))
    old = bpy.data.actions.get("NH_" + name)
    if old:
        bpy.data.actions.remove(old)
    out = bpy.data.actions.new("NH_" + name)
    out.use_fake_user = True
    rig.animation_data_create()
    rig.animation_data.action = out
    for (n, prop), rows in tracks.items():
        for i in range(len(rows[0])):
            fc = out.fcurve_ensure_for_datablock(rig, f'pose.bones["{n}"].{prop}', index=i)
            fc.keyframe_points.add(len(rows))
            flat = []
            for f, row in enumerate(rows):
                flat += [f, row[i]]
            fc.keyframe_points.foreach_set("co", flat)
            fc.keyframe_points.foreach_set("interpolation", [1] * len(rows))
            fc.update()
    out.frame_range = (0, frames)
    out.use_frame_range = True
    if getattr(out, "slots", None):
        rig.animation_data.action_slot = out.slots[0]
    scene.frame_start, scene.frame_end = 0, frames
    # the armature is in centimetres: it is exported at scale 1, as the body was (left at 0.01 the clip shrinks the body to a speck)
    was, size = scene.unit_settings.scale_length, tuple(rig.scale)
    scene.unit_settings.scale_length = 0.01
    rig.scale = (1.0, 1.0, 1.0)
    for o in bpy.context.view_layer.objects:
        o.select_set(o is rig)
    bpy.context.view_layer.objects.active = rig
    path = os.path.join(OUT, f"Naija_Anim_{name}.fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE"}, add_leaf_bones=False, bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0, apply_scale_options="FBX_SCALE_NONE")
    scene.unit_settings.scale_length = was
    rig.scale = size
    made.append((name, frames))
print("RETARGET: made", made)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "Retargeted_Pack.blend"))
