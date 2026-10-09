"""Makes the NAIJA HUSTLE player's action animations in Blender and exports one FBX per clip for Unreal.

  blender -b --python build_player_action_anims.py -- <Naija.fbx> <out folder>

Run inside an open Blender instead, with the body already imported, by setting NH_ANIM_OUT to the out folder.

The clips, all in place at 30 frames a second on the body's own skeleton (MakeHuman game_engine, mannequin bone names):
  Crouch_Idle, Crouch_Walk, Stand_To_Crouch, Crouch_To_Stand
  Pistol_Aim, Pistol_Fire, Crouch_Pistol_Aim, Rifle_Aim, Rifle_Fire, Crouch_Rifle_Aim
  Machete_Idle, Machete_Slash, Machete_Backslash, Machete_Chop
  Drive_Idle, Drive_Left, Drive_Right, Drive_Reverse   (the hips stay at standing height: the game puts the seat under them;
                                                        Left and Right go from straight ahead to 75 degrees of wheel)

Poses are written by hand in the character's own space, in centimetres, V(right, forward, up): where the hips are, how
the spine and head bend, and where the hands and feet go. A small solver turns each pose into bone rotations, and every
frame is keyed. The machete, pistol, rifle, car seat and steering wheel are stand-ins for looking at the clips in
Blender and are not exported. Writes <out>/Naija_Anim_<Clip>.fbx and <out>/Naija_Actions.blend.
"""
import bpy, bmesh, math, os, sys, time
from mathutils import Vector, Matrix, Quaternion

SOURCE = OUT = None
if "--" in sys.argv:
    SOURCE, OUT = sys.argv[sys.argv.index("--") + 1:][:2]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SOURCE, automatic_bone_orientation=False, ignore_leaf_bones=False)   # the leaf bones are real: head, toes, fingertips
    bpy.context.scene.render.fps = 30
OUT = OUT or os.environ.get("NH_ANIM_OUT")


# ============================================================================================== the pose solver
rig = bpy.data.objects["Armature"]
REST = {b.name: b.matrix_local.copy() for b in rig.data.bones}
PARENT = {b.name: (b.parent.name if b.parent else None) for b in rig.data.bones}
ORDER = [b.name for b in rig.data.bones]
FINGERS = ("index", "middle", "ring", "pinky")


def V(right, forward, up):
    """Character space to armature space: he faces -Y and his right hand is at -X."""
    return Vector((-right, -forward, up))


def Rx(d): return Matrix.Rotation(math.radians(d), 3, "X")
def Ry(d): return Matrix.Rotation(math.radians(d), 3, "Y")
def Rz(d): return Matrix.Rotation(math.radians(d), 3, "Z")


def turn(pitch=0.0, yaw=0.0, roll=0.0):
    """Lean forward, turn left, lean right, in degrees."""
    return Rz(yaw) @ Rx(pitch) @ Ry(-roll)


def frame(y, h):
    """A rotation with its Y along y and its X along h, as near as it can be."""
    y = y.normalized()
    x = (h - y * h.dot(y)).normalized()
    m = Matrix((x, y, x.cross(y)))
    m.transpose()
    return m


def hand_shape(side):
    """Where the palm really faces, read from the finger bones, in the hand bone's own space: f along the hand to the
    knuckles, t along the knuckles toward the thumb side, p out of the palm, and the middle of a closed fist."""
    inv = REST["hand_" + side].inverted()
    index, pinky = inv @ REST[f"index_01_{side}"].translation, inv @ REST[f"pinky_01_{side}"].translation
    f = Vector((0, 1, 0))
    t = index - pinky
    t.y = 0
    t.normalize()
    p = t.cross(f)
    thumb = inv @ rig.data.bones[f"thumb_03_{side}"].tail_local
    if p.dot(thumb) < 0:
        p = -p
    mid = (index + pinky) / 2
    fist = Vector((mid.x, 8.5, mid.z)) + p * 2.6      # a handle lies in the fold of the fingers, just off the palm
    return {"f": f, "p": p, "t": t, "fist": (fist.dot(f), fist.dot(p), fist.dot(t)), "fist_local": fist}


HAND = {side: hand_shape(side) for side in "rl"}


def hand_axes(side, fwd, palm):
    f = fwd.normalized()
    p = (palm - f * palm.dot(f)).normalized()
    return f, p, (f.cross(p) if side == "r" else p.cross(f))


def fist_of(side, wrist, fwd, palm):
    """The middle of the closed fist for a wrist there, knuckles toward fwd, palm facing palm."""
    f, p, t = hand_axes(side, fwd, palm)
    a, b, c = HAND[side]["fist"]
    return wrist + f * a + p * b + t * c


def wrist_for(side, fist, fwd, palm):
    """Where the wrist goes for the fist to close round that point."""
    f, p, t = hand_axes(side, fwd, palm)
    a, b, c = HAND[side]["fist"]
    return fist - f * a - p * b - t * c


