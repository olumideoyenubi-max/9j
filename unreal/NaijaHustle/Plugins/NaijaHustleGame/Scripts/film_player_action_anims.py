"""Films the player's action animations in Blender: builds the clips (build_player_action_anims.py, which this runs
first), dresses a sunlit Lagos street round the body, and renders every clip with EEVEE into one labelled film.

  blender -b --python film_player_action_anims.py -- <Naija.fbx> <out folder> [clip ...]

Writes <out>/Naija_Action_Clips.mp4 (and the FBX files and Naija_Actions.blend the build makes), with the drawn frames
kept in <out>/film_frames so a second run only draws what is missing. About a second a frame on an M1.

The street, the taxi, the machete, the guns and the muzzle flash are made here from boxes and noise-textured
materials: they are for the film only and nothing of them is exported to the game.
"""
import bpy, bmesh, math, os, random, sys, time
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
exec(compile(open(os.path.join(HERE, "build_player_action_anims.py")).read(), "build_player_action_anims.py", "exec"))
ONLY = sys.argv[sys.argv.index("--") + 3:]
FRAMES = os.path.join(OUT, "film_frames")


# ============================================================================================ the street and light
scene = bpy.context.scene


def bsdf_of(mat):
    return next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")


# ------------------------------------------------------------------------------------------------- the body
def fix_body():
    """The FBX import leaves the textures wired to transparency only: put them on the colour, and keep
    transparency for the hair cards alone."""
    see_through = ("eyebrow", "eyelash", "beard")
    rough = {"body": 0.52, "tankshirt": 0.85, "cargo": 0.8, "short01": 0.6, "teeth": 0.3, "low-poly": 0.15}
    for mat in bpy.data.objects["Naija"].data.materials:
        tree, bsdf = mat.node_tree, bsdf_of(mat)
        image = next((n for n in tree.nodes if n.type == "TEX_IMAGE" and n.image), None)
        if image and not bsdf.inputs["Base Color"].is_linked:
            tree.links.new(image.outputs["Color"], bsdf.inputs["Base Color"])
        if not any(k in mat.name for k in see_through):
            for link in list(bsdf.inputs["Alpha"].links):
                tree.links.remove(link)
            bsdf.inputs["Alpha"].default_value = 1.0
        for key, value in rough.items():
            if key in mat.name:
                bsdf.inputs["Roughness"].default_value = value


