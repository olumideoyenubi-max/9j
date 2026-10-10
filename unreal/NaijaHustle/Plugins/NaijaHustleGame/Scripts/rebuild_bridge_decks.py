"""Rebuilds the decks of the Lagos model's big bridges from the road graph, as one surface with no walls across it.

  blender -b --factory-startup --python rebuild_bridge_decks.py -- [tiles folder] [bridge names, comma separated]

  tiles folder   where Bridges_Decks.fbx and Bridges_Piers.fbx are   (default ~/Downloads/map lagos/Lagos_Tiles)
  bridge names   road names to start from   (default "Eko Bridge,Third Mainland Bridge")

Why: in the model every bridge way of the map (each carriageway, each slip road) was built as its own slab with its own
parapets. Where two met, one slab's parapet ran across the other's road, slabs lay on top of each other half a metre
apart, and on the Eko Bridge an upper slab ended square in the middle of the lower one: dead ends, and lanes that
could not be changed.

What it does, for the named bridges and every bridge way joined to them (carriageways, slip roads, the next viaduct):
  heights   one height a road node, read off the old deck, so ways that share a node share its height; ends that meet
            a road on the ground come down to it; no stretch is steeper than MAX_GRADE
  surface   a strip a way, as wide as the game takes that class of road to be; where strips overlap they take the same
            height, so two carriageways and the slip roads joining them make one deck. No parapets: the game stands
            those along whatever edge is left open (NHBridgeRails)
  old deck  the old slabs under the new strips are taken out; piers that would come up through the new deck are too,
            and so are piers standing in a road on the ground

The first run keeps the files as they were beside them (.before_rebuild) and every run starts from those, so it can
be run again. Then bring the two files into Unreal over the old ones: LAGOS_REFRESH=Bridges_Decks,Bridges_Piers with
LAGOS_STAGE=meshes on import_lagos_city.py (the pieces keep their names, so the level needs no rebuilding).
"""
import bpy, bmesh, json, math, os, shutil, sys
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
TILES = ARGS[0] if ARGS else os.path.join(os.path.expanduser("~"), "Downloads", "map lagos", "Lagos_Tiles")
SEEDS = [s.strip() for s in (ARGS[1] if len(ARGS) > 1 else "Eko Bridge,Third Mainland Bridge").split(",")]
REAL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Data", "lagos_real.json")

ROAD_Z = 3.05        # the model's main-road surface, m: where a bridge's end meets the ground
LIFT = 0.03          # the deck's end sits this far over the road it meets, so the two do not flicker
MAX_GRADE = 0.06     # in the model; the level stretches bridges 1.5 times upward, so 9% in the game
STEP = 6.0           # a row of the strip every so many metres
SLAB = 1.2           # how thick the deck is
TILE = 1200.0        # the model's pieces are 1.2 km squares
SAME_LEVEL = (2.0, 4.0)   # strips within 2 m of each other in height are one deck; over 4 m apart they pass over and under


def say(*a):
    print("BRIDGES:", *a)


# --------------------------------------------------------------------------------------------------- the road graph
with open(REAL, encoding="utf-8") as fh:
    real = json.load(fh)
roads = real["roads"]
flat = roads["nodes"]
HALF = [h / 100.0 for h in roads["halfWidth"]]


def at(i):
    return Vector((flat[2 * i] / 100.0, -flat[2 * i + 1] / 100.0))   # Unreal cm, Y south: the model's metres, Y north


bridge_ways = [w for w in roads["ways"] if w["b"]]
ground_nodes = set()
for w in roads["ways"]:
    if not w["b"]:
        ground_nodes.update(w["n"])
by_node = {}
for k, w in enumerate(bridge_ways):
    for i in w["n"]:
        by_node.setdefault(i, []).append(k)
chosen, queue = set(), [k for k, w in enumerate(bridge_ways) if w["name"] in SEEDS or SEEDS == ["*"]]   # "*": every bridge
while queue:
    k = queue.pop()
    if k in chosen:
        continue
    chosen.add(k)
    for i in bridge_ways[k]["n"]:
        if i not in ground_nodes:          # a bridge that comes down to the ground ends there: the next one is another bridge
            queue.extend(by_node[i])
