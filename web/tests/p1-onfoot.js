// Phase 1.1 (cont.) — player on foot can't walk into buildings/stalls; getting out never lands
// inside a wall; a head-on crash bounces the vehicle back with minor damage.
const { chromium, open } = require('./harness');
(async () => {
  const b = await chromium.launch(); const { page, errors } = await open(b);
  const r = await page.evaluate(() => {
    const { T } = NH; const out = { walkInside: 0, walkFrames: 0, exitInside: 0, exits: 0 };
    let seed = 3; const rand = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
    const dirs = [['KeyW'], ['KeyS'], ['KeyA'], ['KeyD'], ['KeyW', 'KeyA'], ['KeyS', 'KeyD']];
    for (let i = 0; i < 60; i++) {
      NH.player.veh = null; let x, y;
      do { x = 40 + rand() * 3000; y = 40 + rand() * 1960; } while (!NH.playerFree(x, y) || ![T.SIDE, T.GROUND].includes(NH.tileAt(x, y)));
      NH.player.x = x; NH.player.y = y; const ks = dirs[i % dirs.length];
      ks.forEach(k => NH.keys[k] = true); NH.keys.ShiftLeft = true;
      NH.sim(3, () => { out.walkFrames++; if (NH.solidAt(NH.player.x, NH.player.y) || !NH.playerFree(NH.player.x, NH.player.y)) out.walkInside++; });
      ks.forEach(k => NH.keys[k] = false); NH.keys.ShiftLeft = false;
    }
    // exit next to walls: park vehicles hugging buildings and get out
    for (let i = 0; i < 40; i++) {
      NH.setVehicles([]); let x, y;
      do { x = 40 + rand() * 3000; y = 40 + rand() * 1960; } while (NH.tileAt(x, y) !== T.ROAD || NH.tileAt(x + 40, y) === T.ROAD);
      const v = NH.spawnVehicle(['sedan', 'danfo', 'okada'][i % 3], x, y, rand() * 6.28, { parked: true });
      v.driver = 'player'; NH.player.veh = v; NH.exitVehicle();
      if (!NH.player.veh) { out.exits++; if (!NH.playerFree(NH.player.x, NH.player.y)) out.exitInside++; }
    }
    // head-on crash: aim a sedan at a building from 120px, full throttle
    NH.setVehicles([]); NH.player.veh = null;
    let crash = null;
    for (let tries = 0; tries < 200 && !crash; tries++) {
      const x = 40 + rand() * 3000, y = 40 + rand() * 1960;
      if (NH.tileAt(x, y) !== T.ROAD || NH.tileAt(x + 140, y) !== T.BUILD) continue;
      if ([20, 60, 100].some(dx => NH.tileAt(x + dx, y - 9) !== T.ROAD && NH.tileAt(x + dx, y - 9) !== T.SIDE || NH.tileAt(x + dx, y + 9) !== T.ROAD && NH.tileAt(x + dx, y + 9) !== T.SIDE)) continue;
      const v = NH.spawnVehicle('sedan', x, y, 0, { parked: true }); v.driver = 'player'; NH.player.veh = v;
      const hp0 = v.hp; let minV = 0, maxV = 0; NH.keys.KeyW = true;
      NH.sim(2.5, () => { maxV = Math.max(maxV, v.v); minV = Math.min(minV, v.v); if (v.v < 0) NH.keys.KeyW = false; });
      NH.keys.KeyW = false;
      crash = { topSpeed: Math.round(maxV), bouncedBack: minV < -5, hpLost: Math.round(hp0 - v.hp), inside: NH.solidAt(v.x, v.y) };
    }
    out.crash = crash; return out;
  });
  console.log(JSON.stringify(r)); console.log('errors:', errors.length ? errors : 'none');
  const pass = r.walkInside === 0 && r.exitInside === 0 && r.exits > 30 && r.crash && r.crash.bouncedBack && r.crash.hpLost > 0 && r.crash.hpLost < 40 && !r.crash.inside && !errors.length;
  console.log(pass ? 'PASS' : 'FAIL'); await b.close(); process.exit(pass ? 0 : 1);
})();
