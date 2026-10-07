// Regression: existing gameplay still works after Phase 1 (missions, chase, checkpoint, phone, save).
const { chromium, open } = require('./harness');
(async () => {
  const b = await chromium.launch(); const { page, errors } = await open(b);
  const r = await page.evaluate(() => {
    const out = {}; NH.resetWorld(); NH.seed(5);
    const go = (x, y) => { const o = NH.player.pos(); o.x = x; o.y = y; if (o.v != null) o.v = 0; NH.sim(.3); };
    // lag_01: meet Baba Driver, board the bay-1 danfo, run the 3-stop route, bring it home, collect
    NH.player.x = 210; NH.player.y = 410; out.start1 = NH.startMission('lag_01');
    go(256, 330); out.dialog = NH.DLG.open; NH.dlgSkip();
    const d = NH.MS.cur.danfo; NH.player.x = d.x + 20; NH.player.y = d.y; NH.pressed.add('KeyF'); NH.sim(.1);
    out.inDanfo = NH.player.veh === d;
    for (const id of NH.FIRST_ROUTE.stops) { const st = NH.BUS_STOPS[id]; go(st.x, st.y); NH.sim(.2);
      const pay = document.querySelector('#sheet:not([hidden]) [data-a="pay"]'); if (pay) pay.click();
      NH.callPassengers(); NH.sim(2.5); NH.depart(); NH.sim(.1); }
    go(256, 330); NH.dlgSkip(); const collect = document.querySelector('#sheet:not([hidden]) [data-x]'); if (collect) collect.click();
    out.lag01 = NH.S.done.includes('lag_01');
    // lag_02: chase the pickpocket on an okada and return the bag
    NH.pressed.add('KeyF'); NH.sim(.1); out.start2 = NH.startMission('lag_02');
    const ok = NH.MS.cur.okada; NH.player.x = ok.x + 15; NH.player.y = ok.y; NH.pressed.add('KeyF'); NH.sim(.1);
    NH.sim(.2); const th = NH.MS.cur.thief; out.thiefFlees = !!th && th.driver === 'flee';
    NH.sim(1); const t2 = NH.MS.cur.thief; go(t2.x + 20, t2.y); out.caught = NH.MS.cur && NH.MS.cur.oi === 2;
    go(NH.MS.cur ? 31 * 32 + 16 : 0, 28 * 32 + 16); out.lag02 = NH.S.done.includes('lag_02');
    // police: heat brings Task Force, stays chasing, decays when hidden
    NH.addHeat(2); NH.sim(4); out.cops = NH.vehicles().filter(v => v.driver === 'cop').length; out.stars = NH.stars();
    out.cash = NH.S.cash; out.chats = NH.S.chats.length; out.feed = NH.S.feed.length;
    return out;
  });
  console.log(JSON.stringify(r)); console.log('errors:', errors.length ? errors : 'none');
  const pass = r.start1 && r.dialog && r.inDanfo && r.lag01 && r.start2 && r.thiefFlees && r.caught && r.lag02 && r.cops > 0 && r.stars === 2 && !errors.length;
  console.log(pass ? 'PASS' : 'FAIL'); await b.close(); process.exit(pass ? 0 : 1);
})();
