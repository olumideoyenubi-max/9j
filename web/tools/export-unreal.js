// Exports the browser demo's Lagos (tile map, buildings, shop fronts, signs, street lamps, props, road markings,
// road graph, bus stops, fuel stations, park bays) as JSON for the Unreal project, so both versions share one city.
//   node web/tools/export-unreal.js [data dir]   (default: unreal/NaijaHustle/Plugins/NaijaHustleGame/Data)
// Writes lagos_city.json (the map) and naija_rules.json (routes, fares, vehicles, missions, dialogue).
// Runs the real game headless (same harness as the tests) and reads the city it generates.
//
// Units and axes in the file are Unreal's: centimetres, X = east (the map's x), Y = south (the map's y), Z = up.
// The browser's 3D world is right-handed with Y up (x, y, z) in metres; Unreal is left-handed with Z up. Mapping
// (x, y, z) -> (X = x, Y = z, Z = y) swaps two axes, which flips handedness, so nothing comes out mirrored.
// A rotation quaternion (x, y, z, w) maps to (X = -x, Y = -z, Z = -y, W = w): a yaw of θ in the browser is -θ in Unreal.
const path = require('path'), fs = require('fs');
const { chromium, open } = require('../tests/harness');
const DIR = process.argv[2] || path.join(__dirname, '..', '..', 'unreal', 'NaijaHustle', 'Plugins', 'NaijaHustleGame', 'Data');
const OUT = path.join(DIR, 'lagos_city.json'), RULES_OUT = path.join(DIR, 'naija_rules.json');