# ------------------------------------------------------------------------------------------------ materials
def surface(name, colour_a, colour_b, scale=6.0, rough=0.85, bump=0.25, metal=0.0, detail=8.0):
    """A weathered surface: two colours mottled by noise, with the same noise raising it."""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    tree = mat.node_tree
    for n in [n for n in tree.nodes if n.type not in ("BSDF_PRINCIPLED", "OUTPUT_MATERIAL")]:
        tree.nodes.remove(n)
    bsdf = bsdf_of(mat)
    coord = tree.nodes.new("ShaderNodeTexCoord")
    noise = tree.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value, noise.inputs["Detail"].default_value, noise.inputs["Roughness"].default_value = scale, detail, 0.65
    ramp = tree.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position, ramp.color_ramp.elements[1].position = 0.35, 0.7
    ramp.color_ramp.elements[0].color, ramp.color_ramp.elements[1].color = (*colour_a, 1), (*colour_b, 1)
    fine = tree.nodes.new("ShaderNodeTexNoise")
    fine.inputs["Scale"].default_value, fine.inputs["Detail"].default_value = scale * 14, 4.0
    bumps = tree.nodes.new("ShaderNodeBump")
    bumps.inputs["Strength"].default_value = bump
    tree.links.new(coord.outputs["Object"], noise.inputs["Vector"])
    tree.links.new(coord.outputs["Object"], fine.inputs["Vector"])
    tree.links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    tree.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    tree.links.new(fine.outputs["Fac"], bumps.inputs["Height"])
    tree.links.new(bumps.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Roughness"].default_value, bsdf.inputs["Metallic"].default_value = rough, metal
    mat.diffuse_color = (*[(a + b) / 2 for a, b in zip(colour_a, colour_b)], 1)
    return mat


def stage_materials():
    return {
        "sand": surface("NHS_Sand", (0.27, 0.2, 0.14), (0.44, 0.35, 0.26), 1.6, 0.95, 0.5),
        "concrete": surface("NHS_Concrete", (0.36, 0.34, 0.31), (0.58, 0.55, 0.5), 2.2, 0.9, 0.3),
        "cream": surface("NHS_Cream", (0.55, 0.47, 0.32), (0.74, 0.66, 0.48), 1.8, 0.9, 0.25),
        "green": surface("NHS_Green", (0.16, 0.3, 0.24), (0.27, 0.42, 0.33), 1.8, 0.9, 0.25),
        "ochre": surface("NHS_Ochre", (0.5, 0.3, 0.12), (0.68, 0.45, 0.2), 1.8, 0.9, 0.25),
        "zinc": surface("NHS_Zinc", (0.27, 0.17, 0.11), (0.5, 0.5, 0.5), 3.0, 0.55, 0.4, metal=0.7),
        "dark": surface("NHS_Dark", (0.015, 0.015, 0.02), (0.05, 0.045, 0.04), 4.0, 0.6, 0.1),
        "wood": surface("NHS_Wood", (0.14, 0.08, 0.04), (0.3, 0.19, 0.1), 5.0, 0.8, 0.4),
        "tarp": surface("NHS_Tarp", (0.03, 0.13, 0.45), (0.06, 0.22, 0.6), 3.0, 0.6, 0.3),
        "yellow": surface("NHS_Yellow", (0.72, 0.5, 0.03), (0.85, 0.62, 0.06), 2.0, 0.5, 0.1),
        "red": surface("NHS_Red", (0.45, 0.04, 0.03), (0.62, 0.08, 0.05), 3.0, 0.6, 0.2),
        "leaf": surface("NHS_Leaf", (0.05, 0.16, 0.03), (0.14, 0.3, 0.06), 9.0, 0.7, 0.4),
    }


# ---------------------------------------------------------------------------------------------- the street
class Builder:
    """Collects boxes by material and makes one mesh object of them."""

    def __init__(self, name, mats):
        self.name, self.mats, self.verts, self.faces, self.used = name, mats, [], [], []

    def box(self, at, size, mat, turn=None):
        if mat not in self.used:
            self.used.append(mat)
        n = len(self.verts)
        for i in range(8):
            v = Vector((((i & 1) - 0.5) * size[0], (((i >> 1) & 1) - 0.5) * size[1], (((i >> 2) & 1) - 0.5) * size[2]))
            self.verts.append(tuple((turn @ v if turn else v) + Vector(at)))
        for q in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)):
            self.faces.append((tuple(n + i for i in q), self.used.index(mat)))

    def finish(self, collection):
        old = bpy.data.objects.get(self.name)
        if old:
            bpy.data.objects.remove(old)
        mesh = bpy.data.meshes.new(self.name)
        mesh.from_pydata(self.verts, [], [f for f, _ in self.faces])
        for m in self.used:
            mesh.materials.append(self.mats[m])
        for poly, (_, m) in zip(mesh.polygons, self.faces):
            poly.material_index = m
        obj = bpy.data.objects.new(self.name, mesh)
        collection.objects.link(obj)
        return obj


