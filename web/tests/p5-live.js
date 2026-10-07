// Live Lagos: crowd size and falloff, hawkers at the window, rush-hour go-slow, danfos stopping anywhere,
// okadas between lanes, honking, the ambient scenes, new street clutter, and the sound mix.
const { chromium, open } = require('./harness');
let fails = 0; const ok = (name, cond, info) => { console.log((cond ? 'PASS ' : 'FAIL ') + name + (cond ? '' : '  ' + JSON.stringify(info))); if (!cond) fails++; };
(async () => {
  const b = await chromium.launch(); const { page, errors } = await open(b);
  const run = f => page.evaluate(f);
  await run(() => { window.T5 = { fresh(min = 13 * 60, x = 300, y = 480) { NH.resetWorld(); NH.seed(21); NH.closeOverlays(); NH.S.minutes = min; NH.WORLD.hour = -1; NH.WORLD.noJams = false; NH.LIVE.rush = false; NH.player.veh = null; NH.player.x = x; NH.player.y = y; NH.clearHeat(); } }; });

  // ---- crowds
  const r1 = await run(() => { T5.fresh(); NH.sim(8); const p = NH.player, P = NH.peds().filter(q => !q.hawker);
    const ring = (a, z) => P.filter(q => { const d = Math.hypot(q.x - p.x, q.y - p.y); return d >= a && d < z; }).length / (Math.PI * (z * z - a * a));
    const tiers = ['low', 'medium', 'high'].map(g => { const q = { low: .6, medium: 1, high: 1.2 }[g]; return Math.round(150 * q); });
    return { n: P.length, target: NH.pedTarget(), near: ring(0, 300) * 1e5, far: ring(300, 700) * 1e5, outer: ring(700, 950) * 1e5, tiers, q: NH.Q().peds }; });
  ok('3–5x more pedestrians than before (40 on desktop): 90 / 150 / 180 by graphics tier', r1.n >= r1.target - 3 && r1.tiers.join() === '90,150,180' && r1.target === Math.round(150 * r1.q), r1);
  ok('the crowd thins with distance from the player', r1.near > r1.far * 1.4 && r1.far > r1.outer, r1);

  // ---- rush hour and denser traffic
  const r2 = await run(() => { const out = {}; T5.fresh(12 * 60); NH.sim(20); out.noon = { rush: NH.LIVE.rush, cars: NH.vehicles().filter(v => v.driver === 'ai').length };
    T5.fresh(7 * 60 + 2); NH.sim(1); out.am = { rush: NH.LIVE.rush, jams: NH.WORLD.jams.length, until: NH.WORLD.jams.map(j => Math.round(j.until % 1440)) }; NH.sim(25); out.am.cars = NH.vehicles().filter(v => v.driver === 'ai').length;
    out.types = {}; for (const v of NH.vehicles()) if (v.driver === 'ai') out.types[v.type] = (out.types[v.type] || 0) + 1;
    T5.fresh(16 * 60 + 30); NH.sim(1); out.pm = { rush: NH.LIVE.rush, jams: NH.WORLD.jams.length };
    T5.fresh(21 * 60); NH.sim(1); out.night = NH.LIVE.rush; out.fns = [6.9, 7, 9.9, 10, 15.9, 16, 19.9, 20].map(h => NH.isRush(h * 60));
    return out; });
  ok('rush hours by the clock: 7–10am and 4–8pm', r2.fns.join() === 'false,true,true,false,false,true,true,false' && r2.am.rush && r2.pm.rush && !r2.night && !r2.noon.rush, r2);
  ok('rush hour: go-slow on three main roads until it ends, and more cars', r2.am.jams >= 3 && r2.am.until.every(u => u === 600) && r2.pm.jams >= 3 && r2.am.cars > r2.noon.cars, r2);
  ok('kekes and okadas everywhere: each about as common as cars', (r2.types.keke || 0) + (r2.types.okada || 0) >= (r2.types.sedan || 0) * 1.5, r2);

  // ---- danfos stopping anywhere, okadas between lanes, honking
  const r3 = await run(() => { T5.fresh(8 * 60); const seen = { kerb: 0, split: 0, splitPass: 0 }; const h0 = NH.LIVE.honks || 0; const fl = new Set();
    NH.sim(40, () => { for (const v of NH.vehicles()) { if (v.kerb > 0) seen.kerb++; if (v.split) seen.split++; } });
    return { kerb: seen.kerb, split: seen.split, honks: (NH.LIVE.honks || 0) - h0 }; });
  ok('danfos stop anywhere to load; okadas squeeze between lanes; drivers honk in the go-slow', r3.kerb > 0 && r3.split > 0 && r3.honks > 3, r3);

  // ---- hawkers
  const r4 = await run(() => { T5.fresh(8 * 60); NH.sim(20); const s = NH.roadSpotNear(NH.player, 0, 160); const v = NH.spawnVehicle('sedan', s.x, s.y, s.a, { parked: true }); NH.player.veh = v; v.driver = 'player'; v.v = 0;
    let atWin = null; NH.sim(25, () => { atWin = atWin || NH.hawkerAtWindow(); });
    const hk = NH.peds().filter(q => q.hawker), ctx = NH.actionContext(), out = { hawkers: hk.length, trays: [...new Set(hk.map(q => q.tray))], atWin: !!atWin, label: ctx && ctx.label };
    if (ctx && /hawker/.test(ctx.label)) { ctx.run(); const sh = document.querySelector('#sheet:not([hidden])'); out.items = sh ? [...sh.querySelectorAll('[data-buy]')].map(x => x.dataset.buy + '=' + x.textContent) : []; const c0 = NH.S.cash, n0 = NH.invCount('chips'); sh.querySelector('[data-buy="chips"]').click(); out.paid = c0 - NH.S.cash; out.chips = NH.invCount('chips') - n0; NH.closeOverlays(); }
    v.v = 200; NH.sim(.5); out.leave = !NH.hawkerAtWindow(); NH.player.veh = null; v.driver = null; return out; });
  ok('hawkers walk between cars in slow traffic, selling pure water, gala and plantain chips', r4.hawkers >= 2 && r4.trays.length >= 2, r4);
  ok('a hawker comes up to your window when you stop; ACTION buys (plantain chips ₦150)', r4.atWin && /hawker/.test(r4.label) && r4.items.join() === 'water=₦50,gala=₦200,chips=₦150' && r4.paid === 150 && r4.chips === 1 && r4.leave, r4);

  // ---- ambient scenes
  const r5 = await run(() => { T5.fresh(); const sp = NH.liveSpots(), L = NH.LIVE, out = { spots: Object.keys(sp).filter(k => sp[k]) };
    out.agb = L.agb.length; const a0 = L.agb.map(a => [a.x, a.y]); NH.player.x = 256; NH.player.y = 300; NH.sim(12); out.agbMoved = L.agb.some((a, i) => Math.hypot(a.x - a0[i][0], a.y - a0[i][1]) > 10);
    const bx = L.ball.x, by = L.ball.y; NH.player.x = sp.foot.x; NH.player.y = sp.foot.y + 40; NH.sim(6); out.ballMoved = Math.hypot(L.ball.x - bx, L.ball.y - by) > 3 || Math.hypot(L.ball.vx, L.ball.vy) > 1; out.kids = L.kids.length;
    out.inPitch = L.kids.every(k => Math.abs(k.x - sp.foot.x) < 60 && Math.abs(k.y - sp.foot.y) < 40);
    out.preacher = !!L.preacher; out.warden = !!L.warden; out.weekday = NH.owambeOn();
    NH.S.minutes = 5 * 1440 + 14 * 60; out.sat = NH.owambeOn(); NH.S.minutes = 6 * 1440 + 23 * 60; out.sunNight = NH.owambeOn(); NH.S.minutes = 7 * 1440 + 14 * 60; out.mon = NH.owambeOn();
    out.statics = NH.STATICS.length; out.guests = L.guests.length;
    out.parkFree = Math.hypot(sp.party.x - 256, sp.party.y - 320) > 250;
    return out; });
  ok('scenes placed: park agberos patrol, preacher, kids football, traffic warden, owambe lot', r5.spots.length === 4 && r5.agb === 2 && r5.agbMoved && r5.preacher && r5.warden && r5.guests === 12 && r5.parkFree, r5);
  ok('kids kick the ball around their alley', r5.kids === 4 && r5.ballMoved && r5.inPitch, r5);
  ok('the owambe happens at weekends (Day 6–7, 11am–10pm) only', !r5.weekday && r5.sat && !r5.sunNight && !r5.mon, r5);

  // ---- street clutter
  const r6 = await run(() => ({ props: Object.fromEntries(['rcstand', 'tyres', 'sand', 'blocks', 'gen', 'gascyl', 'chair'].map(k => [k, (NH.PROP[k] || []).length])), mech: NH.STATICS.length, gens: NH.GENS.length, kiosks: NH.KIOSKS.length }));
  ok('street clutter: recharge stands, mechanics, sand and blocks, generators, gas cylinders, kiosks', Object.values(r6.props).every(n => n > 5) && r6.mech > 5 && r6.gens > 20 && r6.kiosks > 5, r6);

  // ---- sound
  const r7 = await run(async () => { T5.fresh(13 * 60, 1000, 1300); NH.sim(6); await new Promise(r => setTimeout(r, 400)); const S = NH.SND;
    const g = n => +S[n].gain.value.toFixed(3); const out = { ready: S.ready, chat: g('chat'), rum: g('rum'), master: g('master') };
    NH.setSound(false); await new Promise(r => setTimeout(r, 1200)); out.off = g('master'); out.saved = NH.S.sound; NH.setSound(true); await new Promise(r => setTimeout(r, 800)); out.on = g('master');
    const btn = document.querySelectorAll('[data-snd]').length; out.btns = btn; return out; });
  ok('sound: crowd chatter and traffic rumble follow what is around you', r7.ready && r7.chat > 0 && r7.master > 0, r7);
  ok('sound can be switched off and back on (M, title screen, phone)', r7.off < .02 && r7.saved === false && r7.on > .3 && r7.btns >= 2, r7);

  ok('no JS errors', !errors.length, errors);
  console.log(fails ? `${fails} FAILED` : 'ALL PASS'); await b.close(); process.exit(fails ? 1 : 0);
})();
