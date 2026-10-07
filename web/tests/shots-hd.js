// Screenshots of the HD world materials: shop interiors and glass, ground decals, god rays at sunset, heat
// shimmer at noon (manual look): OUT=dir GFX=high node shots-hd.js
const { chromium, open } = require('./harness');
const OUT = process.env.OUT || '/tmp/claude-0/shots';
(async () => {
  const b = await chromium.launch();
  const { page, errors } = await open(b, { width: 1280, height: 760 }, { render3d: true });
  await page.evaluate(g => NH.applyGfx(g), process.env.GFX || 'high');
  const shot = async (name, f, frames = 4) => { await page.evaluate(f); await page.evaluate(n => { for (let i = 0; i < n; i++) NH.renderOnce(i / 30); }, frames); await page.evaluate(() => { if (NH.PASS.mblur) NH.PASS.mblur.enabled = false; NH.renderOnce(0); }); await page.screenshot({ path: `${OUT}/hd-${name}.png` }); };
  const at = (x, y, yaw, min = 12.5 * 60) => `NH.resetWorld(); NH.seed(5); NH.S.minutes = ${min}; NH.WORLD.hour = -1; NH.player.veh = null; NH.player.x = ${x}; NH.player.y = ${y}; NH.CAMS.yaw = ${yaw}; NH.CAMS.dragT = performance.now() + 1e9; NH.sim(4);`;
  const spots = (process.env.SPOTS || '').split(';').filter(Boolean);
  if (spots.length) { for (const s of spots) { const [n, x, y, yaw, min] = s.split(','); await shot(n, new Function(at(+x, +y, +yaw, +(min || 750)))); } }
  else {
    // stand 5 m out from a shop front, looking at it (shop fronts are quads in the chunks' 'front' meshes)
    const fronts = await page.evaluate(() => { const out = []; NH.scene().traverse(o => { if (o.isMesh && o.material === NH.MAT().front) { const P = o.geometry.attributes.position.array, N = o.geometry.attributes.normal.array;
      const U = o.geometry.attributes.uv.array; for (let q = 0; q < P.length / 18; q++) { let x = 0, z = 0, u = 0, w = 0; for (let v = 0; v < 6; v++) { x += P[q * 18 + v * 3] / 6; z += P[q * 18 + v * 3 + 2] / 6; u += U[q * 12 + v * 2] / 6; w += U[q * 12 + v * 2 + 1] / 6; }
        const slot = Math.floor(u * 2) + 2 * (1 - Math.floor(w * 2)); if (slot === 1 || slot === 2) out.push([x, z, N[q * 18], N[q * 18 + 2], slot]); } } }); return out.filter((f, i) => i % 5 === 0); });
    console.log('interior fronts', fronts.length);
    const view = (k, min, d = 5, side = 0) => { const [x, z, nx, nz] = fronts[k % fronts.length]; return at((x + nx * d + nz * side) * 8, (z + nz * d - nx * side) * 8, Math.atan2(-nz, -nx), min); };
    for (const k of [1, 4, 7]) await shot('shops' + k, new Function(view(k, 750, 6, 1.5)));
    await shot('shops-night', new Function(view(4, 21 * 60, 6, 1.5)));
    // a long road: god rays looking west into the low sun, and heat shimmer at noon
    const road = (yaw, min) => at(1500, 25 * 32 + 16, yaw, min) + ' const s = NH.roadSpotNear(NH.player, 0, 200); NH.player.x = s.x; NH.player.y = s.y; NH.sim(1);';
    const run = ' NH.keys.ShiftLeft = true; NH.keys.KeyW = true; NH.sim(1.2); NH.keys.ShiftLeft = false; NH.keys.KeyW = false;';
    for (const [i, yaw] of [Math.PI, Math.PI * .85, Math.PI * 1.15].entries()) await shot('sunset' + i, new Function(road(yaw, 18 * 60 + 15) + run), 1);
    await shot('sunrise', new Function(road(0, 6 * 60 + 50) + run), 1);
    await shot('noon-road', new Function(road(Math.PI, 13 * 60) + run), 1);
    await shot('decals', new Function(road(Math.PI / 2, 12 * 60)));
  }
  const fx = await page.evaluate(() => { const p = NH.PASS.atmos; return p ? { on: p.enabled, ray: p.uniforms.uRay.value, heat: p.uniforms.uHeat.value } : null; });
  console.log(JSON.stringify(fx), errors.length ? errors : 'no errors'); await b.close();
})();