def house(b, x, y, wide, deep, floors, wall, facing, rng):
    """A block-built Lagos house with a zinc roof, shuttered windows, a doorway and a balcony. facing: +1 looks toward -Y."""
    high = 3.0 * floors
    b.box((x, y, high / 2), (wide, deep, high), wall)
    b.box((x, y, high + 0.12), (wide + 0.7, deep + 0.7, 0.14), "zinc", Matrix.Rotation(math.radians(rng.uniform(3, 6)), 3, "X"))
    front = y - facing * (deep / 2 + 0.02)
    for floor in range(floors):
        z = floor * 3.0
        count = max(2, int(wide // 2.2))
        for i in range(count):
            wx = x - wide / 2 + (i + 0.5) * wide / count
            if floor == 0 and i == count // 2:
                b.box((wx, front, 1.05), (1.0, 0.08, 2.1), rng.choice(("wood", "dark", "tarp")))        # the door
            else:
                b.box((wx, front, z + 1.75), (1.0, 0.08, 1.1), "dark")                                   # a window
                b.box((wx + rng.choice((-0.3, 0.3)), front - facing * 0.04, z + 1.75), (0.45, 0.06, 1.1), rng.choice(("wood", "green", "zinc")))
                b.box((wx, front - facing * 0.06, z + 1.15), (1.2, 0.16, 0.08), "concrete")              # its sill
        if floor > 0:
            b.box((x, front - facing * 0.55, z + 0.05), (wide, 1.1, 0.12), "concrete")                   # the balcony
            b.box((x, front - facing * 1.07, z + 0.6), (wide, 0.06, 1.0), wall)
            for i in range(rng.randint(2, 5)):                                                           # washing on the rail
                b.box((x - wide / 2 + rng.uniform(0.4, wide - 0.4), front - facing * 1.12, z + 0.72), (0.5, 0.03, rng.uniform(0.5, 0.8)), rng.choice(("tarp", "yellow", "red", "cream", "green")))
    if rng.random() < 0.7:                                                                               # a stall under a tarpaulin
        sx = x + rng.uniform(-wide / 4, wide / 4)
        sy = front - facing * 1.6
        b.box((sx, sy, 2.25), (3.0, 2.4, 0.05), "tarp", Matrix.Rotation(math.radians(8 * facing), 3, "X"))
        for dx in (-1.4, 1.4):
            b.box((sx + dx, sy - facing * 1.1, 1.05), (0.07, 0.07, 2.1), "wood")
        b.box((sx, sy - facing * 0.4, 0.75), (2.2, 0.8, 0.06), "wood")
        for dx in (-0.95, 0.95):
            b.box((sx + dx, sy - facing * 0.4, 0.37), (0.07, 0.7, 0.74), "wood")
        for i in range(8):
            b.box((sx - 0.9 + i * 0.26, sy - facing * 0.4, 0.9), (0.18, 0.18, 0.24), rng.choice(("yellow", "red", "green", "cream")))


def street(collection, mats):
    rng = random.Random(7)
    b = Builder("NHS_Street", mats)
    walls = ("cream", "concrete", "green", "ochre")
    x = -30.0
    while x < 30.0:                                   # the row behind him, across the road
        wide = rng.uniform(6.0, 9.0)
        house(b, x + wide / 2, 12.5 + rng.uniform(-0.6, 0.6), wide, 6.5, rng.choice((1, 2, 2, 3)), rng.choice(walls), 1, rng)
        x += wide + rng.uniform(0.3, 1.6)
    x = -30.0
    while x < 30.0:                                   # the row in front of him, behind the camera, for the reflections and the long views
        wide = rng.uniform(6.0, 9.0)
        house(b, x + wide / 2, -16.0 + rng.uniform(-0.6, 0.6), wide, 6.5, rng.choice((1, 2, 2)), rng.choice(walls), -1, rng)
        x += wide + rng.uniform(0.3, 1.6)
    for px in range(-28, 29, 14):                     # power poles and their lines
        b.box((px, 8.2, 3.6), (0.16, 0.16, 7.2), "wood")
        b.box((px, 8.2, 6.6), (1.6, 0.1, 0.1), "wood")
    for dy, z in ((-0.6, 6.7), (0.6, 6.7), (0.0, 6.2)):
        b.box((0, 8.2 + dy * 0.8, z), (60, 0.02, 0.02), "dark")
    for i in range(26):                               # rubble and rubbish along the gutter
        b.box((rng.uniform(-22, 22), rng.uniform(6.0, 7.6), 0.08), (rng.uniform(0.2, 0.6), rng.uniform(0.2, 0.5), rng.uniform(0.08, 0.25)),
              rng.choice(("concrete", "dark", "cream", "tarp", "red")), Matrix.Rotation(rng.uniform(0, 3), 3, "Z"))
    b.box((0, 7.9, 0.1), (60, 0.5, 0.2), "concrete")  # the kerb of the open gutter
    for tx in (-13.0, 9.5, 21.0):                     # a few trees
        b.box((tx, 9.0, 1.6), (0.3, 0.3, 3.2), "wood")
        for i in range(14):
            b.box((tx + rng.uniform(-1.3, 1.3), 9.0 + rng.uniform(-1.3, 1.3), 3.6 + rng.uniform(-0.6, 1.2)), (rng.uniform(0.9, 1.6),) * 3, "leaf",
                  Matrix.Rotation(rng.uniform(0, 3), 3, (rng.random(), rng.random(), 1)))
    obj = b.finish(collection)
    # the ground: a wide sandy road, bumpy enough to catch the sun
    old = bpy.data.objects.get("NHS_Ground")
    if old:
        bpy.data.objects.remove(old)
    mesh = bpy.data.meshes.new("NHS_Ground")
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=90, y_segments=60, size=1.0, matrix=Matrix.Diagonal((45, 22, 1, 1)))
    for v in bm.verts:
        near = min(1.0, v.co.xy.length / 2.5)        # flat where he stands
        v.co.z = near * (0.035 * math.sin(v.co.x * 1.3 + 0.7 * math.sin(v.co.y)) + 0.03 * math.sin(v.co.y * 2.1 + v.co.x * 0.4)) - 0.002
    bm.to_mesh(mesh)
    bm.free()
    for p in mesh.polygons:
        p.use_smooth = True
    mesh.materials.append(mats["sand"])
    ground = bpy.data.objects.new("NHS_Ground", mesh)
    collection.objects.link(ground)
    return obj, ground


# -------------------------------------------------------------------------------------------- light and lens
def light_and_sky():
    world = bpy.data.worlds.get("NHS_World") or bpy.data.worlds.new("NHS_World")
    scene.world = world
    world.use_nodes = True
    tree = world.node_tree
    tree.nodes.clear()
    sky = tree.nodes.new("ShaderNodeTexSky")
    kinds = [i.identifier for i in sky.bl_rna.properties["sky_type"].enum_items]
    sky.sky_type = "MULTIPLE_SCATTERING" if "MULTIPLE_SCATTERING" in kinds else kinds[0]
    elevation, heading = math.radians(34), math.radians(-128)
    for prop, value in (("sun_elevation", elevation), ("sun_rotation", heading), ("sun_disc", False), ("air_density", 1.6), ("aerosol_density", 2.6), ("dust_density", 3.0), ("ozone_density", 1.0)):
        if hasattr(sky, prop):
            setattr(sky, prop, value)
    back = tree.nodes.new("ShaderNodeBackground")
    back.inputs["Strength"].default_value = 0.07
    out = tree.nodes.new("ShaderNodeOutputWorld")
    tree.links.new(sky.outputs[0], back.inputs["Color"])
    tree.links.new(back.outputs[0], out.inputs["Surface"])
    sun = bpy.data.objects.get("NHS_Sun")
    if not sun:
        sun = bpy.data.objects.new("NHS_Sun", bpy.data.lights.new("NHS_Sun", "SUN"))
        scene.collection.objects.link(sun)
    sun.data.energy, sun.data.angle, sun.data.color = 3.2, math.radians(1.5), (1.0, 0.93, 0.82)
    # the sun comes over his left shoulder from in front, so faces are lit and shadows fall behind and to his right
    toward_sun = Vector((0.55, -0.62, math.tan(elevation) * 0.83)).normalized()
    sun.rotation_euler = toward_sun.to_track_quat("Z", "Y").to_euler()
    return sun


def renderer(samples=24):
    scene.render.engine = "BLENDER_EEVEE"
    eevee = scene.eevee
    eevee.taa_render_samples, eevee.taa_samples = samples, 8
    eevee.use_shadows, eevee.use_raytracing = True, False
    eevee.use_fast_gi = True
    eevee.fast_gi_distance = 2.0
    eevee.shadow_ray_count, eevee.shadow_step_count = 2, 6
    scene.view_settings.view_transform = "AgX"
    looks = [i.identifier for i in type(scene.view_settings).bl_rna.properties["look"].enum_items]
    for want in ("AgX - Medium High Contrast", "Medium High Contrast", "AgX - Punchy"):
        if want in looks:
            scene.view_settings.look = want
            break
    scene.view_settings.exposure = -1.1
    scene.render.use_motion_blur = False
    scene.render.film_transparent = False


def dress():
    for o in [o for o in bpy.data.objects if o.name == "Floor"]:
        bpy.data.objects.remove(o)
    fix_body()
    mats = stage_materials()
    made = street(scene.collection, mats)
    light_and_sky()
    renderer()
    return mats


LOOK_MATS = dress()


# ====================================================================================== the props and the taxi
M = LOOK_MATS
STEEL2 = surface("NHS_Blade", (0.5, 0.5, 0.52), (0.75, 0.75, 0.77), 0.25, 0.28, 0.15, metal=1.0)
GUN = surface("NHS_GunMetal", (0.02, 0.02, 0.025), (0.07, 0.07, 0.08), 0.5, 0.42, 0.1, metal=0.85)
POLYMER = surface("NHS_Polymer", (0.012, 0.012, 0.014), (0.03, 0.03, 0.032), 0.8, 0.65, 0.3)
GUNWOOD = surface("NHS_GunWood", (0.2, 0.08, 0.03), (0.38, 0.17, 0.06), 0.22, 0.45, 0.15)
PAINT = surface("NHS_TaxiYellow", (0.78, 0.52, 0.02), (0.85, 0.6, 0.04), 0.02, 0.35, 0.05)
BLACK = surface("NHS_TaxiBlack", (0.01, 0.01, 0.012), (0.03, 0.03, 0.03), 0.05, 0.45, 0.05)
TRIM = surface("NHS_Trim", (0.05, 0.05, 0.055), (0.1, 0.1, 0.105), 0.3, 0.7, 0.3)
CLOTH = surface("NHS_SeatCloth", (0.1, 0.09, 0.08), (0.2, 0.17, 0.14), 0.6, 0.95, 0.5)
RUBBER = surface("NHS_Rubber", (0.01, 0.01, 0.01), (0.03, 0.03, 0.03), 0.4, 0.9, 0.5)
CHROME = surface("NHS_Chrome", (0.6, 0.6, 0.62), (0.8, 0.8, 0.82), 0.1, 0.15, 0.02, metal=1.0)


def rounded(obj, width, segments=2):
    for m in list(obj.modifiers):
        obj.modifiers.remove(m)
    bevel = obj.modifiers.new("Bevel", "BEVEL")
    bevel.width, bevel.segments, bevel.limit_method, bevel.angle_limit = width, segments, "ANGLE", math.radians(40)
    for p in obj.data.polygons:
        p.use_smooth = True
    return obj


def fresh(name, mats):
    old = bpy.data.objects.get(name)
    if old:
        bpy.data.objects.remove(old)
    mesh = bpy.data.meshes.new(name)
    for m in mats:
        mesh.materials.append(m)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


RX = lambda d: Matrix.Rotation(math.radians(d), 3, "X")

# ---------------------------------------------------------------------------------------------- the machete
machete_prop = fresh("Prop_Machete", [GUNWOOD, STEEL2, CHROME])
bm = bmesh.new()
# the blade, drawn side on: Y toward the cutting edge, Z along it
outline = [(-1.9, 6.0), (2.3, 6.0), (2.6, 24), (3.6, 42), (4.4, 52), (3.6, 58), (1.2, 62), (-1.2, 62.5), (-1.9, 58)]
face = bm.faces.new([bm.verts.new((-0.16, y, z)) for y, z in outline])
side = bmesh.ops.extrude_face_region(bm, geom=[face])
bmesh.ops.translate(bm, verts=[v for v in side["geom"] if isinstance(v, bmesh.types.BMVert)], vec=(0.32, 0, 0))
for f in bm.faces:
    f.material_index = 1
for at, size, mat in (((0, 0.2, 0), (2.5, 3.3, 12.5), 0), ((0, 0.4, -6.6), (2.8, 3.9, 1.4), 0), ((0, 0.2, 6.3), (2.9, 4.4, 0.9), 2)):
    made = bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Translation(at) @ Matrix.Diagonal((*size, 1)))
    for f in {f for v in made["verts"] for f in v.link_faces}:
        f.material_index = mat
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bm.to_mesh(machete_prop.data)
bm.free()
in_hand(machete_prop)
rounded(machete_prop, 0.12)

