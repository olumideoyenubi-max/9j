// AAA look: photo textures and GLB models loaded from textures/ and models/ (fixtures generated here and
// served over HTTP), the procedural fallback when they're missing, dust, the helicopter at 3 stars,
// the sprint camera, the singlet-and-cargo outfit, and the graphics tiers' texture sizes.
const { chromium, open } = require('./harness');
const fs = require('fs'), path = require('path'), os = require('os'), http = require('http');
let fails = 0; const ok = (name, cond, info) => { console.log((cond ? 'PASS ' : 'FAIL ') + name + (cond ? '' : '  ' + JSON.stringify(info))); if (!cond) fails++; };
const SRC = process.env.SRC || path.join(__dirname, '..', 'index.html');
const page0 = '<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"></head><body>' + fs.readFileSync(SRC, 'utf8') + '</body></html>';
function serve(dir) { // a tiny static server; missing files are 404s, like an artifact without them
  const types = { '.html': 'text/html', '.jpg': 'image/jpeg', '.glb': 'model/gltf-binary', '.json': 'application/json' };
  const srv = http.createServer((q, r) => { const f = path.join(dir, decodeURIComponent(q.url.split('?')[0])); if (!f.startsWith(dir) || !fs.existsSync(f) || fs.statSync(f).isDirectory()) { r.writeHead(404); return r.end(); } r.writeHead(200, { 'content-type': types[path.extname(f)] || 'application/octet-stream' }); fs.createReadStream(f).pipe(r); });
  return new Promise(res => srv.listen(0, '127.0.0.1', () => res(srv)));
}
(async () => {
  const b = await chromium.launch();
  // ---- fixtures: JPGs drawn on a canvas, GLBs exported with GLTFExporter (a box body named "paint" plus a dark "glass" part)
  const gen = await open(b, undefined, { noStart: true });
  const fx = await gen.page.evaluate(async () => {
    const jpg = (w, col) => { const c = document.createElement('canvas'); c.width = c.height = w; const g = c.getContext('2d'); g.fillStyle = col; g.fillRect(0, 0, w, w); g.fillStyle = 'rgba(0,0,0,.3)'; for (let i = 0; i < 40; i++) g.fillRect(Math.random() * w, Math.random() * w, 8, 8); return c.toDataURL('image/jpeg', .8).split(',')[1]; };
    const tex = {}; for (const base of ['concrete_wall', 'peeling_plaster', 'laterite_road', 'cracked_asphalt', 'rusty_zinc', 'blue_tarp', 'wood_planks', 'rubble']) { tex[base + '_albedo.jpg'] = jpg(base === 'rusty_zinc' ? 4096 : 256, '#a07050'); tex[base + '_normal.jpg'] = jpg(256, '#8080ff'); tex[base + '_rough.jpg'] = jpg(256, '#c0c0c0'); }
    const THREE = await import('three'); const { GLTFExporter } = await import('three/addons/exporters/GLTFExporter.js');
    const glb = async (sx, sy, sz) => { const s = new THREE.Scene(); const paint = new THREE.MeshStandardMaterial({ name: 'paint', color: 0xffcc00 }), glass = new THREE.MeshStandardMaterial({ name: 'glass', color: 0x223344 });
      const body = new THREE.Mesh(new THREE.BoxGeometry(sx, sy, sz), paint); body.position.y = sy / 2; const top = new THREE.Mesh(new THREE.BoxGeometry(sx * .8, sy * .3, sz * .6), glass); top.position.set(0, sy * 1.15, sz * .1); s.add(body, top);
      const buf = await new GLTFExporter().parseAsync(s, { binary: true }); let bin = ''; const u = new Uint8Array(buf); for (let i = 0; i < u.length; i++) bin += String.fromCharCode(u[i]); return btoa(bin); };
    // glTF faces +Z, so a danfo is long along z: 2 wide, 1.6 tall, 5 long (scaled into the 5.52 m box)
    return { tex, models: { 'danfo.glb': await glb(2, 1.6, 5), 'heli.glb': await glb(2, 2.2, 9), 'chair.glb': await glb(1, 2, 1), 'stall.glb': await glb(3, 2, 1.5) } };
  });
  await gen.ctx.close();
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'nh-assets-')); fs.mkdirSync(path.join(dir, 'textures')); fs.mkdirSync(path.join(dir, 'models'));
  fs.writeFileSync(path.join(dir, 'index.html'), page0);
  for (const [f, d] of Object.entries(fx.tex)) fs.writeFileSync(path.join(dir, 'textures', f), Buffer.from(d, 'base64'));
  // models: a manifest naming four of the nine (keke, okada... absent) and one entry that points at a missing file
  for (const [f, d] of Object.entries(fx.models)) fs.writeFileSync(path.join(dir, 'models', f), Buffer.from(d, 'base64'));
  fs.writeFileSync(path.join(dir, 'models', 'manifest.json'), JSON.stringify({ danfo: 'danfo.glb', heli: { file: 'heli.glb' }, chair: 'chair.glb', stall: 'stall.glb', tank: 'nope.glb' }));
  const srv = await serve(dir), url = `http://127.0.0.1:${srv.address().port}/index.html`;

  // ---- with assets
  const { page, errors } = await open(b, { width: 960, height: 600 }, { render3d: true, url, live: true });
  const run = f => page.evaluate(f);
  await page.waitForFunction(() => NH.ASSET && NH.ASSET.done, null, { timeout: 30000 });
  await run(() => NH.ASSET.done); await run(() => NH.freeze());
  const r1 = await run(() => { const MAT = NH.MAT(), z = NH.TEXR.zinc;
    return { loaded: NH.ASSET.loaded.slice().sort(), failed: NH.ASSET.failed, tex: Object.keys(NH.TEXR).sort(), zincMap: MAT.zinc.map === z.map, zincNormal: MAT.zinc.normalMap === z.normal, zincRough: MAT.zinc.roughnessMap === z.rough,
      zincW: z.map.image.width, wrap: z.map.wrapS === 1000, aniso: z.map.anisotropy, tarp: MAT.tarp.map === NH.TEXR.tarp.map, photo: NH.WALLU.uPhoto.value, conc: NH.WALLU.uConc.value === NH.TEXR.concrete.map,
      lat: NH.TERRU.uLat.value === NH.TEXR.laterite.map, asph: NH.TERRU.uDetail.value === NH.TEXR.asphalt.map, tier: NH.gfx() }; });
  ok('all eight photo texture sets load (albedo + normal + roughness) and land on their materials', r1.tex.join() === 'asphalt,concrete,laterite,planks,plaster,rubble,tarp,zinc' && r1.zincMap && r1.zincNormal && r1.zincRough && r1.tarp && r1.photo === 1 && r1.conc && r1.lat && r1.asph, r1);
  ok('textures tile, are anisotropic, and a 4096 px file is cut to the tier size', r1.wrap && r1.aniso >= 1 && r1.zincW === { low: 512, medium: 1024, high: 2048 }[r1.tier], r1);
  ok('models named in the manifest load; a missing one fails alone', ['models/chair.glb', 'models/danfo.glb', 'models/heli.glb', 'models/stall.glb'].every(f => r1.loaded.includes(f)) && r1.failed.join() === 'models/nope.glb', r1);

  const r2 = await run(() => { const M = NH.MODELS.danfo, box = { x: 0, y: 0, z: 0 }; for (const p of M.parts) { p.geometry.computeBoundingBox(); const s = p.geometry.boundingBox; box.x = Math.max(box.x, s.max.x - s.min.x); box.y = Math.max(box.y, s.max.y); box.z = Math.max(box.z, s.max.z - s.min.z); box.min = Math.min(box.min ?? 0, s.min.y); }
    NH.resetWorld(); NH.player.x = 300; NH.player.y = 480; const s = NH.roadSpotNear(NH.player, 0, 100); NH.spawnVehicle('danfo', s.x, s.y, s.a, { parked: true }); NH.renderOnce(0);
    const counts = NH.MB.danfo.map(x => ({ n: x.b.m.count, paint: x.paint }));
    const chair = NH.PROPM.chair, stall = NH.PROPM.stallModel, hidden = NH.scene().getObjectsByProperty ? 0 : 0; let stallMeshes = 0, stallHidden = 0; NH.scene().traverse(o => { if (o.userData && o.userData.stall) { stallMeshes++; if (!o.visible) stallHidden++; } });
    return { box, parts: M.parts.length, paint: M.parts.filter(p => p.paint).length, counts, chairMats: Array.isArray(chair.material) ? chair.material.length : 0, chairN: chair.count, stall: !!stall, stallMeshes, stallHidden }; });
  ok('a model is turned to face +X, centred, sat on the ground and scaled uniformly to fit the vehicle box', r2.box.x <= 5.53 && r2.box.y <= 2.11 && r2.box.z <= 2.37 && (Math.abs(r2.box.x - 5.52) < .05 || Math.abs(r2.box.y - 2.1) < .02 || Math.abs(r2.box.z - 2.36) < .05) && r2.box.x > r2.box.z * 2 && Math.abs(r2.box.min) < .01, r2);
  ok('model danfos draw from instanced batches; only the "paint" part takes the livery colour', r2.parts === 2 && r2.paint === 1 && r2.counts.some(c => c.n > 0), r2);
  ok('street props swap to the model in place: chairs keep their instances, stall models replace procedural stalls', r2.chairMats === 2 && r2.chairN > 0 && r2.stall && r2.stallMeshes > 0 && r2.stallHidden === r2.stallMeshes, r2);

  // ---- helicopter at 3 stars, rotor sound and wash
  const r3 = await run(() => { NH.resetWorld(); NH.S.minutes = 13 * 60; NH.WORLD.hour = -1; NH.player.x = 300; NH.player.y = 480; NH.setSound(true); NH.addHeat(3); const p0 = NH.DUSTP.n; NH.sim(8);
    const h = NH.heliState(), d = h && Math.hypot(h.x - NH.player.x, h.y - NH.player.y); NH.renderOnce(0); const heliN = NH.MB.heli.map(x => x.b.m.count);
    return { heli: !!h, d, heliN, dust: NH.DUSTP.n, rotor: !!NH.SND.rotor }; });
  ok('at 3 stars a helicopter (the GLB) circles the player and kicks up rotor-wash dust', r3.heli && r3.d > 60 && r3.d < 260 && r3.heliN.some(n => n > 0) && r3.dust > 0, r3);
  const r3b = await run(() => { NH.clearHeat(); NH.sim(1); return !!NH.heliState(); });
  ok('the helicopter leaves when the heat drops', !r3b, r3b);

  // ---- sprint camera, foot dust, outfit
  const r4 = await run(() => { NH.resetWorld(); NH.S.minutes = 12 * 60; NH.WORLD.hour = -1; NH.player.x = 6 * 32 + 16; NH.player.y = 35 * 32 + 16; NH.CAMS.yaw = 0; NH.CAMS.dragT = performance.now() + 1e9; NH.sim(1); NH.renderOnce(0);
    const cam = NH.camera(), walk = { y: cam.position.y, fov: cam.fov }; NH.DUSTP.n = 0; NH.keys.ShiftLeft = true; NH.keys.KeyW = true; let dust = 0; for (let i = 0; i < 40; i++) { NH.sim(1 / 30); NH.renderOnce(i / 30); dust = Math.max(dust, NH.DUSTP.n); }
    const spr = { y: cam.position.y - NH.heightAt(NH.player.x, NH.player.y), fov: cam.fov }; NH.keys.ShiftLeft = NH.keys.KeyW = false; return { walk, spr, dust, dusty: NH.isDusty(6, 35) }; });
  ok('sprinting drops the camera to shoulder height and widens the FOV; feet kick up dust on laterite', r4.spr.y < 2.2 && r4.spr.fov > r4.walk.fov + 4 && r4.dust > 2 && r4.dusty, r4);
  const r5 = await run(() => { NH.S.outfits.push('fit_singlet_cargo'); NH.S.outfit = 'fit_singlet_cargo'; NH.renderOnce(0); return { outfit: NH.S.outfit }; });
  ok('the singlet-and-cargo outfit can be worn', r5.outfit === 'fit_singlet_cargo', r5);
  const errs1 = errors.filter(e => !/404|Failed to load resource/.test(e)); ok("no errors with assets", errs1.length === 0, errs1);
  await page.context().close();

  // ---- without assets: a 404 for every file keeps the procedural look and stops probing early
  const empty = fs.mkdtempSync(path.join(os.tmpdir(), 'nh-noassets-')); fs.writeFileSync(path.join(empty, 'index.html'), page0); const srv2 = await serve(empty); const reqs = [];
  const o2 = await open(b, { width: 960, height: 600 }, { render3d: true, url: `http://127.0.0.1:${srv2.address().port}/index.html`, live: true });
  o2.page.on('request', q => reqs.push(q.url()));
  await o2.page.waitForFunction(() => NH.ASSET && NH.ASSET.done, null, { timeout: 30000 }); await o2.page.evaluate(() => NH.ASSET.done);
  const r6 = await o2.page.evaluate(() => ({ tex: Object.keys(NH.TEXR).length, models: Object.keys(NH.MODELS).length, zinc: !!NH.MAT().zinc.map, photo: NH.WALLU.uPhoto.value, failed: NH.ASSET.failed }));
  ok('missing files fall back to the procedural textures and meshes', r6.tex === 0 && r6.models === 0 && r6.photo === 0 && r6.failed.length <= 2, r6);
  const o2errs = o2.errors.filter(e => !/404|Failed to load resource/.test(e));
  ok('no errors without assets', o2errs.length === 0, o2errs);
  srv.close(); srv2.close(); await b.close(); fs.rmSync(dir, { recursive: true }); fs.rmSync(empty, { recursive: true });
  console.log(fails ? `${fails} FAILED` : 'ALL PASS'); process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