ways = [bridge_ways[k] for k in sorted(chosen)]
others = [w for k, w in enumerate(bridge_ways) if k not in chosen]
say(f"{len(ways)} bridge ways joined to {SEEDS}, {sum(sum((at(a) - at(b)).length for a, b in zip(w['n'], w['n'][1:])) for w in ways) / 1000:.1f} km; {len(others)} other bridge ways left as they are")

# ------------------------------------------------------------------------------------------------- the old model
decks_fbx, piers_fbx = os.path.join(TILES, "Bridges_Decks.fbx"), os.path.join(TILES, "Bridges_Piers.fbx")
for f in (decks_fbx, piers_fbx):
    if not os.path.exists(f + ".before_rebuild"):
        shutil.copy2(f, f + ".before_rebuild")


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path + ".before_rebuild")
    return [o for o in bpy.data.objects if o.type == "MESH"]


def world_mesh(objs):
    bm = bmesh.new()
    for o in objs:
        m = o.data.copy()
        m.transform(o.matrix_world)
        bm.from_mesh(m)
        bpy.data.meshes.remove(m)
    bm.faces.ensure_lookup_table()
    return bm


deck_objs = load(decks_fbx)
old = world_mesh(deck_objs)
old_tree = BVHTree.FromBMesh(old)


def tops(tree, x, y, limit=6):
    """Heights of every surface facing up over a point, highest first."""
    out, z = [], 500.0
    while len(out) < limit:
        hit = tree.ray_cast(Vector((x, y, z)), Vector((0, 0, -1)))
        if hit[0] is None:
            break
        if hit[1].z > 0.5:
            out.append(hit[0].z)
        z = hit[0].z - 0.05
    return out


# ----------------------------------------------------------------------------------------------------- heights
height, fixed = {}, set()
for w in sorted(ways, key=lambda w: -len(w["n"])):
    last = None
    for i in w["n"]:
        if i in height:
            last = height[i]
            continue
        p = at(i)
        if i in ground_nodes:
            height[i], last = ROAD_Z + LIFT, ROAD_Z + LIFT
            fixed.add(i)
            continue
        found = [z for z in tops(old_tree, p.x, p.y) if z > ROAD_Z + 0.5]
        if not found:
            continue
        # a parapet, or another slab, may lie over the deck here: take the surface that carries on from the last node
        z = min(found, key=lambda v: abs(v - last)) if last is not None and last > ROAD_Z + 1.0 else found[-1] if len(found) > 1 and found[0] - found[-1] < 1.6 else found[0]
        height[i], last = z, z
missing = 0
for w in ways:                                  # nodes the old deck did not reach: carried along the way from those it did
    n = w["n"]
    known = [k for k, i in enumerate(n) if i in height]
    for k, i in enumerate(n):
        if i in height:
            continue
        missing += 1
        before, after = [j for j in known if j < k], [j for j in known if j > k]
        if before and after:
            a, b = before[-1], after[0]
            height[i] = height[n[a]] + (height[n[b]] - height[n[a]]) * (k - a) / (b - a)
        elif before or after:
            height[i] = height[n[(before or after)[-1 if before else 0]]]
        else:
            height[i] = ROAD_Z + 7.0
# ------------------------------------------------------------------------------------------------- the centre lines
# A way becomes rows a few metres apart. A strip reaches EDGE past the road's half width to the driver's right, and to
# the left as far as the lanes do ('l' in the road data: the middle of a dual road), so two carriageways just overlap.
EDGE = 1.0