# ------------------------------------------------------------------------------------------------- the guns
pistol_prop = rounded(in_hand(make("Prop_Pistol", [
    ((0, 5.5, 3.9), (2.7, 18.5, 3.4), None, 0),                      # slide
    ((0, 4.6, 1.6), (2.5, 14.5, 1.6), None, 1),                      # frame
    ((0, -1.9, -3.4), (2.7, 4.8, 10.5), RX(-14), 1),                 # grip
    ((0, -3.0, -8.8), (2.9, 5.2, 0.9), RX(-14), 1),                  # magazine floor
    ((0, 4.4, -2.4), (0.9, 5.6, 0.45), None, 1), ((0, 7.1, -0.8), (0.9, 0.45, 3.4), None, 1),   # trigger guard
    ((0, 3.2, -0.6), (0.5, 0.6, 2.0), RX(18), 0),                    # trigger
    ((0, 15.0, 4.2), (1.3, 0.8, 1.3), None, 2),                      # muzzle
    ((0, -2.8, 5.9), (1.6, 0.9, 0.6), None, 0), ((0, 13.6, 5.9), (0.5, 0.9, 0.6), None, 0),     # sights
], [GUN, POLYMER, CHROME])), 0.22)

rifle_prop = rounded(in_hand(make("Prop_Rifle", [
    ((0, 6, 5.0), (3.4, 27, 5.6), None, 0),                          # receiver
    ((0, 6, 8.1), (3.0, 24, 1.2), None, 0),                          # dust cover
    ((0, -21, 3.4), (3.0, 27, 5.2), RX(-6), 1), ((0, -34.6, 2.0), (3.2, 1.2, 7.4), RX(-6), 0),  # stock and butt plate
    ((0, -1.6, -3.0), (2.7, 3.8, 9.8), RX(-16), 1),                  # grip
    ((0, 13.0, -2.6), (2.5, 5.6, 7.0), RX(10), 0), ((0, 15.2, -8.6), (2.5, 5.6, 7.0), RX(22), 0), ((0, 18.6, -14.0), (2.5, 5.6, 7.0), RX(34), 0),   # the curved magazine
    ((0, 6.5, -0.6), (0.9, 7.5, 0.4), None, 0), ((0, 5.0, 0.8), (0.5, 0.6, 2.2), RX(18), 0),    # trigger guard and trigger
    ((0, 29, 3.6), (4.0, 19, 3.8), None, 1), ((0, 29, 7.0), (3.4, 17, 2.4), None, 1),           # handguard, lower and upper
    ((0, 52, 4.6), (1.5, 34, 1.5), None, 0),                         # barrel
    ((0, 44, 7.4), (1.3, 16, 1.3), None, 0),                         # gas tube
    ((0, 52.5, 6.0), (1.9, 2.2, 3.6), None, 0),                      # gas block
    ((0, 63, 7.2), (1.0, 1.6, 5.2), None, 0), ((0, 63, 9.6), (2.4, 1.6, 0.6), None, 0),         # front sight
    ((0, 70, 4.6), (1.9, 3.6, 1.9), None, 0),                        # muzzle brake
    ((0, 20, 9.2), (1.6, 4.0, 1.0), None, 0),                        # rear sight
], [GUN, GUNWOOD])), 0.3)