(async () => {
  const b = await chromium.launch();
  const { page, errors } = await open(b, { width: 640, height: 400 }, { render3d: true, noStart: true });
  const data = await page.evaluate(() => {
    const M = 1 / 8, CELL = 32, cm = v => Math.round(v * 100), pxcm = v => Math.round(v * M * 100);
    const { T, BUILDINGS, PROP, PROPM, nodes, BUS_STOPS, STATIONS, PARK_BAYS } = NH, MAT = NH.MAT();
    const r1 = v => Math.round(v * 1000) / 1000;
    const hex = c => '#' + c.getHexString();
    const hsl2hex = s => { const m = /hsl\((\d+),\s*(\d+)%,\s*(\d+)%\)/.exec(s); if (!m) return s; let [h, sa, l] = [+m[1] / 360, +m[2] / 100, +m[3] / 100];
      const f = n => { const k = (n + h * 12) % 12, a = sa * Math.min(l, 1 - l); return Math.round(255 * (l - a * Math.max(-1, Math.min(k - 3, 9 - k, 1)))); }; return '#' + [f(0), f(8), f(4)].map(v => v.toString(16).padStart(2, '0')).join(''); };
    // tiles: one character per 4 m cell
    const CH = { [T.GROUND]: 'G', [T.ROAD]: 'R', [T.BUILD]: 'B', [T.WATER]: 'W', [T.STALL]: 'S', [T.PARK]: 'P', [T.TOWER]: 'T', [T.STILT]: 'L', [T.SIDE]: 'K', [T.GRASS]: 'V', [T.STATION]: 'F' };
    const cols = 96, rows = 64, tiles = [], districtNames = [], dgrid = [];
    for (let r = 0; r < rows; r++) { let s = '', d = ''; for (let c = 0; c < cols; c++) { s += CH[NH.tileAt(c * CELL + 16, r * CELL + 16)];
      const pn = NH.placeName({ x: c * CELL + 16, y: r * CELL + 16 }), n = Array.isArray(pn) ? pn[0] : String(pn); let i = districtNames.indexOf(n); if (i < 0) { i = districtNames.length; districtNames.push(n); } d += String.fromCharCode(97 + i); } tiles.push(s); dgrid.push(d); }
    const buildings = BUILDINGS.map(b => ({ x: pxcm(b.x0), y: pxcm(b.y0), w: pxcm(b.w), d: pxcm(b.h), h: cm(b.ht), kind: b.kind, roof: b.style, dusty: !!b.dusty, color: hsl2hex(b.wall[1]), seed: b.seed }));
    // quads of the chunk meshes that use one material: centre, outward normal, size, atlas uv centre
    const quads = mat => { const out = []; NH.scene().traverse(o => { if (!o.isMesh || o.material !== mat) return; const P = o.geometry.attributes.position.array, N = o.geometry.attributes.normal.array, U = o.geometry.attributes.uv.array;
      for (let q = 0; q < P.length / 18; q++) { const v = k => [P[q * 18 + k * 3], P[q * 18 + k * 3 + 1], P[q * 18 + k * 3 + 2]], vs = [0, 1, 2, 3, 4, 5].map(v);
        let x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9, z0 = 1e9, z1 = -1e9, u = 0, w = 0; for (const [x, y, z] of vs) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); z0 = Math.min(z0, z); z1 = Math.max(z1, z); }
        for (let k = 0; k < 6; k++) { u += U[q * 12 + k * 2] / 6; w += U[q * 12 + k * 2 + 1] / 6; }
        const nx = N[q * 18], nz = N[q * 18 + 2];
        out.push({ x: cm((x0 + x1) / 2), y: cm((z0 + z1) / 2), z: cm(y0), w: cm(Math.hypot(x1 - x0, z1 - z0)), h: cm(y1 - y0), yaw: r1(Math.atan2(nz, nx) * 180 / Math.PI), u, v: w }); } }); return out; };
    const FRONT = ['shutter', 'half', 'open', 'painted'];
    const shopfronts = quads(MAT.front).map(q => { const slot = Math.floor(q.u * 2) + 2 * (1 - Math.floor(q.v * 2)); delete q.u; delete q.v; return Object.assign(q, { state: FRONT[slot] }); });
    const boards = (NH.SHOP_SIGNS || []).map(s => ({ title: s[0], sub: s[1], bg: s[2], fg: s[3] }));
    const extra = NH.SIGN_TEXT || {};
    const signs = quads(MAT.sign).map(q => { const slot = Math.floor((1 - q.v) * 16) * 2 + (q.u > .5 ? 1 : 0); delete q.u; delete q.v; return Object.assign(q, boards[slot] || extra[slot] || { title: '', sub: '', bg: '#444444', fg: '#ffffff' }, { slot }); });
    // props: every instance, as a box / cylinder / sphere / cone fitted to the prop's base shape
    const SKIP = { blob: 1, beacon: 1, puddle: 1 };
    const SHAPE = { pole: 'cyl', trunk: 'cyl', tank: 'cyl', drum: 'cyl', klbase: 'cyl', gascyl: 'cyl', bucket: 'cyl', basin: 'cyl', tyres: 'cyl', stool: 'cyl', trafo: 'cyl',
      crown: 'sphere', goods: 'sphere', trash: 'sphere', umb: 'cone', sand: 'cone', frond: 'cone' };
    const props = {};
    for (const [k, list] of Object.entries(PROP)) { if (SKIP[k] || !list.length) continue;
      const g = PROPM[k].geometry; g.computeBoundingBox(); const bb = g.boundingBox, c = bb.getCenter(bb.min.clone()), s = bb.getSize(bb.min.clone());
      const items = list.map(({ m, c: col }) => { const p = m.elements; const pos = { x: p[12], y: p[13], z: p[14] };
        const sx = Math.hypot(p[0], p[1], p[2]), sy = Math.hypot(p[4], p[5], p[6]), sz = Math.hypot(p[8], p[9], p[10]);
        // rotation from the normalised basis (Shepperd's method), then into Unreal's axes
        const m00 = p[0] / sx, m10 = p[1] / sx, m20 = p[2] / sx, m01 = p[4] / sy, m11 = p[5] / sy, m21 = p[6] / sy, m02 = p[8] / sz, m12 = p[9] / sz, m22 = p[10] / sz, tr = m00 + m11 + m22; let qx, qy, qz, qw;
        if (tr > 0) { const S = Math.sqrt(tr + 1) * 2; qw = S / 4; qx = (m21 - m12) / S; qy = (m02 - m20) / S; qz = (m10 - m01) / S; }
        else if (m00 > m11 && m00 > m22) { const S = Math.sqrt(1 + m00 - m11 - m22) * 2; qw = (m21 - m12) / S; qx = S / 4; qy = (m01 + m10) / S; qz = (m02 + m20) / S; }
        else if (m11 > m22) { const S = Math.sqrt(1 + m11 - m00 - m22) * 2; qw = (m02 - m20) / S; qx = (m01 + m10) / S; qy = S / 4; qz = (m12 + m21) / S; }
        else { const S = Math.sqrt(1 + m22 - m00 - m11) * 2; qw = (m10 - m01) / S; qx = (m02 + m20) / S; qy = (m12 + m21) / S; qz = S / 4; }
        return [cm(pos.x), cm(pos.z), cm(pos.y), r1(-qx), r1(-qz), r1(-qy), r1(qw), r1(sx), r1(sz), r1(sy), col ? hex(col) : '#ffffff']; });
      props[k] = { shape: SHAPE[k] || 'box', center: [cm(c.x), cm(c.z), cm(c.y)], size: [cm(s.x), cm(s.z), cm(s.y)], items }; }
    const lamps = NH.LAMPS.map(l => [cm(l.x), cm(l.z), cm(l.y)]);
    // road paint: centre dashes (skipping junction boxes), zebra crossings, motor park bays. [x0, y0, x1, y1, width, colour]
    const markings = [], VM = [2, 12, 22, 32, 42], VI = [66, 74, 82, 90], HR = [4, 14, 24, 34, 44, 54], rc = v => (v + 1) * CELL;
    const inJ = (x, y) => nodes.some(n => Math.abs(n.x - x) < 34 && Math.abs(n.y - y) < 34);
    const dashes = (x1, y1, x2, y2) => { const L = Math.hypot(x2 - x1, y2 - y1), ux = (x2 - x1) / L, uy = (y2 - y1) / L;
      for (let d = 0; d < L; d += 28) { const ax = x1 + ux * d, ay = y1 + uy * d; if (inJ(ax, ay)) continue; markings.push([pxcm(ax), pxcm(ay), pxcm(ax + ux * 14), pxcm(ay + uy * 14), 25, '#f0d78c']); } };
    for (const v of VM.concat(VI)) dashes(rc(v), 0, rc(v), 64 * CELL);
    for (const h of HR) { dashes(0, rc(h), 48 * CELL, rc(h)); dashes(64 * CELL, rc(h), 96 * CELL, rc(h)); }
    dashes(48 * CELL, rc(24), 64 * CELL, rc(24));
    const hash = (c, r) => { let h = (c * 374761393 + r * 668265263) | 0; h = Math.imul(h ^ (h >>> 13), 1274126177); return ((h ^ (h >>> 16)) >>> 0) / 4294967295; };
    const isRoadAt = (x, y) => NH.tileAt(x, y) === T.ROAD;
    for (const n of nodes) for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { if (hash(n.x + dx * 3, n.y + dy * 5) > .55) continue;
      const ax = n.x + dx * 40, ay = n.y + dy * 40; if (!isRoadAt(ax, ay)) continue;
      for (let k = -26; k <= 22; k += 8) { if (dx) markings.push([pxcm(ax - 5), pxcm(ay + k + 2.5), pxcm(ax + 5), pxcm(ay + k + 2.5), 62, '#ebe8dc']); else markings.push([pxcm(ax + k + 2.5), pxcm(ay - 5), pxcm(ax + k + 2.5), pxcm(ay + 5), 62, '#ebe8dc']); } }
    for (const b of PARK_BAYS) { const x0 = b.x - 15, x1 = b.x + 15, y0 = b.top, y1 = b.top + 60; for (const [a, c, d, e] of [[x0, y0, x0, y1], [x1, y0, x1, y1]]) markings.push([pxcm(a), pxcm(c), pxcm(d), pxcm(e), 25, '#f5c400']); }
    const stops = Object.entries(BUS_STOPS).map(([id, s]) => ({ id, name: s.name, kerb: [pxcm(s.x), pxcm(s.y)], wait: [pxcm(s.wx), pxcm(s.wy)], agbero: s.agbero || 0 }));
    return {
      format: 'naija-hustle-city', version: 1, units: 'cm', axes: 'X east, Y south, Z up (Unreal); 1 map cell = 400 cm',
      cellSize: 400, cols, rows, heights: { road: 0, kerb: 16, water: -130 },
      legend: { G: 'ground', R: 'road', B: 'building plot', W: 'lagoon water', S: 'market stall', P: 'motor park', T: 'tower plot', L: 'stilt house', K: 'pavement', V: 'grass', F: 'fuel forecourt' },
      dusty: { x: 0, y: 24 * 400, w: 22 * 400, d: 20 * 400, name: 'Oke-Erupe' },
      tiles, districts: { names: districtNames, grid: dgrid },
      buildings, shopfronts, signs, lamps, props, markings,
      roads: { nodes: nodes.map(n => [pxcm(n.x), pxcm(n.y)]), links: [].concat(...nodes.map(n => n.nb.filter(m => m > n.id).map(m => [n.id, m]))), laneOffset: 200, rightHandTraffic: true },
      busStops: stops, stations: STATIONS.map(s => ({ name: s.name, x: s.c * 400, y: s.r * 400, w: 1200, d: 800 })),
      parkBays: PARK_BAYS.map((b, i) => ({ n: i + 1, x: pxcm(b.x), y: pxcm(b.y), yaw: r1(b.a * 180 / Math.PI) })),
      playerStart: { x: pxcm(176), y: pxcm(330), z: 120, yaw: 0, note: 'home, next to Oshoja Motor Park' },
    };
  });
  // gameplay rules, with every distance, speed and acceleration converted from map pixels to cm (1 px = 12.5 cm)
  const rules = await page.evaluate(() => {
    const R = JSON.parse(JSON.stringify(NH.RULES)), K = 12.5, cm = v => Math.round(v * K);
    const c = R.conductor; for (const k of ['arriveRadius', 'slowSpeed', 'nearRadius', 'passRadius', 'departSpeed', 'departRadius', 'roughAccel', 'roughLateral']) c[k] = cm(c[k]);
    for (const v of Object.values(R.vehicles)) { v.len = cm(v.len); v.wid = cm(v.wid); v.max = cm(v.max); v.acc = cm(v.acc); v.bike = !!v.bike; v.cop = !!v.cop; }
    for (const p of Object.values(R.places)) { p.x = cm(p.x); p.y = cm(p.y); }
    for (const m of Object.values(R.missions)) for (const o of m.objectives) o.r = cm(o.r);
    R.units = 'cm, cm/s, cm/s², rad/s; minutes are in-game minutes';
    return R; });
  if (errors.length) { console.error('page errors:', errors); process.exit(1); }
  fs.mkdirSync(path.dirname(OUT), { recursive: true });
  fs.writeFileSync(OUT, JSON.stringify(data));
  fs.writeFileSync(RULES_OUT, JSON.stringify(rules, null, 1));
  const n = Object.values(data.props).reduce((a, p) => a + p.items.length, 0);
  console.log(`wrote ${RULES_OUT}: ${Object.keys(rules.routes).length} routes, ${Object.keys(rules.vehicles).length} vehicle types, ${Object.keys(rules.missions).length} missions`);
  console.log(`wrote ${OUT}: ${(fs.statSync(OUT).size / 1024).toFixed(0)} KB, ${data.buildings.length} buildings, ${data.shopfronts.length} shop fronts, ${data.signs.length} signs, ${data.lamps.length} lamps, ${n} props in ${Object.keys(data.props).length} kinds, ${data.markings.length} road markings`);
  await b.close();
})().catch(e => { console.error(e); process.exit(1); });