class Skeleton:
    def __init__(self):
        self.basis = {n: Matrix.Identity(4) for n in ORDER}

    def carried(self, n):
        """Where the bone sits with its own pose cleared."""
        p = PARENT[n]
        return REST[n].copy() if p is None else self.world(p) @ REST[p].inverted() @ REST[n]

    def world(self, n):
        return self.carried(n) @ self.basis[n]

    def at(self, n):
        return self.world(n).translation

    def set_rot(self, n, rot):
        self.basis[n] = (self.carried(n).to_3x3().inverted() @ rot).to_4x4()

    def set_world(self, n, m):
        self.basis[n] = self.carried(n).inverted() @ m

    def bend(self, n, pitch=0.0, yaw=0.0, roll=0.0):
        r = REST[n].to_3x3()
        self.basis[n] = (r.inverted() @ turn(pitch, yaw, roll) @ r).to_4x4()

    def spin(self, n, axis, degrees):
        """Turns the bone about an armature-space axis from where its parents carry it."""
        self.set_rot(n, Matrix.Rotation(math.radians(degrees), 3, axis) @ self.carried(n).to_3x3())

    def reach(self, upper, lower, end, target, pole, hinge=None):
        """Two-bone IK: the end bone's head goes to target, the middle joint bends toward pole."""
        r0, r1, r2 = REST[upper].translation, REST[lower].translation, REST[end].translation
        a, b = (r1 - r0).length, (r2 - r1).length
        rest_u, rest_l = (r1 - r0).normalized(), (r2 - r1).normalized()
        hinge = hinge or rest_u.cross(rest_l)
        p0 = self.at(upper)
        d = target - p0
        length = max(min(d.length, (a + b) * 0.998), abs(a - b) + 0.5)
        dn = d.normalized()
        x = (a * a - b * b + length * length) / (2 * length)
        h = math.sqrt(max(a * a - x * x, 0.0))
        pn = (pole - dn * pole.dot(dn)).normalized()
        elbow = p0 + dn * x + pn * h
        du, dl = (elbow - p0).normalized(), (p0 + dn * length - elbow).normalized()
        new_hinge = du.cross(dl)
        self.set_rot(upper, frame(du, new_hinge) @ frame(rest_u, hinge).inverted() @ REST[upper].to_3x3())
        self.set_rot(lower, frame(dl, new_hinge) @ frame(rest_l, hinge).inverted() @ REST[lower].to_3x3())

    def aim(self, n, direction, amount=1.0):
        """Points the bone along direction by the shortest turn from where its parents carry it."""
        c = self.carried(n).to_3x3()
        turn_to = Quaternion().slerp(c.col[1].rotation_difference(direction.normalized()), amount)
        self.set_rot(n, turn_to.to_matrix() @ c)

    def hand(self, side, forward, palm, grip, index=None):
        n = "hand_" + side
        shape = HAND[side]
        r = REST[n].to_3x3()
        self.set_rot(n, frame(forward, palm) @ frame(r @ shape["f"], r @ shape["p"]).inverted() @ r)
        w = self.world(n).to_3x3()
        f, p, t = w @ shape["f"], w @ shape["p"], w @ shape["t"]
        axis = f.cross(p)                 # turning about this curls a finger toward the palm
        for finger, more in (("index", 0.94), ("middle", 1.0), ("ring", 1.04), ("pinky", 1.08)):
            amount = index if (finger == "index" and index is not None) else grip * more
            # straighten the splay of the fingers as they close, so they lie side by side round a handle
            first = f"{finger}_01_{side}"
            self.aim(first, f, min(amount, 1.0) * 0.8)
            c = self.world(first).to_3x3()
            self.set_rot(first, Matrix.Rotation(math.radians(amount * 78.0), 3, axis) @ c)
            for joint, most in (("02", 92.0), ("03", 58.0)):
                self.spin(f"{finger}_{joint}_{side}", axis, amount * most)
        # the thumb comes across the palm and closes over the fingers from the other side
        g = min(grip, 1.0)
        self.aim(f"thumb_01_{side}", f * 0.62 + p * 0.72 + t * 0.3, g * 0.9)
        self.aim(f"thumb_02_{side}", f * 0.74 + p * 0.62 - t * 0.25, g * 0.95)
        self.aim(f"thumb_03_{side}", f * 0.62 + p * 0.3 - t * 0.72, g * 0.95)


BALL_REACH = (REST["ball_r"].translation - REST["foot_r"].translation).length


def solve(p):
    s = Skeleton()
    m = (turn(*p["hip_rot"]) @ REST["pelvis"].to_3x3()).to_4x4()
    m.translation = p["hip"]
    s.set_world("pelvis", m)
    for bone, share in (("spine_01", 0.3), ("spine_02", 0.3), ("spine_03", 0.4)):
        s.bend(bone, *[v * share for v in p["spine"]])
    for bone, share in (("neck_01", 0.4), ("head", 0.6)):
        s.bend(bone, *[v * share for v in p["head"]])
    for side, sign in (("r", 1.0), ("l", -1.0)):
        fwd, up = p["cl_" + side]
        s.bend("clavicle_" + side, yaw=fwd * sign, roll=-up * sign)
        s.reach("upperarm_" + side, "lowerarm_" + side, "hand_" + side, p["hand_" + side], p["elb_" + side])
        s.hand(side, p[side + "_fwd"], p[side + "_palm"], p["grip_" + side], p.get("idx_" + side))
        pitch, yaw, roll = p["footrot_" + side]
        ankle = p["foot_" + side].copy()
        if pitch > 0:   # a raised heel turns about the ball of the foot, so the toes stay on the ground
            rise = math.radians(min(pitch, 80.0))
            ankle += Vector((0, 0, BALL_REACH * math.sin(rise))) + Rz(yaw) @ Vector((0, -BALL_REACH * (1 - math.cos(rise)), 0))
        s.reach("thigh_" + side, "calf_" + side, "foot_" + side, ankle, p["knee_" + side], hinge=Vector((1, 0, 0)))
        s.set_rot("foot_" + side, turn(pitch, yaw, roll) @ REST["foot_" + side].to_3x3())
        s.set_rot("ball_" + side, turn(pitch - p["toe_" + side], yaw, roll) @ REST["ball_" + side].to_3x3())
    # the IK bones the mannequin's animation Blueprint reads sit on the feet and hands
    for ik, bone in (("ik_foot_l", "foot_l"), ("ik_foot_r", "foot_r"), ("ik_hand_gun", "hand_r"), ("ik_hand_l", "hand_l"), ("ik_hand_r", "hand_r")):
        s.set_world(ik, s.world(bone) @ REST[bone].inverted() @ REST[ik])
    return s


def apply(s, key_frame=None):
    for n in ORDER:
        pb = rig.pose.bones[n]
        pb.rotation_mode = "QUATERNION"
        loc, rot, _ = s.basis[n].decompose()
        pb.location, pb.rotation_quaternion, pb.scale = loc, rot, (1, 1, 1)


def blend(a, b, t):
    out = {}
    for k in a:
        x, y = a[k], b[k]
        if x is None or y is None:
            out[k] = x if t < 0.5 else y
        elif isinstance(x, (int, float)):
            out[k] = x + (y - x) * t
        elif isinstance(x, Vector):
            out[k] = x.lerp(y, t)
        else:
            out[k] = tuple(i + (j - i) * t for i, j in zip(x, y))
    return out