# the flash at the muzzle: shown only on the frames a shot goes off
flash_mat = bpy.data.materials.get("NHS_Flash") or bpy.data.materials.new("NHS_Flash")
flash_mat.use_nodes = True
fb = bsdf_of(flash_mat)
fb.inputs["Base Color"].default_value = (1, 0.6, 0.15, 1)
fb.inputs["Emission Color"].default_value = (1.0, 0.55, 0.12, 1)
fb.inputs["Emission Strength"].default_value = 14.0


def flash_for(name, muzzle_y, muzzle_z, size):
    obj = fresh(name, [flash_mat])
    bm = bmesh.new()
    for i in range(4):          # crossed spikes fanning out of the barrel
        turn = Matrix.Rotation(math.radians(45 * i), 4, "Y")
        tip = bm.verts.new(turn @ Vector((0, size * (1.6 if i % 2 == 0 else 1.0), 0)))
        a, b, c = (bm.verts.new(turn @ Vector(v)) for v in ((size * 0.42, size * 0.25, 0), (-size * 0.42, size * 0.25, 0), (0, 0, 0)))
        bm.faces.new((c, a, tip))
        bm.faces.new((c, tip, b))
    bmesh.ops.translate(bm, verts=bm.verts, vec=(0, muzzle_y, muzzle_z))
    bm.to_mesh(obj.data)
    bm.free()
    in_hand(obj)
    lamp = bpy.data.objects.get(name + "_Light")
    if lamp:
        bpy.data.objects.remove(lamp)
    lamp = bpy.data.objects.new(name + "_Light", bpy.data.lights.new(name + "_Light", "POINT"))
    bpy.context.scene.collection.objects.link(lamp)
    lamp.data.energy, lamp.data.color, lamp.data.shadow_soft_size = 45.0, (1.0, 0.6, 0.25), 0.03
    lamp.parent = obj
    lamp.location = (0, muzzle_y + size * 0.6, muzzle_z)
    return [obj, lamp]