class Line:
    def __init__(self, k, w):
        self.k, self.way = k, w
        half = HALF[w["c"]]
        self.right, self.left = half + EDGE, (min(w.get("l", 1e9) / 100.0, half) if w["o"] else half) + EDGE
        self.mid, self.half = (self.right - self.left) / 2, (self.right + self.left) / 2
        self.rows, self.node_at = [], {}            # [point, variable, direction, how far along]; node: how far along
        pts, run = [at(i) for i in w["n"]], 0.0
        for j in range(len(pts) - 1):
            a, b = pts[j], pts[j + 1]
            d = (b - a).length
            if d < 0.05:
                continue
            cuts = max(1, round(d / STEP))
            for c in range(cuts):
                self.rows.append([a.lerp(b, c / cuts), ("n", w["n"][j]) if c == 0 else ("r", k, len(self.rows)), None, run + d * c / cuts,
                                  height[w["n"][j]] + (height[w["n"][j + 1]] - height[w["n"][j]]) * c / cuts])
            self.node_at[w["n"][j]] = run
            run += d
        self.rows.append([pts[-1], ("n", w["n"][-1]), None, run, height[w["n"][-1]]])
        self.node_at[w["n"][-1]] = run
        for j, r in enumerate(self.rows):
            t = self.rows[min(j + 1, len(self.rows) - 1)][0] - self.rows[max(j - 1, 0)][0]
            r[2] = t.normalized() if t.length > 1e-6 else Vector((1, 0))

    def side(self, j):
        t = self.rows[j][2]
        return Vector((t.y, -t.x))                  # the driver's right

    def over(self, p, j):
        """The point of this way's line nearest p, looked for round row j: (height, how far p is to its right)."""
        best = None
        for a in range(max(0, j - 2), min(len(self.rows) - 1, j + 2)):
            ra, rb = self.rows[a], self.rows[a + 1]
            ab = rb[0] - ra[0]
            t = min(1.0, max(0.0, (p - ra[0]).dot(ab) / max(ab.length_squared, 1e-9)))
            q = ra[0].lerp(rb[0], t)
            d = (p - q).length
            if best is None or d < best[0]:
                n = ab.normalized()
                best = (d, z[ra[1]] + (z[rb[1]] - z[ra[1]]) * t, (p - q).dot(Vector((n.y, -n.x))))
        return best[1], best[2]

    def weight(self, off):
        return max(0.0, 1.0 - abs(off - self.mid) / (self.half + 1.0)) ** 2


lines = [Line(k, w) for k, w in enumerate(ways)]
lines = [l for l in lines if len(l.rows) >= 2]
for k, l in enumerate(lines):
    l.k = k
samples = [(n, j) for n, l in enumerate(lines) for j in range(len(l.rows))]
kd = KDTree(len(samples))
for k, (n, j) in enumerate(samples):
    p = lines[n].rows[j][0]
    kd.insert((p.x, p.y, 0.0), k)
kd.balance()
REACH = 2 * max(max(l.left, l.right) for l in lines) + STEP

# ----------------------------------------------------------------------------------------------- one deck or two
# Two ways close enough for their strips to overlap are one deck where they started at much the same height, or near a
# node they share (a slip road coming onto a carriageway). Anywhere else they pass over and under, and are left to.
z = {}
for l in lines:
    for r in l.rows:
        z.setdefault(r[1], r[4])
fixed_vars = {("n", i) for i in fixed}
along = [(l.rows[j][1], l.rows[j + 1][1], max((l.rows[j + 1][0] - l.rows[j][0]).length, 0.5)) for l in lines for j in range(len(l.rows) - 1)]
ties, tied = [], {}                                  # (variable, variable, metres apart); (line, row): the lines it is one deck with
steepest = max(abs(z[a] - z[b]) / d for a, b, d in along)
CLEAR = 4.5                                          # a deck this far over another is a flyover; nearer, and it has to be the same deck
for attempt in range(8):
    added = 0
    for n, l in enumerate(lines):
        for j, r in enumerate(l.rows):
            near = {}
            for _, k, d in kd.find_range((r[0].x, r[0].y, 0.0), REACH):
                m, i = samples[k]
                if m != n and (m not in near or d < near[m][0]):
                    near[m] = (d, i)
            for m, (d, i) in near.items():
                o = lines[m]
                if m in tied.get((n, j), {}) or d > max(l.left, l.right) + max(o.left, o.right):
                    continue
                shared = [node for node in l.node_at if node in o.node_at]
                joined = any(abs(l.node_at[node] - r[3]) < 300.0 and abs(o.node_at[node] - o.rows[i][3]) < 300.0 for node in shared)
                # the first time by where the old deck put them; after that, whatever has ended up too close to pass under
                if abs(z[r[1]] - z[o.rows[i][1]]) < (SAME_LEVEL[0] if attempt == 0 else CLEAR) or (joined and attempt == 0):
                    ties.append((r[1], o.rows[i][1], d))
                    tied.setdefault((n, j), {})[m] = i
                    added += 1
    if not added:
        break
    for sweep in range(2500):                        # no stretch steeper than MAX_GRADE, and one deck level across itself
        moved = 0.0
        for group, allow in ((along, lambda d: MAX_GRADE * d), (ties, lambda d: 0.02 * d + 0.01)):
            for a, b, d in group:
                over = abs(z[a] - z[b]) - allow(d)
                if over <= 1e-4 or a == b:
                    continue
                hi, lo = (a, b) if z[a] > z[b] else (b, a)
                if hi in fixed_vars and lo in fixed_vars:
                    continue
                share = 1.0 if lo in fixed_vars else 0.0 if hi in fixed_vars else 0.6     # mostly the high end comes down
                z[hi] -= over * share
                z[lo] += over * (1.0 - share)
                moved = max(moved, over)
        if moved < 0.003:
            break
    say(f"round {attempt + 1}: {added} more places where two ways are one deck, settled in {sweep + 1} sweeps")