EASE = {
    "lin": lambda t: t,
    "io": lambda t: t * t * (3 - 2 * t),
    "in": lambda t: t * t,
    "out": lambda t: 1 - (1 - t) * (1 - t),
}


def flat(v):
    return tuple(v) if isinstance(v, (Vector, tuple)) else (v,)


def sample(keys, f):
    """keys: [(frame, pose, _)]. One smooth curve through every key, at rest only at the first and the last, so a
    swing keeps its speed through the poses in the middle instead of stopping at each one."""
    if f <= keys[0][0]:
        return keys[0][1]
    if f >= keys[-1][0]:
        return keys[-1][1]
    i = max(k for k in range(len(keys) - 1) if keys[k][0] <= f)
    (t0, p0, _), (t1, p1, _) = keys[i], keys[i + 1]
    h, u = t1 - t0, (f - t0) / (t1 - t0)
    out = {}
    for name in p0:
        a, b = p0[name], p1[name]
        if a is None or b is None:
            out[name] = a if u < 0.5 else b
            continue

        def slope(k):                      # the speed the curve passes key k at
            if k == 0 or k == len(keys) - 1 or keys[k - 1][1][name] is None or keys[k + 1][1][name] is None:
                return tuple(0.0 for _ in flat(keys[k][1][name]))
            before, after = flat(keys[k - 1][1][name]), flat(keys[k + 1][1][name])
            return tuple((y - x) / (keys[k + 1][0] - keys[k - 1][0]) for x, y in zip(before, after))
        m0, m1 = slope(i), slope(i + 1)
        u2, u3 = u * u, u * u * u
        got = tuple((2 * u3 - 3 * u2 + 1) * x + (u3 - 2 * u2 + u) * h * s0 + (-2 * u3 + 3 * u2) * y + (u3 - u2) * h * s1
                    for x, y, s0, s1 in zip(flat(a), flat(b), m0, m1))
        out[name] = Vector(got) if isinstance(a, Vector) else got if isinstance(a, tuple) else got[0]
    return out


def bake(name, frames, pose_at):
    """Writes one action with a key on every bone on every frame."""
    old = bpy.data.actions.get(name)
    if old:
        bpy.data.actions.remove(old)
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    if not rig.animation_data:
        rig.animation_data_create()
    rig.animation_data.action = act
    tracks = {}
    last = {}
    for f in range(frames + 1):
        s = solve(pose_at(f))
        for n in ORDER:
            if n == "Root":
                continue
            loc, rot, _ = s.basis[n].decompose()
            if n in last and last[n].dot(rot) < 0:
                rot.negate()
            last[n] = rot
            tracks.setdefault((n, "rotation_quaternion"), []).append(tuple(rot))
            if n == "pelvis" or n.startswith("ik_"):
                tracks.setdefault((n, "location"), []).append(tuple(loc))
    for (n, prop), rows in tracks.items():
        path = f'pose.bones["{n}"].{prop}'
        for i in range(len(rows[0])):
            fc = act.fcurve_ensure_for_datablock(rig, path, index=i)
            fc.keyframe_points.add(len(rows))
            flat = []
            for f, row in enumerate(rows):
                flat += [f, row[i]]
            fc.keyframe_points.foreach_set("co", flat)
            fc.keyframe_points.foreach_set("interpolation", [1] * len(rows))  # linear: every frame is keyed
            fc.update()
    act.frame_range = (0, frames)
    act.use_frame_range = True
    return act


# ==================================================================================================== the clips
FPS = 30


def P(base, **over):
    return dict(base, **over)


def wave(f, n, phase=0.0):
    return math.sin(2 * math.pi * (f / n + phase))


def add(v, right=0.0, forward=0.0, up=0.0):
    return v + V(right, forward, up)


def unit(v):
    return v.normalized()


STAND = dict(
    hip=V(0, 0.7, 96.5), hip_rot=(0, 0, 0), spine=(0, 0, 0), head=(0, 0, 0), cl_r=(0, 0), cl_l=(0, 0),
    hand_r=V(27, 6, 92), elb_r=V(0.3, -1, 0), r_fwd=V(0, 0.3, -1), r_palm=V(-1, 0, 0), grip_r=0.25, idx_r=None,
    hand_l=V(-27, 6, 92), elb_l=V(-0.3, -1, 0), l_fwd=V(0, 0.3, -1), l_palm=V(1, 0, 0), grip_l=0.25, idx_l=None,
    foot_r=V(12, 1.8, 7.6), knee_r=V(0.15, 1, 0), footrot_r=(0, -8, 0), toe_r=0,
    foot_l=V(-12, 1.8, 7.6), knee_l=V(-0.15, 1, 0), footrot_l=(0, 8, 0), toe_l=0, steer=0.0)

# ------------------------------------------------------------------------------------------------------ crouching
CROUCH_LEGS = dict(
    hip=V(0, -4, 60), foot_l=V(-14, 14, 7.6), footrot_l=(0, 6, 0), knee_l=V(-0.3, 1, 0),
    foot_r=V(14, -18, 7.6), footrot_r=(38, -10, 0), toe_r=38, knee_r=V(0.3, 1, 0))
CROUCH = P(STAND, **CROUCH_LEGS, spine=(24, 0, 0), head=(-14, 0, 0),
           hand_l=V(-22, 26, 60), l_fwd=V(0.3, 1, -0.3), l_palm=V(0.3, 0, -1), elb_l=V(-1, -0.5, 0),
           hand_r=V(22, 12, 50), r_fwd=V(-0.3, 1, -0.4), r_palm=V(-0.3, 0, -1), elb_r=V(1, -0.5, 0))


def breathing(base, n, amount=1.0, hands=True):
    def at(f):
        w = wave(f, n)
        p = P(base, spine=(base["spine"][0] + 1.3 * amount * w, base["spine"][1], base["spine"][2]),
              hip=add(base["hip"], up=-0.5 * amount * w), head=(base["head"][0] - 0.8 * amount * w, base["head"][1], base["head"][2]))
        if hands:
            p["hand_r"] = add(base["hand_r"], up=0.5 * amount * w)
            p["hand_l"] = add(base["hand_l"], up=0.5 * amount * wave(f, n, 0.08))
        return p
    return at