pistol_flash = flash_for("Prop_PistolFlash", 15.6, 4.2, 9.0)
rifle_flash = flash_for("Prop_RifleFlash", 72.0, 4.6, 13.0)
# clip -> frames the flash shows on
FLASH = {"Pistol_Fire": (pistol_flash, {1, 2}), "Rifle_Fire": (rifle_flash, {1, 4})}

# --------------------------------------------------------------------------------------------------- the taxi
# Laid out in the driver's space, V(right, forward, up): he sits on the left, the car's middle is 38 cm to his right.
CAR_MID, HALF = 38.0, 85.0
car = fresh("Prop_Car", [PAINT, BLACK, TRIM, CLOTH, RUBBER, CHROME])
bm = bmesh.new()


def part(mid, size, mat, tilt=0.0):
    """A box: mid and size as (right, forward, up); tilt leans its top back, in degrees."""
    m = Matrix.Translation(V(*mid)) @ Matrix.Rotation(math.radians(-tilt), 4, "X") @ Matrix.Diagonal((size[0], size[1], size[2], 1))
    made = bmesh.ops.create_cube(bm, size=1.0, matrix=m)
    for f in {f for v in made["verts"] for f in v.link_faces}:
        f.material_index = mat


def wheel_at(right, forward):
    made = bmesh.ops.create_cone(bm, cap_ends=True, segments=24, radius1=31, radius2=31, depth=20,
                                 matrix=Matrix.Translation(V(right, forward, 31)) @ Matrix.Rotation(math.radians(90), 4, "Y"))
    for f in {f for v in made["verts"] for f in v.link_faces}:
        f.material_index = 4
    hub = bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=17, radius2=17, depth=21,
                                matrix=Matrix.Translation(V(right, forward, 31)) @ Matrix.Rotation(math.radians(90), 4, "Y"))
    for f in {f for v in hub["verts"] for f in v.link_faces}:
        f.material_index = 5