say(f"{len(height)} nodes, {len(fixed)} on the ground, {missing} the old deck did not reach; {len(ties)} places where two ways are one deck")
say(f"steepest stretch was {steepest * 100:.0f}%, now {max(abs(z[a] - z[b]) / d for a, b, d in along) * 100:.1f}%; "
    f"across one deck the ways differ by at most {max([abs(z[a] - z[b]) for a, b, d in ties] or [0]):.2f} m")


def surface(n, j, off):
    """The deck's height at a point `off` to the right of row j of line n: every way of the same deck has a say, by how near its middle the point is."""
    l = lines[n]
    p = l.rows[j][0] + l.side(j) * off
    total = weight = l.weight(off) + 1e-4
    total *= z[l.rows[j][1]]
    for m, i in tied.get((n, j), {}).items():
        h, o = lines[m].over(p, i)
        k = lines[m].weight(o)
        total += h * k
        weight += k
    return p, total / weight


# ----------------------------------------------------------------------------------------------------- the strips
new = bmesh.new()
ACROSS = (-1.0, -0.5, 0.0, 0.5, 1.0)
SLOT = 6.0           # a gap narrower than this between two strips of one deck is closed
tops_of = []
top_faces = 0
for n, l in enumerate(lines):
    grid = []
    for j in range(len(l.rows)):
        # A strip of the same deck that stops a little short of this one (the other carriageway where the two draw apart,
        # a slip road running alongside before it joins) would leave a slot between them: this one's edge goes out to it.
        lo, hi = -l.left, l.right
        for m, i in tied.get((n, j), {}).items():
            o = lines[m]
            for edge in (lo, hi):
                _, off = o.over(l.rows[j][0] + l.side(j) * edge, i)
                gap = -o.left - off if off < -o.left else off - o.right
                if 0.0 < gap < SLOT:
                    if edge == lo:
                        lo = min(lo, -l.left - gap - 0.6)
                    else:
                        hi = max(hi, l.right + gap + 0.6)
        row = []
        for u in ACROSS:
            p, h = surface(n, j, (lo + hi) / 2 + u * (hi - lo) / 2)
            row.append(new.verts.new((p.x, p.y, h)))
        grid.append(row)
    tops_of.extend(v for row in grid for v in row)
    under = [[new.verts.new((row[k].co.x, row[k].co.y, row[k].co.z - SLAB)) for k in (0, len(ACROSS) - 1)] for row in grid]
    for k in range(len(grid) - 1):
        a, b = grid[k], grid[k + 1]
        for c in range(len(ACROSS) - 1):
            new.faces.new((a[c], a[c + 1], b[c + 1], b[c])).material_index = 0
            top_faces += 1
        ua, ub = under[k], under[k + 1]
        for quad in ((a[0], b[0], ub[0], ua[0]), (b[-1], a[-1], ua[1], ub[1]), (ua[0], ub[0], ub[1], ua[1])):
            new.faces.new(quad).material_index = 1
    new.faces.new((grid[0][-1], grid[0][0], under[0][0], under[0][1])).material_index = 1      # the two ends closed
    new.faces.new((grid[-1][0], grid[-1][-1], under[-1][1], under[-1][0])).material_index = 1