def keyed(keys):
    return lambda f: sample(keys, f)


def crouch_walk(f, n=30, stride=20.0):
    phase = (f % n) / n
    p = P(CROUCH)

    def foot(ph, side):
        ph %= 1.0
        x = 13 * side
        if ph < 0.5:                       # on the ground, carried back under the body
            u = ph / 0.5
            return V(x, stride - 2 * stride * u, 7.6), (0.0 if u < 0.75 else 30 * (u - 0.75) / 0.25), 0.0
        u = (ph - 0.5) / 0.5               # in the air, swung forward
        e = u * u * (3 - 2 * u)
        return V(x, -stride + 2 * stride * e, 7.6 + 11 * math.sin(math.pi * u)), 30 * (1 - e) - 14 * math.sin(math.pi * u) * u, u
    (pr, pitch_r, _), (pl, pitch_l, _) = foot(phase, 1), foot(phase + 0.5, -1)
    p.update(foot_r=pr, footrot_r=(pitch_r, -8, 0), toe_r=max(pitch_r, 0), foot_l=pl, footrot_l=(pitch_l, 8, 0), toe_l=max(pitch_l, 0))
    sway = math.sin(2 * math.pi * phase)   # +1 as the left foot passes under
    p["hip"] = V(-2.5 * math.cos(2 * math.pi * phase), -2, 63 - 2.0 * math.cos(4 * math.pi * phase))
    p["hip_rot"] = (0, 6 * math.cos(2 * math.pi * phase), 0)
    p["spine"] = (26, -9 * math.cos(2 * math.pi * phase), 0)
    p["head"] = (-16, 3 * math.cos(2 * math.pi * phase), 0)
    swing = math.cos(2 * math.pi * phase)
    p["hand_r"] = V(24, 14 - 10 * swing, 58)
    p["hand_l"] = V(-24, 14 + 10 * swing, 58)
    p["r_fwd"], p["l_fwd"] = V(-0.2, 1, -0.5), V(0.2, 1, -0.5)
    p["r_palm"], p["l_palm"] = V(-1, 0, -0.3), V(1, 0, -0.3)
    p["elb_r"], p["elb_l"] = V(1, -1, 0.2), V(-1, -1, 0.2)
    return p


def stand_to_crouch(f, n=14):
    t = min(f / n, 1.0)
    e = t * t * (3 - 2 * t)
    p = blend(STAND, CROUCH, e)
    lift = math.sin(math.pi * t)
    p["foot_r"] = add(p["foot_r"], up=5 * lift)     # the right foot steps back, the left forward
    p["foot_l"] = add(p["foot_l"], up=3 * math.sin(math.pi * min(t * 1.4, 1.0)))
    return p


# ------------------------------------------------------------------------------------------------------- shooting
GUN_PALM = V(-1, 0, 0)   # the right palm faces left round a grip


def gun_frame(wrist, fwd):
    """The fist, and the gun's barrel, left and up directions, from the right wrist and where the barrel points."""
    f, left, up = hand_axes("r", fwd, GUN_PALM)
    return fist_of("r", wrist, f, left), f, left, up


def left_on(point, knuckles, palm, grip, elbow):
    """The left hand closed round a point: which way its knuckles and palm face, and how far it closes."""
    return dict(hand_l=wrist_for("l", point, knuckles, palm), l_fwd=unit(knuckles), l_palm=unit(palm), grip_l=grip, elb_l=elbow)


def pistol(base, wrist, fwd, **over):
    fist, f, left, up = gun_frame(wrist, fwd)
    # the left hand wraps the right hand's fingers from the left side, knuckles under the trigger guard
    p = P(base, hand_r=wrist, r_fwd=f, r_palm=left, grip_r=0.95, idx_r=0.35, elb_r=V(0.7, -0.2, -1),
          **left_on(fist + f * 2.2 - up * 2.6 + left * 1.6, f * 0.78 + up * 0.62, -left + f * 0.25, 0.62, V(-0.7, -0.2, -1)))
    p.update(over)
    return p


def rifle(base, wrist, fwd, **over):
    fist, f, left, up = gun_frame(wrist, fwd)
    # the left hand cups the handguard from below, fingers up its far side
    p = P(base, hand_r=wrist, r_fwd=f, r_palm=left, grip_r=0.95, idx_r=0.35, elb_r=V(1, -0.5, -0.6),
          **left_on(fist + f * 29 + up * 2.4, -left * 0.82 + f * 0.5 + up * 0.25, up + left * 0.15, 0.8, V(-0.35, 0, -1)))
    p.update(over)
    return p


PISTOL_BODY = P(STAND, hip=V(0, -3, 93), hip_rot=(0, -12, 0), spine=(13, -4, 0), head=(-2, 15, 0), cl_r=(14, 6), cl_l=(16, 6),
                foot_l=V(-15, 10, 7.6), footrot_l=(0, 2, 0), foot_r=V(15, -10, 7.6), footrot_r=(0, -28, 0))
PISTOL_WRIST, PISTOL_FWD = V(4, 45, 156), V(-0.05, 1, 0.0)
PISTOL_AIM = pistol(PISTOL_BODY, PISTOL_WRIST, PISTOL_FWD)
PISTOL_KICK = pistol(P(PISTOL_BODY, spine=(10.5, -4, 0), head=(-3.5, 15, 0)), add(PISTOL_WRIST, forward=-3.5, up=4.0), V(-0.05, 1, 0.3), idx_r=0.6)

RIFLE_BODY = P(STAND, hip=V(1, -4, 92), hip_rot=(0, -32, 0), spine=(16, -8, 4), head=(4, 36, 9), cl_r=(8, 9), cl_l=(26, 4),
               foot_l=V(-10, 20, 7.6), footrot_l=(0, -5, 0), foot_r=V(16, -16, 7.6), footrot_r=(0, -50, 0))