L, R = CAR_MID - HALF, CAR_MID + HALF
part((CAR_MID, -30, 40), (170, 440, 5), 2)                                   # floor
for x in (L, R):
    part((x, -45, 74), (7, 250, 66), 0)                                      # doors
    part((x, -45, 84), (7.6, 250, 9), 1)                                     # the black stripe along them
    part((x, 58, 148), (6, 6, 92), 1, tilt=28)                               # windscreen pillar
    part((x, -42, 148), (6, 8, 82), 1)                                       # middle pillar
    part((x, -160, 148), (6, 8, 90), 1, tilt=-22)                            # rear pillar
    part((x, -55, 110), (8, 232, 3), 1)                                      # window sills
part((CAR_MID, 134, 78), (170, 112, 62), 0)                                  # bonnet
part((CAR_MID, 134, 110), (150, 100, 4), 0)
part((CAR_MID, 192, 62), (176, 8, 22), 1)                                    # front bumper
part((CAR_MID, 190, 92), (96, 5, 16), 1)                                     # grille
for x in (L + 22, R - 22):
    part((x, 190, 94), (26, 5, 12), 5)                                       # headlamps
part((CAR_MID, 64, 106), (158, 30, 24), 2)                                   # dashboard
part((0, 56, 120), (34, 12, 9), 2)                                           # the hood over the dials
part((CAR_MID, 38, 80), (22, 60, 26), 2)                                     # the tunnel between the seats
part((CAR_MID + 2, 30, 100), (4, 4, 16), 5)                                  # gear stick
part((CAR_MID, -62, 191), (172, 216, 6), 0)                                  # roof
part((CAR_MID, 42, 189), (160, 6, 5), 1)                                     # windscreen header
part((CAR_MID, -215, 80), (170, 80, 66), 0)                                  # boot
part((CAR_MID, -257, 62), (176, 8, 22), 1)                                   # rear bumper
part((CAR_MID, -120, 84), (150, 52, 14), 3)                                  # back seat
part((CAR_MID, -148, 118), (150, 12, 62), 3, tilt=12)
for seat_x in (0.0, 76.0):                                                   # front seats
    part((seat_x, -8, 84), (50, 52, 11), 3)
    part((seat_x, -36, 118), (48, 11, 64), 3, tilt=14)
    part((seat_x, -45, 160), (26, 9, 20), 3, tilt=14)
for x in (L + 4, R - 4):
    wheel_at(x, 132)
    wheel_at(x, -196)
part((L - 9, 44, 114), (14, 4, 10), 1)                                       # wing mirrors
part((R + 9, 44, 114), (14, 4, 10), 1)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bm.to_mesh(car.data)
bm.free()
car.parent = rig
rounded(car, 2.2, 3)