new.normal_update()
new.faces.ensure_lookup_table()
new_tree = BVHTree.FromBMesh(new)
# Where one strip lies a hand's breadth under another of the same deck, its points are lifted to the one on top, so
# the two are one surface and not a lip across the road.
for _ in range(2):
    lifted = 0
    for v in tops_of:
        hit = new_tree.ray_cast(Vector((v.co.x, v.co.y, v.co.z + 0.9)), Vector((0, 0, -1)), 0.89)
        if hit[0] is not None and hit[1].z > 0.5 and hit[0].z > v.co.z + 0.005:
            v.co.z = hit[0].z
            lifted += 1
    new.normal_update()
    new_tree = BVHTree.FromBMesh(new)
    say(f"{lifted} points lifted to the strip over them")
say(f"{len(lines)} strips, {top_faces} road faces")

# ------------------------------------------------------------------------------- the old deck under the new strips
other_kd_pts = []
for w in others:
    pts = [at(i) for i in w["n"]]
    for a, b in zip(pts, pts[1:]):
        for c in range(max(1, round((b - a).length / STEP)) + 1):
            other_kd_pts.append((a.lerp(b, c / max(1, round((b - a).length / STEP))), HALF[w["c"]]))
okd = KDTree(max(len(other_kd_pts), 1))
for k, (p, _) in enumerate(other_kd_pts):
    okd.insert((p.x, p.y, 0.0), k)
okd.balance()


def covered(x, y, margin=5.0):
    """Under one of the new strips (and not nearer to a bridge that is being left alone)."""
    best = None
    for _, k, d in kd.find_range((x, y, 0.0), REACH + margin):
        n, j = samples[k]
        l = lines[n]
        off = (Vector((x, y)) - l.rows[j][0]).dot(l.side(j))
        if abs((Vector((x, y)) - l.rows[j][0]).dot(l.rows[j][2])) > STEP:
            continue
        r = abs(off - l.mid) / (l.half + margin)
        best = r if best is None else min(best, r)
    if best is None or best > 1.0:
        return False
    for _, k, d in okd.find_range((x, y, 0.0), REACH):
        if d / other_kd_pts[k][1] < min(best, 1.0):
            return False
    return True


removed = kept = 0
for o in deck_objs:
    inv = o.matrix_world.inverted()
    bm = bmesh.new()
    bm.from_mesh(o.data)
    doomed = []
    for f in bm.faces:
        c = o.matrix_world @ f.calc_center_median()
        if covered(c.x, c.y):
            doomed.append(f)
    removed += len(doomed)
    kept += len(bm.faces) - len(doomed)
    bmesh.ops.delete(bm, geom=doomed, context="FACES")
    bm.to_mesh(o.data)
    bm.free()
say(f"old deck: {removed} faces taken out, {kept} kept")

# the new faces go into the piece of the square they are in (or the nearest piece there is), in that piece's own space
centres = {o: sum(((o.matrix_world @ Vector(c)) for c in o.bound_box), Vector()) / 8 for o in deck_objs}
homes = {}
for f in new.faces:
    c = f.calc_center_median()
    key = (math.floor(c.x / TILE), math.floor(c.y / TILE))
    homes.setdefault(key, []).append(f)
for key, faces in homes.items():
    name = "Bridges_Decks__" + "_".join(("m" + str(-v)) if v < 0 else str(v) for v in key)
    o = bpy.data.objects.get(name) or min(deck_objs, key=lambda o: (centres[o].xy - Vector(((key[0] + 0.5) * TILE, (key[1] + 0.5) * TILE))).length)
    inv = o.matrix_world.inverted()
    bm = bmesh.new()
    bm.from_mesh(o.data)
    made = {}
    for f in faces:
        vs = []
        for v in f.verts:
            if v not in made:
                made[v] = bm.verts.new(inv @ v.co)
            vs.append(made[v])
        bm.faces.new(vs).material_index = f.material_index
    bm.normal_update()
    bm.to_mesh(o.data)
    bm.free()
empty = [o.name for o in deck_objs if len(o.data.polygons) == 0]
for o in deck_objs:                               # a piece with nothing left keeps a speck under the ground: the level has an actor for it
    if len(o.data.polygons) == 0:
        inv = o.matrix_world.inverted()
        c = centres[o]
        bm = bmesh.new()
        bm.faces.new([bm.verts.new(inv @ Vector((c.x + dx, c.y + dy, -30.0))) for dx, dy in ((0, 0), (0.1, 0), (0, 0.1))])
        bm.to_mesh(o.data)
        bm.free()