RIFLE_WRIST, RIFLE_FWD = V(12, 21, 144), V(-0.07, 1, 0.0)
RIFLE_AIM = rifle(RIFLE_BODY, RIFLE_WRIST, RIFLE_FWD)
RIFLE_KICK = rifle(P(RIFLE_BODY, spine=(14.6, -9, 4), hip=V(1, -4.6, 92)), add(RIFLE_WRIST, forward=-2.2, up=0.7), V(-0.07, 1, 0.035), idx_r=0.6)

CROUCH_GUN_BODY = P(STAND, **CROUCH_LEGS, hip_rot=(0, -14, 0), spine=(16, -4, 0), head=(-4, 16, 0), cl_r=(14, 6), cl_l=(16, 6))
CROUCH_PISTOL_AIM = pistol(CROUCH_GUN_BODY, V(4, 44, 118), PISTOL_FWD)
CROUCH_RIFLE_BODY = P(STAND, **CROUCH_LEGS, hip_rot=(0, -30, 0), spine=(16, -8, 4), head=(2, 36, 9), cl_r=(8, 9), cl_l=(26, 4))
CROUCH_RIFLE_AIM = rifle(CROUCH_RIFLE_BODY, V(12, 22, 107), RIFLE_FWD)

# -------------------------------------------------------------------------------------------------------- machete
MACHETE_BODY = P(STAND, hip=V(0, 0, 87), hip_rot=(0, -22, 0), spine=(12, -6, 0), head=(0, 26, 0),
                 foot_l=V(-13, 26, 7.6), footrot_l=(0, -4, 0), foot_r=V(17, -20, 7.6), footrot_r=(0, -45, 0), cl_r=(0, 4), cl_l=(10, 4))


def machete(base, wrist, blade, edge, **over):
    """The right hand holds the machete: blade is where it points, edge the way its cutting edge faces."""
    b, e = unit(blade), unit(edge)
    p = P(base, hand_r=wrist, r_fwd=e, r_palm=b.cross(e), grip_r=0.98, idx_r=None)
    p.update(over)
    return p


LEFT_GUARD = dict(hand_l=V(-20, 30, 118), l_fwd=V(0.25, 1, 0.5), l_palm=V(0.6, 0.3, -0.7), elb_l=V(-1, -0.4, -0.6), grip_l=0.35)
MACHETE_GUARD = machete(P(MACHETE_BODY, **LEFT_GUARD), V(24, 20, 116), V(-0.2, 0.25, 1), V(-0.2, 1, -0.2), elb_r=V(1, -0.6, -0.5))

MACHETE_SLASH = [  # a flat cut from the right shoulder across to the left hip
    (0, MACHETE_GUARD, "io"),
    (7, machete(P(MACHETE_BODY, hip_rot=(0, -40, 0), spine=(4, -24, 0), head=(0, 52, 0), hip=V(3, -4, 88), hand_l=V(-14, 36, 122), l_fwd=V(0.3, 1, 0.3), l_palm=V(0.5, 0, -1), elb_l=V(-1, -0.4, -0.6), grip_l=0.3),
                V(34, -6, 152), V(0.55, -0.75, 0.4), V(0.5, 0.6, 0.6), elb_r=V(1, -0.6, -0.2)), "io"),
    (10, machete(P(MACHETE_BODY, hip_rot=(0, -18, 0), spine=(12, -6, 0), head=(2, 22, 0), hip=V(0, 6, 85), hand_l=V(-28, 10, 104), l_fwd=V(0, 1, 0), l_palm=V(1, 0, -0.5), elb_l=V(-0.5, -1, 0.1), grip_l=0.6),
                 V(40, 30, 140), V(0.8, 0.6, 0.1), V(-0.6, 0.8, -0.1), elb_r=V(1, -0.6, -0.3)), "in"),
    (12, machete(P(MACHETE_BODY, hip_rot=(0, 2, 0), spine=(16, 14, 0), head=(4, -12, 0), hip=V(-2, 11, 82), hand_l=V(-30, 0, 100), l_fwd=V(0, 0.6, -0.6), l_palm=V(1, 0, 0), elb_l=V(-0.5, -1, 0.1), grip_l=0.7),
                 V(6, 50, 126), V(-0.25, 1, 0.0), V(-1, -0.25, -0.15), elb_r=V(0.8, -0.3, -0.7)), "lin"),
    (15, machete(P(MACHETE_BODY, hip_rot=(0, 14, 0), spine=(18, 26, 0), head=(4, -34, 0), hip=V(-3, 10, 82), hand_l=V(-30, -4, 98), l_fwd=V(0, 0.5, -0.7), l_palm=V(1, 0, 0), elb_l=V(-0.5, -1, 0.1), grip_l=0.7),
                 V(-28, 26, 108), V(-0.9, 0.1, -0.2), V(-0.2, -0.9, -0.3), elb_r=V(0.3, 0.2, -1)), "out"),
    (21, machete(P(MACHETE_BODY, hip_rot=(0, -4, 0), spine=(14, 8, 0), head=(2, -4, 0), hip=V(-1, 4, 85), **LEFT_GUARD),
                 V(2, 34, 112), V(-0.6, 0.45, 0.65), V(0.2, 0.9, -0.4), elb_r=V(0.8, -0.3, -0.7)), "io"),
    (28, MACHETE_GUARD, "io"),
]