for old_name in ("Prop_Seat",):
    old = bpy.data.objects.get(old_name)
    if old:
        bpy.data.objects.remove(old)
wheel.data.materials.clear()
wheel.data.materials.append(TRIM)
for p in wheel.data.polygons:
    p.use_smooth = True
PROPS.clear()
PROPS.update({"machete": [machete_prop], "pistol": [pistol_prop], "rifle": [rifle_prop], "car": [car, wheel]})


def flash_show(clip, frame):
    for name, (objs, frames) in FLASH.items():
        for o in objs:
            o.hide_viewport = o.hide_render = not (name == clip and frame in frames)


flash_show("", 0)


# ==================================================================================================== the filming
def camera(direction, distance, look_at):
    cam = bpy.data.objects.get("NH_Camera")
    if not cam:
        cam = bpy.data.objects.new("NH_Camera", bpy.data.cameras.new("NH_Camera"))
        scene.collection.objects.link(cam)
    at = Vector(look_at)
    cam.location = at + Vector(direction).normalized() * distance
    cam.rotation_euler = (at - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens, cam.data.dof.use_dof, cam.data.dof.focus_distance, cam.data.dof.aperture_fstop = 35, True, distance, 2.8
    scene.camera = cam


def draw_frames():
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1280, 720, 100
    scene.eevee.taa_render_samples = 16
    scene.render.image_settings.media_type = "IMAGE"
    scene.render.image_settings.file_format = "JPEG"
    scene.render.image_settings.quality = 92
    started, drawn = time.time(), 0
    for clip, (frames, _, prop, _) in CLIPS.items():
        if ONLY and clip not in ONLY:
            continue
        # the driver is seen through the windscreen, everyone else from in front and to his left
        camera((0.35, -1.0, 0.25), 3.4, (0.1, -0.25, 1.25)) if prop == "car" else camera((0.75, -1.0, 0.18), 3.9, (0.0, -0.15, 0.93))
        os.makedirs(os.path.join(FRAMES, clip), exist_ok=True)
        for f in range(frames + 1):
            path = os.path.join(FRAMES, clip, f"{f:03d}.jpg")
            if os.path.exists(path):
                continue
            show(clip, f)
            flash_show(clip, f)
            scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            drawn += 1
        print("NH filmed", clip, "-", drawn, "frames in", round(time.time() - started), "s", flush=True)


def join_film(path):
    """One film of every clip in turn, each named on screen. Short clips are repeated to last about two seconds."""
    film = bpy.data.scenes.new("NH_Film")
    film.render.fps = 30
    film.render.resolution_x, film.render.resolution_y, film.render.resolution_percentage = 1280, 720, 100
    editor = film.sequence_editor_create()
    strips = editor.strips if hasattr(editor, "strips") else editor.sequences
    at = 1
    for clip in CLIPS:
        folder = os.path.join(FRAMES, clip)
        if not os.path.isdir(folder):
            continue
        files = sorted(os.listdir(folder))
        if len(files) > 40:
            play = files[:-1]
        elif len(files) < 12:
            play = files[:-1] * max(1, round(60 / (len(files) - 1)))
        else:
            once = files + [files[-1]] * 8          # hold the last frame a moment
            play = once * max(1, round(60 / len(once)))
        start = at
        for name in play:
            strip = strips.new_image(name=clip, filepath=os.path.join(folder, name), channel=1, frame_start=at)
            strip.frame_final_duration = 1
            at += 1
        title = strips.new_effect(name="T_" + clip, type="TEXT", channel=2, frame_start=start, length=at - start)
        title.text, title.font_size, title.location = clip.replace("_", " "), 44, (0.5, 0.08)
    film.frame_start, film.frame_end = 1, at - 1
    film.render.image_settings.media_type = "VIDEO"
    film.render.image_settings.file_format = "FFMPEG"
    film.render.ffmpeg.format, film.render.ffmpeg.codec, film.render.ffmpeg.constant_rate_factor = "MPEG4", "H264", "HIGH"
    film.render.filepath = path
    bpy.ops.render.render(animation=True, scene=film.name)
    print("NH film", path, at - 1, "frames")


draw_frames()
show("Machete_Slash", 12)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "Naija_Actions.blend"))
join_film(os.path.join(OUT, "Naija_Action_Clips.mp4"))
