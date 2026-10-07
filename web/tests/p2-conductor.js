// Phase 2 — danfo loop: passengers, routes, CALL, change, comfort, full bus, missed stops,
// agberos, shift summary, go-slows, and HUD layout with the conductor panel on screen.
const { chromium, open } = require('./harness');
const { VIEWPORTS, layout } = require('./layout');
const results = []; const ok = (name, pass, info) => results.push({ name, pass, info });
(async () => {
  const b = await chromium.launch();
  { const { page, errors } = await open(b);
    // setup helper inside the page: player in Baba Sule's danfo at Oshoja Park
    await page.evaluate(() => {
      window.T2 = {
        bus() { NH.resetWorld(); NH.WORLD.noJams = true; NH.seed(11); const v = NH.spawnVehicle('danfo', 256, 520, 0, { parked: true, parkDanfo: true }); v.driver = 'player'; NH.player.veh = v; return v; },
        at(id, speed = 0) { const st = NH.BUS_STOPS[id], v = NH.player.veh; v.x = st.x; v.y = st.y; v.v = speed; NH.sim(.05); },
        leave() { const v = NH.player.veh; v.x = 66 * 32 + 200; v.y = 55 * 32; v.a = 0; v.v = 120; NH.sim(.05); v.v = 0; }, // drive off onto an open road
        sheet() { const s = document.querySelector('#sheet'); return s.hidden ? '' : s.textContent; },
        click(sel) { const el = document.querySelector(sel); el && el.click(); NH.sim(1 / 60); },
      };
    });
    // route picker
    const picker = await page.evaluate(() => { T2.bus(); NH.pressed.add('KeyE'); NH.sim(1 / 60); return { text: T2.sheet(), routes: [...document.querySelectorAll('#sheet [data-r]')].map(b => b.dataset.r) }; });
    ok('route picker: E in a danfo offers the 3 routes', picker.routes.length === 3 && /Pick a route/.test(picker.text), picker);
    const r = await page.evaluate(() => {
      T2.click('#sheet [data-r="osh-balo"]');
      const S = NH.SHIFT, out = { on: S.on, route: S.route };
      // passengers waiting at every stop, fares 200–500, raised hands (state 'wait')
      out.waiting = Object.entries(S.waiting).map(([k, l]) => [k, l.length, Math.min(...l.map(p => p.fare)), Math.max(...l.map(p => p.fare))]);
      // arrive at Oshoja Park (agbero stop) -> agbero sheet, pay
      T2.at('oshoja'); out.atStop = S.atStop; out.agberoSheet = /Agbero at Oshoja Park/.test(T2.sheet());
      T2.click('#sheet [data-a="pay"]'); out.agberoPaid = S.agbero;
      // panel + CALL
      NH.sim(.05); out.panel = !document.querySelector('#cpanel').hidden; const w0 = S.waiting.oshoja.length, f0 = S.fares;
      NH.pressed.add('KeyE'); NH.sim(1 / 60); out.boarded = S.onboard.length; out.faresUp = S.fares - f0; out.waitDrop = w0 - S.waiting.oshoja.length;
      // change questions: answer the first right, the next "No change"
      const integ0 = NH.S.integ; out.changeQs = S.changeQ.length;
      if (S.changeQ.length) { NH.answerChange(S.changeQ[0].right); }
      if (S.changeQ.length) { const k0 = S.keep; NH.answerChange(-1); out.noChangeKept = S.keep - k0; out.integDrop = integ0 - NH.S.integ; }
      // leave -> next stop index
      T2.leave(); out.idxAfterLeave = S.idx; out.atStopAfter = S.atStop;
      // next stop: Iya Basira; passengers bound here get down (tips possible)
      const due = S.onboard.filter(p => p.dest === 'iya').length; T2.at('iya'); out.alighted = S.carried; out.dueIya = due;
      T2.leave();
      // drive past Second Gate without stopping -> missed
      const st = NH.BUS_STOPS.second, v = NH.player.veh; const pen0 = S.penalties;
      v.x = st.x - 60; v.y = st.y; v.v = 150; NH.sim(.05); v.x = st.x + 220; v.v = 150; NH.sim(.05);
      out.missed = S.missed; out.missPenalty = S.penalties - pen0; out.idxAfterMiss = S.idx;
      // rough driving drops comfort, smooth driving restores it
      if (!S.onboard.length) { S.onboard.push(...S.waiting.marketrd.splice(0, 2)); S.onboard.forEach(p => p.state = 'ride'); }
      NH.clearHeat(false); NH.setVehicles([v]); // just the bus on an empty road: no cops, no traffic to hit
      S.comfort = 100; v.x = 66 * 32 + 200; v.y = 55 * 32; v.a = 0; S.atStop = null; // open road on Lekka Strip, far from any stop
      for (let i = 0; i < 3; i++) { v.x = 66 * 32 + 200; v.y = 55 * 32; v.a = 0; NH.keys.KeyW = true; NH.sim(2.5); NH.keys.KeyW = false; NH.keys.Space = true; NH.sim(.8); NH.keys.Space = false; }
      out.comfortRough = Math.round(S.comfort); NH.sim(4); out.comfortCalm = Math.round(S.comfort);
      return out;
    });
    ok('passengers wait at every route stop with ₦200–₦500 fares', r.on && r.waiting.length === 6 && r.waiting.every(([, n, lo, hi]) => n >= 2 && lo >= 200 && hi <= 500), r.waiting);
    ok('agbero at Oshoja Park demands a ticket; paying records it', r.atStop === 'oshoja' && r.agberoSheet && r.agberoPaid === 400, r);
    ok('CALL loads up to 3, collects fares; panel shows at the stop', r.panel && r.boarded > 0 && r.boarded <= 3 && r.faresUp > 0 && r.waitDrop >= r.boarded - 1, r);
    ok('change: "No change" keeps the money but costs integrity', r.changeQs === 0 || (r.noChangeKept === undefined || (r.noChangeKept > 0 && r.integDrop >= 3)), r);
    ok('leaving a stop moves on to the next one; due passengers get down', r.idxAfterLeave === 1 && r.atStopAfter === null && r.alighted >= r.dueIya, r);
    ok('driving past a stop = missed stop, ₦100 fine (+ refunds)', r.missed === 1 && r.missPenalty >= 100 && r.idxAfterMiss === 3, r);
    ok('rough driving lowers comfort; smooth driving restores it', r.comfortRough < 60 && r.comfortCalm > r.comfortRough, r);

    // full bus bonus, agbero haggle / drive off, summary
    const r2 = await page.evaluate(() => {
      const out = {};
      T2.bus(); NH.startShift('osh-balo'); const S = NH.SHIFT, v = NH.player.veh;
      // full bus at Market Road
      S.idx = 3; const fill = []; while (S.waiting.marketrd.length < 20) S.waiting.marketrd.push(S.waiting.marketrd[0] ? Object.assign({}, S.waiting.marketrd[0], { id: Math.random(), dest: 'marketsq' }) : null);
      T2.at('marketrd'); for (let i = 0; i < 8 && S.onboard.length < NH.CAP; i++) { NH.callPassengers(); while (S.changeQ.length) NH.answerChange(S.changeQ[0].right); }
      out.full = S.onboard.length; const b0 = S.bonus; T2.leave(); out.bonus = S.bonus - b0;
      // Balo Gate agbero: drive off -> heat + doubled demand
      T2.at('balogate'); out.sheet = /Agbero at Balo Gate/.test(T2.sheet()); T2.click('#sheet [data-a="off"]');
      out.stars = NH.stars(); out.demand = S.demand.balogate; NH.clearHeat(false);
      // settle up
      const t = NH.shiftTotals(); const cash0 = NH.S.cash; NH.endShift();
      out.totals = t; out.paid = NH.S.cash - cash0; out.summary = T2.sheet();
      out.sumOk = t.net === t.fares + t.tips + t.bonus + t.keep - t.penalties - t.agbero - t.damage - t.cut && t.cut === Math.round(t.fares * .35);
      NH.closeOverlays(); out.shiftOff = !S.on;
      return out;
    });
    ok('full bus (14/14) pays the ₦500 bonus on departure', r2.full === 14 && r2.bonus === 500, r2);
    ok('driving off from agberos raises heat and doubles their next demand', r2.sheet && r2.stars === 1 && r2.demand === 2, r2);
    ok('shift summary: fares, tips, bonus, agbero, damage, owner\'s cut, take-home', r2.sumOk && r2.paid === Math.max(r2.totals.net, -Infinity) &&
      ['Fares collected', 'Tips', 'Full-bus bonus', 'Agbero tickets', 'Damage to the bus', "Owner's cut", 'Take-home'].every(k => r2.summary.includes(k)) && r2.shiftOff, r2);
    // agbero negotiate: seeded success and failure
    const r3 = await page.evaluate(() => {
      const out = {};
      for (const [seed, cred] of [[3, 400], [4, 0]]) {
        T2.bus(); NH.S.cred = cred; NH.startShift('osh-balo'); NH.seed(seed); T2.at('oshoja'); T2.click('#sheet [data-a="haggle"]');
        const s = T2.sheet(); if (/Pay/.test(s) && document.querySelector('#sheet [data-a="pay"]')) { out[cred ? 'hi' : 'lo'] = { text: s.slice(0, 120), paid: NH.SHIFT.agbero }; T2.click('#sheet [data-a="pay"]'); }
        else out[cred ? 'hi' : 'lo'] = { text: 'closed', paid: NH.SHIFT.agbero };
        out[(cred ? 'hi' : 'lo') + 'Final'] = NH.SHIFT.agbero; NH.closeOverlays(); NH.endShift(); NH.closeOverlays(); NH.S.cred = 0;
      }
      return out;
    });
    ok('agbero negotiation: high cred halves the ticket, a failed haggle raises it', r3.hiFinal === 200 && r3.loFinal === 600, r3);
    ok('no JS errors (conductor flow)', !errors.length, errors);
    await page.context().close();
  }
  // ---------- go-slow ----------
  { const { page, errors } = await open(b);
    const r = await page.evaluate(() => {
      NH.resetWorld(); NH.WORLD.noJams = true; NH.seed(21); NH.player.veh = null; NH.player.x = 700; NH.player.y = 700; NH.S.minutes = 600;
      NH.sim(20); // traffic fills in
      const road = NH.MAIN_ROADS.find(r => r.name === 'Market Road'); NH.startJam(road, 120);
      let jam = 0, jn = 0, free = 0, fn = 0;
      NH.sim(40, () => { for (const v of NH.vehicles()) if (v.driver === 'ai' && v.mode === 'lane') { if (NH.jammed(v.from, v.to)) { jam += Math.abs(v.v); jn++; } else { free += Math.abs(v.v); fn++; } } });
      const during = { jamSpeed: Math.round(jam / Math.max(1, jn)), freeSpeed: Math.round(free / Math.max(1, fn)), jamSamples: jn, onMinimap: NH.jams().length };
      NH.S.minutes += 120; NH.sim(70); // jam over; let it drain
      const stuck = NH.vehicles().filter(v => v.driver === 'ai' && (v.waitT || 0) > 8).length;
      return Object.assign(during, { after: NH.jams().length, stuck });
    });
    ok('go-slow: traffic crawls on the jammed road, flows elsewhere', r.jamSamples > 200 && r.jamSpeed < 30 && r.freeSpeed > 50 && r.onMinimap === 1, r);
    ok('go-slow clears and traffic drains (nobody stuck afterwards)', r.after === 0 && r.stuck === 0, r);
    ok('no JS errors (go-slow)', !errors.length, errors);
    await page.context().close();
  }
  // ---------- HUD layout with the conductor panel + change question ----------
  for (const vp of VIEWPORTS) {
    const { page, errors } = await open(b, vp);
    await page.evaluate(() => { NH.resetWorld(); NH.WORLD.noJams = true; NH.seed(8); const v = NH.spawnVehicle('danfo', 256, 520, 0, { parked: true, parkDanfo: true }); v.driver = 'player'; NH.player.veh = v;
      NH.startShift('mainland'); const st = NH.BUS_STOPS.ebute; v.x = st.x; v.y = st.y; v.v = 0; NH.sim(.1);
      for (let i = 0; i < 3 && !NH.SHIFT.changeQ.length; i++) { NH.SHIFT.waiting.ebute.push(...NH.SHIFT.waiting.church.splice(0, 2).map(p => Object.assign(p, { note: 1000 }))); NH.callPassengers(); }
      NH.sim(.1); NH.ui.toast('Ebute Corner: 4 waiting.'); });
    await page.waitForTimeout(300); await page.evaluate(() => NH.sim(1 / 60));
    const L = await layout(page); const vis = await page.evaluate(() => ({ panel: !document.querySelector('#cpanel').hidden, q: !document.querySelector('#cp-change').hidden }));
    ok(`layout ${vp.name}: conductor panel + change question`, vis.panel && vis.q && !L.overlaps.length && !L.off.length && !L.hscroll, Object.assign(L, vis));
    await page.screenshot({ path: `/tmp/claude-0/shots/p2-${vp.width}x${vp.height}.png` });
    ok(`no JS errors ${vp.name}`, !errors.length, errors);
    await page.context().close();
  }
  await b.close();
  let fails = 0; for (const t of results) { if (!t.pass) fails++; console.log((t.pass ? 'PASS ' : 'FAIL ') + t.name + (t.pass ? '' : '  ' + JSON.stringify(t.info))); }
  console.log(fails ? `${fails} FAILED` : 'ALL PASS'); process.exit(fails ? 1 : 0);
})();