MACHETE_BACKSLASH = [  # the return cut, from the left hip up across to the right
    (0, MACHETE_GUARD, "io"),
    (7, machete(P(MACHETE_BODY, hip_rot=(0, 10, 0), spine=(14, 24, 0), head=(4, -30, 0), hip=V(-3, 4, 84), hand_l=V(-28, 4, 104), l_fwd=V(0, 0.8, -0.5), l_palm=V(1, 0, 0), elb_l=V(-0.5, -1, 0.1), grip_l=0.6),
                V(-24, 22, 112), V(-0.85, -0.3, 0.3), V(0.3, 0.6, 0.6), elb_r=V(0.3, 0.2, -1)), "io"),
    (10, machete(P(MACHETE_BODY, hip_rot=(0, -8, 0), spine=(12, 6, 0), head=(3, 2, 0), hip=V(0, 9, 83), hand_l=V(-28, 10, 106), l_fwd=V(0.2, 1, 0), l_palm=V(1, 0, -0.5), elb_l=V(-0.5, -1, 0.1), grip_l=0.6),
                 V(4, 50, 124), V(-0.3, 0.95, 0.1), V(0.95, 0.3, 0.2), elb_r=V(0.8, -0.3, -0.7)), "in"),
    (12, machete(P(MACHETE_BODY, hip_rot=(0, -30, 0), spine=(8, -18, 0), head=(2, 44, 0), hip=V(2, 7, 84), hand_l=V(-22, 28, 116), l_fwd=V(0.3, 1, 0.2), l_palm=V(0.6, 0, -0.8), elb_l=V(-1, -0.6, -0.4), grip_l=0.5),
                 V(44, 26, 138), V(0.75, 0.6, 0.25), V(0.6, -0.75, 0.2), elb_r=V(1, -0.6, -0.3)), "lin"),
    (15, machete(P(MACHETE_BODY, hip_rot=(0, -40, 0), spine=(6, -26, 0), head=(0, 58, 0), hip=V(3, 3, 86), hand_l=V(-18, 32, 118), l_fwd=V(0.3, 1, 0.3), l_palm=V(0.6, 0, -0.8), elb_l=V(-1, -0.5, -0.5), grip_l=0.4),
                 V(42, -4, 150), V(0.6, -0.5, 0.6), V(0.3, -0.7, -0.6), elb_r=V(1, -0.6, -0.2)), "out"),
    (26, MACHETE_GUARD, "io"),
]

MACHETE_CHOP = [  # an overhead chop straight down in front
    (0, MACHETE_GUARD, "io"),
    (7, machete(P(MACHETE_BODY, hip=V(0, -4, 90), spine=(-6, -10, 0), head=(-4, 28, 0), cl_r=(-4, 22), hand_l=V(-16, 34, 128), l_fwd=V(0.3, 1, 0.4), l_palm=V(0.5, 0.3, -0.8), elb_l=V(-1, -0.4, -0.6), grip_l=0.3),
                V(22, -2, 186), V(0.1, -0.85, 0.5), V(0, 0.5, 0.85), elb_r=V(1, -0.2, 0.3)), "io"),
    (10, machete(P(MACHETE_BODY, hip=V(0, 4, 86), spine=(14, -2, 0), head=(4, 22, 0), cl_r=(6, 14), hand_l=V(-26, 12, 112), l_fwd=V(0, 1, -0.2), l_palm=V(1, 0, -0.4), elb_l=V(-0.5, -1, 0.1), grip_l=0.6),
                 V(18, 40, 160), V(0, 0.75, 0.65), V(0, 0.65, -0.75), elb_r=V(1, -0.4, -0.5)), "in"),
    (12, machete(P(MACHETE_BODY, hip=V(0, 9, 80), spine=(30, 2, 0), head=(-4, 20, 0), cl_r=(10, 0), hand_l=V(-28, 2, 104), l_fwd=V(0, 0.6, -0.7), l_palm=V(1, 0, 0), elb_l=V(-0.5, -1, 0.1), grip_l=0.7),
                 V(12, 46, 102), V(-0.05, 0.9, -0.42), V(0, -0.42, -0.9), elb_r=V(1, -0.5, -0.4)), "lin"),
    (15, machete(P(MACHETE_BODY, hip=V(0, 9, 78), spine=(34, 4, 0), head=(-6, 20, 0), cl_r=(10, 0), hand_l=V(-28, 0, 102), l_fwd=V(0, 0.6, -0.7), l_palm=V(1, 0, 0), elb_l=V(-0.5, -1, 0.1), grip_l=0.7),
                 V(10, 40, 84), V(-0.05, 0.75, -0.66), V(0, -0.66, -0.75), elb_r=V(1, -0.5, -0.3)), "out"),
    (27, MACHETE_GUARD, "io"),
]

# -------------------------------------------------------------------------------------------------------- driving
# The hips stay where they are in the standing pose: the game puts the seat under them.
WHEEL_AT, WHEEL_RADIUS, WHEEL_TILT = V(0, 41, 121), 18.0, 25.0
WHEEL_AXIS = V(0, -math.cos(math.radians(WHEEL_TILT)), math.sin(math.radians(WHEEL_TILT)))   # out of the wheel toward the driver
WHEEL_UP, WHEEL_RIGHT = V(0, math.sin(math.radians(WHEEL_TILT)), math.cos(math.radians(WHEEL_TILT))), V(1, 0, 0)

SEATED = P(STAND, hip=V(0, 0.7, 98.2), hip_rot=(-14, 0, 0), spine=(8, 0, 0), head=(5, 0, 0), cl_r=(12, 2), cl_l=(12, 2),
           foot_r=V(13, 60, 51), knee_r=V(0.25, 0.4, 1), footrot_r=(-28, -4, 0), toe_r=-6,
           foot_l=V(-17, 55, 49), knee_l=V(-0.3, 0.4, 1), footrot_l=(-24, 6, 0), toe_l=-4)


def on_wheel(side, angle):
    """A hand closed round the rim, angle in degrees clockwise from the top as the driver sees it."""
    a = math.radians(angle)
    out = WHEEL_RIGHT * math.sin(a) + WHEEL_UP * math.cos(a)
    knuckles, palm = unit(-WHEEL_AXIS + out * 0.25), -out
    return wrist_for(side, WHEEL_AT + out * WHEEL_RADIUS, knuckles, palm), knuckles, palm


def drive(steer=0.0, right_at=62.0, left_at=-62.0, **over):
    """steer in degrees, positive to the right. A hand that would be carried past the bottom is kept from crossing."""
    p = P(SEATED, steer=steer)
    p["hand_r"], p["r_fwd"], p["r_palm"] = on_wheel("r", right_at + steer)
    p["hand_l"], p["l_fwd"], p["l_palm"] = on_wheel("l", left_at + steer)
    p.update(grip_r=0.92, grip_l=0.92, elb_r=V(1, -0.2, -1), elb_l=V(-1, -0.2, -1),
             head=(5, -steer * 0.22, 0), spine=(8, -steer * 0.06, steer * 0.04))
    p.update(over)
    return p


