// HD world materials: tier-sized procedural atlases, generated normal maps, glass that reflects, shop interiors,
// ground decals (oil, tyre marks, cracks, stains), god rays at sunrise/sunset and heat shimmer at noon.
const { chromium, open } = require('./harness');
let fails = 0; const ok = (name, cond, info) => { console.log((cond ? 'PASS ' : 'FAIL ') + name + (cond ? '' : '  ' + JSON.stringify(info))); if (!cond) fails++; };
(async () => {
  const b = await chromium.launch();
  const res = {};
  for (const tier of ['medium', 'high']) { // the atlas size is fixed when the page loads, so load once per tier
    const { page, errors } = await open(b, { width: 800, height: 500 }, { render3d: true, init: `try { localStorage.setItem('naijahustle.gfx', '${tier}'); } catch (e) {}` });
    res[tier] = await page.evaluate(() => { const M = NH.MAT(), img = t => t && t.image ? t.image.width : 0;
      NH.resetWorld(); NH.player.x = 900; NH.player.y = 560; NH.S.minutes = 13 * 60; NH.WORLD.hour = -1; NH.sim(1); NH.renderOnce(0);
      let gd = 0; NH.scene().traverse(o => { if (o.isMesh && o.material === M.gdecal) gd += o.geometry.attributes.position.count / 6; });
      return { gfx: NH.gfx(), texk: NH.TEXK, house: img(M.house0.map), front: img(M.front.map), decal: img(M.decal.map), ground: img(M.gdecal.map),
        normals: ['house0', 'house1', 'house2', 'conc', 'tile', 'terrain', 'zinc', 'gdecal'].filter(k => M[k].normalMap).length, terrainCh: M.terrain.normalMap && M.terrain.normalMap.channel,
        glass: !!M.house0.roughnessMap && M.house0.roughness === 1, interior: M.front.customProgramCacheKey().includes('interior'), gd, atmos: !!NH.PASS.atmos }; });
    res[tier].errors = errors.slice(); if (tier === 'high') res.page = page; else await page.context().close();
  }
  const m = res.medium, h = res.high;
  ok('procedural atlases are drawn at twice the size on High (facades 1024, shop fronts 2048, decals 2048)', m.texk === 1 && h.texk === 2 && m.house === 512 && h.house === 1024 && m.front === 1024 && h.front === 2048 && h.decal === 2048 && h.ground === 2048, { m, h });
  ok('normal maps on walls, concrete, tiles, roads (world-tiled), zinc and ground decals', h.normals === 8 && h.terrainCh === 1, h);
  ok('window glass is smooth (roughness map) so it reflects the sky; shop fronts use the interior shader', h.glass && h.interior, h);
  ok('ground decals: hundreds of oil spills, tyre marks, cracks and stains in the chunk meshes', h.gd > 300 && h.gd === m.gd, { h: h.gd, m: m.gd });

  const page = res.page, run = f => page.evaluate(f);
  const fx = await run(() => { const p = NH.PASS.atmos, u = p.uniforms, out = {};
    const look = (min, yaw) => { NH.S.minutes = min; NH.WORLD.hour = -1; NH.CAMS.yaw = yaw; NH.CAMS.dragT = performance.now() + 1e9; for (let i = 0; i < 30; i++) { NH.sim(1 / 15); NH.renderOnce(i / 15); } return { on: p.enabled, ray: +u.uRay.value.toFixed(2), heat: +u.uHeat.value.toFixed(2), sun: u.uSun.value.toArray().map(v => +v.toFixed(2)) }; };
    out.noon = look(13 * 60, Math.PI); out.sunsetWest = look(18 * 60 + 15, Math.PI); out.sunsetEast = look(18 * 60 + 15, 0); out.morning = look(9 * 60 + 30, Math.PI); out.night = look(22 * 60, Math.PI);
    return out; });
  ok('heat shimmer at noon; god rays looking into the low sun at sunset, not with it behind you', fx.noon.on && fx.noon.heat > .8 && fx.noon.ray === 0 && fx.sunsetWest.ray > .2 && fx.sunsetEast.ray === 0, fx);
  ok('neither effect mid-morning or at night (the pass switches off)', !fx.morning.on && !fx.night.on, fx);
  const night = await run(() => { NH.S.minutes = 22 * 60; NH.WORLD.hour = -1; NH.sim(.2); return NH.INTU.uNight.value; });
  ok('shop interiors light up at night', night > .9, night);
  ok('no errors (medium, high)', !m.errors.length && !h.errors.length, [m.errors, h.errors]);
  await b.close(); console.log(fails ? `${fails} FAILED` : 'ALL PASS'); process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
