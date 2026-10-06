// Phase 1.2 — traffic: no overlaps, no rear-ending, give way at junctions, traffic still flows.
const { chromium, open } = require('./harness');
(async () => {
  const b = await chromium.launch(); const { page, errors } = await open(b);
  const r = await page.evaluate(() => {
    const { VT, nodes } = NH;
    // Oriented-rectangle overlap (SAT), shrunk 2px so touching bumpers don't count.
    const corners = v => { const d = VT[v.type], c = Math.cos(v.a), s = Math.sin(v.a), hl = d.len / 2 - 2, hw = d.wid / 2 - 2;
      return [[hl, hw], [hl, -hw], [-hl, -hw], [-hl, hw]].map(([f, w]) => [v.x + c * f - s * w, v.y + s * f + c * w]); };
    const overlap = (A, B) => { const a = corners(A), b2 = corners(B);
      for (const poly of [a, b2]) for (let i = 0; i < 4; i++) { const [x1, y1] = poly[i], [x2, y2] = poly[(i + 1) % 4]; const nx = y1 - y2, ny = x2 - x1;
        const pa = a.map(([x, y]) => x * nx + y * ny), pb = b2.map(([x, y]) => x * nx + y * ny);
        if (Math.max(...pa) < Math.min(...pb) || Math.max(...pb) < Math.min(...pa)) return false; } return true; };
    const runs = []; const spots = [[300, 430], [1300, 700], [2300, 600], [990, 1250]];
    for (const extra of [0, 1, 2, 3, 4]) for (const [px, py] of spots) {
      NH.seed(px * 7 + py + extra * 7919); NH.resetWorld(); NH.WORLD.noJams = true; NH.player.veh = null; NH.player.x = px; NH.player.y = py; NH.S.minutes = 12 * 60;
      if (NH.tileAt(px, py) === NH.T.ROAD) throw new Error('test spot is on the road: ' + px + ',' + py);
      const st = { frames: 0, overlapFrames: 0, pairsOverlapping: 0, aiCrashes: 0, junctionConflicts: 0, avgSpeed: 0, samples: 0, stuck: 0 };
      NH.stats.aiCrashes = 0;
      NH.sim(60, () => {
        const ai = NH.vehicles().filter(v => v.driver === 'ai'); st.frames++;
        let any = false;
        for (let i = 0; i < ai.length; i++) for (let j = i + 1; j < ai.length; j++) {
          const A = ai[i], B = ai[j]; if (Math.abs(A.x - B.x) > 60 || Math.abs(A.y - B.y) > 60) continue;
          if (overlap(A, B)) { st.pairsOverlapping++; any = true; } }
        if (any) st.overlapFrames++;
        // junction conflict: two moving AI cars crossing paths inside one junction box (perpendicular, <30px apart)
        for (const n of nodes) { const inBox = ai.filter(v => Math.abs(v.x - n.x) < 32 && Math.abs(v.y - n.y) < 32 && Math.abs(v.v) > 15);
          for (let i = 0; i < inBox.length; i++) for (let j = i + 1; j < inBox.length; j++) if (Math.abs(Math.cos(inBox[i].a - inBox[j].a)) < .5 && Math.hypot(inBox[i].x - inBox[j].x, inBox[i].y - inBox[j].y) < 30) st.junctionConflicts++; }
        if (st.frames % 30 === 0) for (const v of ai) { st.samples++; st.avgSpeed += Math.abs(v.v); }
      });
      st.aiCrashes = NH.stats.aiCrashes; st.avgSpeed = Math.round(st.avgSpeed / Math.max(1, st.samples));
      st.cars = NH.vehicles().filter(v => v.driver === 'ai').length;
      const stuckCars = NH.vehicles().filter(v => v.driver === 'ai' && (v.waitT || 0) > 8);
      st.stuck = stuckCars.length;
      runs.push(st);
    }
    return runs;
  });
  const tot = k => r.reduce((a, x) => a + x[k], 0);
  console.log(r.map(x => JSON.stringify(x)).join('\n'));
  console.log('totals: overlapFrames', tot('overlapFrames'), 'aiCrashes', tot('aiCrashes'), 'junctionConflicts', tot('junctionConflicts'), 'avgSpeed', Math.round(tot('avgSpeed') / r.length), 'stuck', tot('stuck'));
  console.log('errors:', errors.length ? errors : 'none');
  const pass = tot('overlapFrames') === 0 && tot('aiCrashes') === 0 && tot('junctionConflicts') === 0 && tot('avgSpeed') / r.length > 60 && tot('stuck') === 0 && !errors.length;
  console.log(pass ? 'PASS' : 'FAIL'); await b.close(); process.exit(pass ? 0 : 1);
})();