def drive_idle(f, n=90):
    p = drive(2.5 * wave(f, n) + 1.2 * wave(f, n / 3, 0.2))
    w = wave(f, n, 0.3)
    p["spine"] = (8 + 1.0 * w, p["spine"][1], p["spine"][2])
    p["head"] = (5 - 0.6 * w, p["head"][1] + 2.0 * wave(f, n, 0.6), 0)
    return p


def drive_turn(sign):
    return lambda f: drive(sign * 75.0 * EASE["io"](min(f / 15.0, 1.0)))


DRIVE_REVERSE = drive(0.0, left_at=-8.0, hip_rot=(-14, -12, 0), spine=(4, -34, 0), head=(0, -62, 0),
                      hand_r=V(46, -22, 128), r_fwd=V(0.7, -0.7, -0.1), r_palm=V(0, 0, -1), grip_r=0.3, elb_r=V(1, 0.4, -0.6), cl_r=(-14, 6))


def drive_reverse(f, n=16):
    return blend(drive(0.0), DRIVE_REVERSE, EASE["io"](min(f / n, 1.0)))


CLIPS = {
    # name: (frames, pose at frame, prop, loops)
    "Crouch_Idle": (60, breathing(CROUCH, 60), None, True),
    "Crouch_Walk": (30, crouch_walk, None, True),
    "Stand_To_Crouch": (14, stand_to_crouch, None, False),
    "Crouch_To_Stand": (14, lambda f: stand_to_crouch(14 - f), None, False),
    "Pistol_Aim": (60, breathing(PISTOL_AIM, 60, 0.6, hands=False), "pistol", True),
    "Pistol_Fire": (10, keyed([(0, PISTOL_AIM, "io"), (2, PISTOL_KICK, "out"), (10, PISTOL_AIM, "io")]), "pistol", False),
    "Crouch_Pistol_Aim": (60, breathing(CROUCH_PISTOL_AIM, 60, 0.6, hands=False), "pistol", True),
    "Rifle_Aim": (60, breathing(RIFLE_AIM, 60, 0.6, hands=False), "rifle", True),
    "Rifle_Fire": (6, keyed([(0, RIFLE_AIM, "io"), (1, RIFLE_KICK, "out"), (3, RIFLE_AIM, "io"), (4, RIFLE_KICK, "out"), (6, RIFLE_AIM, "io")]), "rifle", True),
    "Crouch_Rifle_Aim": (60, breathing(CROUCH_RIFLE_AIM, 60, 0.6, hands=False), "rifle", True),
    "Machete_Idle": (60, breathing(MACHETE_GUARD, 60, 1.0), "machete", True),
    "Machete_Slash": (28, keyed(MACHETE_SLASH), "machete", False),
    "Machete_Backslash": (26, keyed(MACHETE_BACKSLASH), "machete", False),
    "Machete_Chop": (27, keyed(MACHETE_CHOP), "machete", False),
    "Drive_Idle": (90, drive_idle, "car", True),
    "Drive_Left": (15, drive_turn(-1.0), "car", False),
    "Drive_Right": (15, drive_turn(1.0), "car", False),
    "Drive_Reverse": (16, drive_reverse, "car", False),
}


# ==================================================================================================== the props
def material(name, colour, rough=0.5, metal=0.0):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = (*colour, 1)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    mat.diffuse_color = (*colour, 1)
    return mat


STEEL, DARK, WOOD, SEAT = (material("NH_Steel", (0.6, 0.61, 0.63), 0.3, 0.9), material("NH_Dark", (0.03, 0.03, 0.035), 0.5, 0.5),
                           material("NH_Wood", (0.3, 0.15, 0.06), 0.7), material("NH_Seat", (0.12, 0.12, 0.14), 0.8))


def box(verts, faces, at, size, turn=None, mat=0):
    """Adds a box to the mesh being built: centre, size, and an optional rotation."""
    n = len(verts)
    for i in range(8):
        v = Vector(((i & 1) - 0.5, ((i >> 1) & 1) - 0.5, ((i >> 2) & 1) - 0.5))
        v = Vector((v.x * size[0], v.y * size[1], v.z * size[2]))
        if turn:
            v = turn @ v
        verts.append(tuple(v + Vector(at)))
    for q in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)):
        faces.append((tuple(n + i for i in q), mat))


def make(name, parts, mats):
    old = bpy.data.objects.get(name)
    if old:
        bpy.data.objects.remove(old)
    verts, faces = [], []
    for part in parts:
        box(verts, faces, *part)
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], [f for f, _ in faces])
    for m in mats:
        mesh.materials.append(m)
    for poly, (_, m) in zip(mesh.polygons, faces):
        poly.material_index = m
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def in_hand(obj):
    """Held in the right fist: the prop's Y runs along the knuckles (a barrel, a cutting edge), its Z out of the thumb side."""
    bone, shape = rig.data.bones["hand_r"], HAND["r"]
    obj.parent, obj.parent_type, obj.parent_bone = rig, "BONE", "hand_r"
    grip = Matrix((-shape["p"], shape["f"], shape["t"])).transposed().to_4x4()   # columns: X = -palm, Y = knuckles, Z = thumb side
    grip.translation = shape["fist_local"] - Vector((0, bone.length, 0))         # bone children hang from the bone's tail
    obj.matrix_parent_inverse = Matrix.Identity(4)
    obj.matrix_basis = grip
    return obj


RX = lambda d: Matrix.Rotation(math.radians(d), 3, "X")
machete_prop = in_hand(make("Prop_Machete", [((0, 0, 0), (2.6, 3.2, 13), None, 0), ((0, 1.0, 33), (0.5, 5.5, 52), None, 1),
                                             ((0, 0.2, 60.5), (0.5, 3.6, 4), RX(-28), 1)], [WOOD, STEEL]))
pistol_prop = in_hand(make("Prop_Pistol", [((0, -1.5, -3), (3.0, 4.5, 11), RX(-12), 0), ((0, 5.5, 3.6), (3.2, 19, 4.2), None, 0),
                                           ((0, 5.0, 0.6), (1.2, 6, 0.8), None, 0)], [DARK]))
