// Phase 1.1 — vehicles must never end up inside buildings or stalls.
const { chromium, open } = require('./harness');
(async () => {
  const b = await chromium.launch(); const { page, errors } = await open(b);
  const r = await page.evaluate(() => {
    const { T, VT } = NH; const res = { trials: 0, embeddedFrames: 0, frames: 0, worstTrial: null, aiEmbedded: 0, aiFrames: 0 };
    const types = ['sedan', 'danfo', 'okada', 'suv', 'keke'];
    let seed = 7; const rand = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
    const inside = v => NH.solidAt(v.x, v.y) ? 1 : 0;
    for (let trial = 0; trial < 80; trial++) {
      NH.setVehicles([]); NH.player.veh = null;
      // pick a road cell that borders a building or stall
      let x, y;
      for (;;) { x = 40 + rand() * (3072 - 80); y = 40 + rand() * (2048 - 80);
        if (NH.tileAt(x, y) !== T.ROAD) continue;
        if ([[48,0],[-48,0],[0,48],[0,-48]].some(([dx,dy]) => [T.BUILD, T.STALL, T.TOWER].includes(NH.tileAt(x+dx, y+dy)))) break; }
      const type = types[trial % types.length];
      const v = NH.spawnVehicle(type, x, y, rand() * Math.PI * 2, { parked: true });
      v.driver = 'player'; NH.player.veh = v; v.hp = 1e9;
      NH.keys.KeyW = true; let frames = 0, bad = 0;
      NH.sim(4, () => {
        frames++;
        if (frames % 40 === 0) { NH.keys.KeyA = rand() < .4; NH.keys.KeyD = !NH.keys.KeyA && rand() < .5; NH.keys.KeyS = rand() < .15; NH.keys.KeyW = !NH.keys.KeyS; }
        bad += inside(v);
        for (const o of NH.vehicles()) if (o !== v) { res.aiFrames++; res.aiEmbedded += inside(o); }
      });
      for (const k of ['KeyW','KeyA','KeyS','KeyD']) NH.keys[k] = false;
      res.trials++; res.frames += frames; res.embeddedFrames += bad;
      if (bad && (!res.worstTrial || bad > res.worstTrial.bad)) res.worstTrial = { type, bad, x: Math.round(x), y: Math.round(y) };
    }
    return res;
  });
  console.log(JSON.stringify(r)); console.log('errors:', errors.length ? errors : 'none');
  const pass = r.embeddedFrames === 0 && r.aiEmbedded === 0 && !errors.length;
  console.log(pass ? 'PASS' : 'FAIL'); await b.close(); process.exit(pass ? 0 : 1);
})();