def export(path):
    for o in bpy.data.objects:
        o.select_set(o.type == "MESH")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)


export(decks_fbx)
say(f"wrote {decks_fbx}; {len(empty)} pieces left empty {empty[:6]}")

# checks on the result: what is left stacked close together, and how steep the surface is under the centre lines
final = world_mesh(deck_objs)
final_tree = BVHTree.FromBMesh(final)
stacked = walls = low = holes = 0
where = {}
for n, j in samples:
    l = lines[n]
    if j < 2 or j > len(l.rows) - 3:               # the very ends lie on the ground road
        continue
    for off in (0.0, l.mid - 0.8 * l.half, l.mid + 0.8 * l.half):
        p = l.rows[j][0] + l.side(j) * off
        found = tops(final_tree, p.x, p.y)
        gaps = [a - b for a, b in zip(found, found[1:])]
        kind = "hole" if not found else "stacked" if any(0.08 < g <= 0.6 for g in gaps) else "wall" if any(0.6 < g < 1.6 for g in gaps) else "low" if any(1.6 <= g < 4.0 for g in gaps) else None
        if kind:
            where.setdefault((kind, round(p.x / 150), round(p.y / 150)), []).append((l.way["name"] or "slip road", l.way["c"], [round(v, 1) for v in found[:3]]))
        holes += kind == "hole"
        stacked += kind == "stacked"
        walls += kind == "wall"
        low += kind == "low"
for (kind, x, y), found in sorted(where.items()):
    say(f"  {kind} near {x * 150}, {y * 150}: {len(found)} points, e.g. {found[0]}")
say(f"across the strips ({3 * len(samples)} points): {holes} with no deck, {stacked} with two decks within 60 cm, {walls} with a wall-high step, {low} with a deck less than 4 m over another")

# ------------------------------------------------------------------------------------------------------- the piers
ground_pts = []
for w in roads["ways"]:
    if w["b"]:
        continue
    pts = [at(i) for i in w["n"]]
    for a, b in zip(pts, pts[1:]):
        cuts = max(1, round((b - a).length / 3.0))
        ground_pts.extend((a.lerp(b, c / cuts), HALF[w["c"]]) for c in range(cuts + 1))
gkd = KDTree(len(ground_pts))
for k, (p, _) in enumerate(ground_pts):
    gkd.insert((p.x, p.y, 0.0), k)
gkd.balance()
pier_objs = load(piers_fbx)
cut = left = 0
for o in pier_objs:
    bm = bmesh.new()
    bm.from_mesh(o.data)
    seen, doomed = set(), []
    for start in bm.faces:                       # a pier is one loose part of the piece
        if start in seen:
            continue
        part, stack = [], [start]
        seen.add(start)
        while stack:
            f = stack.pop()
            part.append(f)
            for e in f.edges:
                for g in e.link_faces:
                    if g not in seen:
                        seen.add(g)
                        stack.append(g)
        vs = {v for f in part for v in f.verts}
        ws = [o.matrix_world @ v.co for v in vs]
        c = sum(ws, Vector()) / len(ws)
        top = max(v.z for v in ws)
        deck = tops(new_tree, c.x, c.y)
        # under the new deck it must stop below the road; where the old deck has gone and no new one is, it holds up
        # nothing; and it cannot stand in a road on the ground (the map's roads are newer than the model's piers)
        in_road = any(d < ground_pts[k][1] - 0.5 for _, k, d in gkd.find_range((c.x, c.y, 0.0), 14.0))
        if in_road or (deck and top > min(deck) - 0.15) or (not deck and covered(c.x, c.y, 2.0)):
            doomed.extend(part)
            cut += 1
        else:
            left += 1
    bmesh.ops.delete(bm, geom=doomed, context="FACES")
    if len(bm.faces) == 0:
        c = sum(((o.matrix_world @ Vector(b)) for b in o.bound_box), Vector()) / 8
        inv = o.matrix_world.inverted()
        bm.faces.new([bm.verts.new(inv @ Vector((c.x + dx, c.y + dy, -30.0))) for dx, dy in ((0, 0), (0.1, 0), (0, 0.1))])
    bm.to_mesh(o.data)
    bm.free()
export(piers_fbx)
say(f"piers: {cut} taken out, {left} left; wrote {piers_fbx}")