rifle_prop = in_hand(make("Prop_Rifle", [((0, -1.5, -3), (3.0, 4, 10), RX(-15), 1), ((0, 8, 5), (4, 36, 6.5), None, 0),
                                         ((0, -23, 3.5), (3.5, 28, 6), RX(-7), 1), ((0, 16, -6), (3, 6, 19), RX(22), 0),
                                         ((0, 35, 5), (4.5, 19, 5.5), None, 1), ((0, 58, 6), (2, 30, 2), None, 0),
                                         ((0, 70, 8.5), (0.8, 1.5, 4), None, 0)], [DARK, WOOD]))

# the car: a seat under the hips and a steering wheel that turns with the hands
seat = make("Prop_Seat", [((0, 8, 84), (48, 50, 10), None, 0), ((0, 36, 118), (46, 10, 62), RX(-14), 0), ((0, 44, 158), (26, 9, 20), RX(-14), 0),
                          ((0, -64, 41), (60, 40, 4), RX(-28), 0)], [SEAT])
seat.parent = rig
old = bpy.data.objects.get("Prop_Wheel")
if old:
    bpy.data.objects.remove(old)
mesh = bpy.data.meshes.new("Prop_Wheel")
bm = bmesh.new()
ring = bmesh.ops.create_circle(bm, segments=10, radius=1.7)
bmesh.ops.translate(bm, verts=ring["verts"], vec=(WHEEL_RADIUS, 0, 0))
bmesh.ops.rotate(bm, verts=ring["verts"], matrix=Matrix.Rotation(math.radians(90), 3, "X"), cent=(WHEEL_RADIUS, 0, 0))
bmesh.ops.spin(bm, geom=bm.verts[:] + bm.edges[:], angle=math.tau, steps=32, axis=(0, 0, 1), cent=(0, 0, 0))
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.01)
for spoke in (90, 210, 330):
    made = bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Rotation(math.radians(spoke), 4, "Z") @ Matrix.Translation((WHEEL_RADIUS / 2, 0, 0)) @ Matrix.Diagonal((WHEEL_RADIUS, 3, 1.5, 1)))
bm.to_mesh(mesh)
bm.free()
mesh.materials.append(DARK)
wheel = bpy.data.objects.new("Prop_Wheel", mesh)
bpy.context.scene.collection.objects.link(wheel)
wheel.parent = rig
wheel.rotation_mode = "QUATERNION"
PROPS = {"machete": [machete_prop], "pistol": [pistol_prop], "rifle": [rifle_prop], "car": [seat, wheel]}


def wheel_turn(steer):
    """The wheel's rotation with its plane tilted to the driver and turned steer degrees to the right."""
    m = Matrix((WHEEL_RIGHT, WHEEL_UP, WHEEL_AXIS)).transposed()   # columns: right, up, toward the driver
    return (m @ Matrix.Rotation(math.radians(-steer), 3, "Z")).to_quaternion()


# =================================================================================================== the baking

started = time.time()
for name, (frames, pose_at, prop, loops) in CLIPS.items():
    act = bake("NH_" + name, frames, pose_at)
    act["nh_prop"], act["nh_loops"] = prop or "", loops
    if prop == "car":
        old = bpy.data.actions.get("NHW_" + name)
        if old:
            bpy.data.actions.remove(old)
        wact = bpy.data.actions.new("NHW_" + name)
        wact.use_fake_user = True
        if not wheel.animation_data:
            wheel.animation_data_create()
        wheel.animation_data.action = wact
        rows = [tuple(wheel_turn(pose_at(f)["steer"])) for f in range(frames + 1)]
        for i in range(4):
            fc = wact.fcurve_ensure_for_datablock(wheel, "rotation_quaternion", index=i)
            fc.keyframe_points.add(len(rows))
            flat = []
            for f, row in enumerate(rows):
                flat += [f, row[i]]
            fc.keyframe_points.foreach_set("co", flat)
            fc.update()
wheel.location = WHEEL_AT
print("NH baked", len(CLIPS), "clips in", round(time.time() - started, 1), "s")


def show(name, frame=0):
    """Puts a clip on the body with its prop in view."""
    act = bpy.data.actions["NH_" + name]
    rig.animation_data.action = act
    rig.animation_data.action_slot = act.slots[0]
    for key, objs in PROPS.items():
        for o in objs:
            o.hide_viewport = o.hide_render = key != act["nh_prop"]
    if act["nh_prop"] == "car":
        wact = bpy.data.actions["NHW_" + name]
        wheel.animation_data.action = wact
        wheel.animation_data.action_slot = wact.slots[0]
    scene = bpy.context.scene
    scene.frame_start, scene.frame_end = 0, int(act.frame_range[1])
    scene.frame_set(frame)


# =================================================================================================== the export


def export(folder, prefix="Naija_Anim_"):
    os.makedirs(folder, exist_ok=True)
    scene = bpy.context.scene
    was = (scene.unit_settings.scale_length, tuple(rig.scale), rig.hide_get())
    scene.unit_settings.scale_length = 0.01      # the armature is in centimetres: export it at scale 1, as the body was
    rig.scale = (1.0, 1.0, 1.0)
    rig.hide_set(False)
    for o in bpy.context.view_layer.objects:
        o.select_set(o is rig)
    bpy.context.view_layer.objects.active = rig
    made = []
    try:
        for name in CLIPS:
            show(name, 0)
            path = os.path.join(folder, f"{prefix}{name}.fbx")
            bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"ARMATURE"}, add_leaf_bones=False,
                                     bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                                     bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
                                     bake_anim_simplify_factor=0.0, apply_scale_options="FBX_SCALE_NONE")
            made.append((name, os.path.getsize(path) // 1000))
    finally:
        scene.unit_settings.scale_length = was[0]
        rig.scale = was[1]
        rig.hide_set(was[2])
    return made


if OUT:
    print("NH exported", export(OUT))
    show("Machete_Slash", 0)
    if bpy.app.background:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "Naija_Actions.blend"))
