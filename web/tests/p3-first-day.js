// "First Day on the Danfo", end to end: Baba Driver's dialogue, the bay-1 danfo, three stops with
// passengers boarding and getting down, mood, the agbero at Yaba, the summary with Baba's 40%
// cut, fail + retry (wrecked, late), and the conductor-shift side job unlocking afterwards.
const { chromium, open } = require('./harness');
let fails = 0; const ok = (name, cond, info) => { console.log((cond ? 'PASS ' : 'FAIL ') + name + (cond ? '' : '  ' + JSON.stringify(info))); if (!cond) fails++; };
(async () => {
  const b = await chromium.launch(); const { page, errors } = await open(b);
  const run = f => page.evaluate(f);
  // helpers inside the page
  await run(() => {
    window.T3 = {
      go(x, y) { const o = NH.player.pos(); o.x = x; o.y = y; if (o.v != null) o.v = 0; NH.sim(.3); },
      sheet: () => document.querySelector('#sheet:not([hidden])'),
      click(sel) { const e = document.querySelector('#sheet:not([hidden]) ' + sel); if (e) e.click(); return !!e; },
      start() { NH.resetWorld(); NH.seed(11); NH.S.done = NH.S.done.filter(x => x !== 'lag_01'); NH.S.minutes = 9 * 60; NH.player.veh = null; NH.player.x = 256; NH.player.y = 330; return NH.startMission('lag_01'); },
      board() { const d = NH.MS.cur.danfo; NH.player.x = d.x + 20; NH.player.y = d.y; NH.pressed.add('KeyF'); NH.sim(.1); return NH.player.veh === d; },
      stop(id, agbero = 'pay') { const st = NH.BUS_STOPS[id]; T3.go(st.x, st.y); NH.sim(.2); const sheet = !!T3.sheet(); if (sheet) T3.click(`[data-a="${agbero}"]`); return sheet; },
    };
  });

  // ---- intro dialogue
  const r1 = await run(() => { const started = T3.start(); NH.sim(.1);
    const D = NH.DLG, name = document.querySelector('#dlg-name').textContent, open1 = D.open && !document.querySelector('#dialog').hidden;
    NH.dlgAdvance(); const full = document.querySelector('#dlg-text').textContent; // first press finishes the typewriter line
    const face = document.querySelector('#dlg-face').getContext('2d').getImageData(72, 60, 1, 1).data[0] > 0;
    const paused = NH.WORLD.paused; NH.dlgSkip();
    return { started, open1, name, full, face, paused, closed: !D.open, oi: NH.MS.cur.oi, line: NH.MS.cur.def.objs[NH.MS.cur.oi].text }; });
  ok('meeting Baba Driver opens a portrait dialogue in Pidgin that pauses the game', r1.started && r1.open1 && r1.name === 'Baba Driver' && /Ehen/.test(r1.full) && r1.face && r1.paused, r1);
  ok('after the dialogue the objective is the bay-1 danfo', r1.closed && r1.oi === 1 && /bay 1/.test(r1.line), r1);

  // ---- board and run the route
  const r2 = await run(() => { const bay = NH.PARK_BAYS[0], d = NH.MS.cur.danfo; const out = { atBay: Math.hypot(d.x - bay.x, d.y - bay.y) < 2, dest: d.dest };
    out.inBus = T3.board(); out.oi = NH.MS.cur.oi; out.shift = NH.SHIFT.on && NH.SHIFT.mission; out.cut = NH.SHIFT.cut;
    out.deadline = NH.MS.cur.deadline - NH.S.minutes;
    const m0 = NH.S.minutes; NH.sim(10); out.clock10s = +(NH.S.minutes - m0).toFixed(1); // half-speed clock while the deadline runs
    const st = NH.BUS_STOPS.oshoja; NH.updateJobCard(); out.target = NH.JOB.target && Math.hypot(NH.JOB.target.x - st.x, NH.JOB.target.y - st.y) < 1; // job card and arrow share this target
    // stop 1: Oshodi (no agbero on the first day)
    out.agbero1 = T3.stop('oshoja'); out.ctx = NH.actionContext() && NH.actionContext().label;
    const waiting = NH.SHIFT.waiting.oshoja.length; out.waiting = waiting; NH.callPassengers();
    out.onboard1 = NH.SHIFT.onboard.length; out.fares = NH.SHIFT.onboard.map(p => p.fare); out.dests = NH.SHIFT.onboard.map(p => p.dest);
    out.noChange = !NH.SHIFT.changeQ.length; NH.sim(3); out.inside = NH.SHIFT.onboard.every(p => p.state === 'ride'); NH.depart();
    // stop 2: Yaba, the agbero wants ₦500; pay
    const before = NH.SHIFT.carried; out.agbero2 = T3.stop('balogate', 'pay'); out.ticket = NH.SHIFT.agbero;
    out.droppedAt2 = NH.SHIFT.carried - before; out.noneFor2 = NH.SHIFT.onboard.every(p => p.dest !== 'balogate');
    NH.callPassengers(); NH.sim(3); NH.depart();
    // mood: a hard stop from speed upsets passengers
    const v = NH.player.veh; NH.SHIFT.comfort = 100; v.v = 260; NH.keys.Space = true; NH.sim(.6); NH.keys.Space = false; out.moodAfterBrake = Math.round(NH.SHIFT.comfort);
    out.hud = document.querySelector('#hustle').textContent;
    // stop 3: Charity
    T3.stop('iya'); NH.callPassengers(); NH.sim(3); NH.depart(); NH.sim(.2);
    out.routeDone = NH.SHIFT.routeDone; out.oi3 = NH.MS.cur.oi; out.left = NH.SHIFT.onboard.length;
    return out; });
  ok('the mission danfo waits in bay 1 with YABA on its board', r2.atBay && r2.dest === 'YABA', r2);
  ok('getting in starts the mission route with Baba Driver\'s 40% cut and a 2-hour in-game deadline', r2.inBus && r2.oi === 2 && r2.shift && r2.cut === .4 && Math.round(r2.deadline) === 120, r2);
  ok('the clock runs at half speed while the deadline counts', r2.clock10s === 10, r2.clock10s);
  ok('job card and arrow share one target: the next stop', r2.target, r2);
  ok('stop 1: ACTION calls passengers; 2–5 wait and all board, paying ₦200–₦400 exact', !r2.agbero1 && /^Call passengers/.test(r2.ctx) && r2.waiting >= 2 && r2.waiting <= 5 && r2.onboard1 === r2.waiting && r2.fares.every(f => f >= 200 && f <= 400) && r2.noChange, r2);
  ok('passengers walk to the danfo and disappear inside', r2.inside, r2);
  ok('passengers only ride forward (later stops, or home to the park)', r2.dests.every(d => d === 'balogate' || d === 'iya' || d === null), r2.dests);
  ok('stop 2: the agbero at Yaba takes a ₦500 ticket', r2.agbero2 && r2.ticket === 500, r2);
  ok('passengers for Yaba got down there automatically', r2.droppedAt2 === r2.dests.filter(d => d === 'balogate').length && r2.noneFor2, r2);
  ok('hard braking lowers the mood meter (shown as a face on the HUD)', r2.moodAfterBrake < 100 && /Mood/.test(r2.hud), r2);
  ok('after stop 3 the objective is to bring the danfo back to the park', r2.routeDone && r2.oi3 === 3, r2);

  // ---- back at the park: Baba's line, then the summary
  const r3 = await run(() => { const cash0 = NH.S.cash; const t = NH.shiftTotals(); T3.go(256, 330); const dlg = NH.DLG.open; NH.dlgSkip();
    const s = T3.sheet(), txt = s ? s.textContent : ''; const net = NH.shiftTotals(); T3.click('[data-x]'); NH.sim(.1);
    return { dlg, txt, cutOk: /Baba Driver's cut \(40%\)/.test(txt), rows: ['Passengers carried', 'Fares collected', 'Tips', 'Agbero tickets', 'Damage to the bus', 'Your earnings'].every(k => txt.includes(k)),
      fares: t.fares, cut: t.cut, cutCalc: Math.round(t.fares * .4), net: t.net, gained: NH.S.cash - cash0, done: NH.S.done.includes('lag_01'), noMission: !NH.MS.cur }; });
  ok('arriving at the park plays Baba Driver\'s line, then the shift summary', r3.dlg && r3.rows && r3.cutOk, r3);
  ok('Baba Driver takes 40% of fares; your earnings land in your naira balance', r3.cut === r3.cutCalc && r3.gained === r3.net && r3.net > 0, r3);
  ok('the mission completes', r3.done && r3.noMission, r3);

  // ---- unlocked: conductor shift side job, next mission teaser
  const r4 = await run(() => { NH.sim(.2); T3.go(900, 560); const v = NH.player.veh; const ctx = NH.actionContext(); NH.updateJobCard();
    return { ctx: ctx && ctx.label, title: document.querySelector('#job-title').textContent, line: document.querySelector('#job-line').textContent, more: document.querySelector('#job-more').innerHTML, inDanfo: !!v && v.type === 'danfo' }; });
  ok('Conductor Shift unlocks as a side job in any danfo', r4.inDanfo && /Conductor Shift/.test(r4.ctx), r4);
  ok('the job card teases the next mission', r4.title === 'Next job' && r4.line === 'Go-Slow Getaway' && /pickpocket/i.test(r4.more) && /Conductor Shift/.test(r4.more), r4);

  // ---- before completion, a danfo doesn't offer shifts
  const r5 = await run(() => { T3.go(900, 560); NH.S.done = NH.S.done.filter(x => x !== 'lag_01'); const ctx = NH.actionContext(); NH.S.done.push('lag_01'); return ctx && ctx.label; });
  ok('conductor shifts are locked until the first day is done', /unlock after/.test(r5), r5);

  // ---- agbero: drive off = 1 star
  const r6 = await run(() => { NH.player.veh && (NH.player.veh.driver = null, NH.player.veh = null); T3.start(); NH.sim(.1); NH.dlgSkip(); T3.board();
    T3.stop('oshoja'); NH.callPassengers(); NH.sim(2); NH.depart(); const had = T3.stop('balogate', 'off'); NH.sim(.1); return { had, stars: NH.stars() }; });
  ok('driving off from the agbero gives 1 wanted star', r6.had && r6.stars === 1, r6);

  // ---- fail: wrecked → Baba scolds → retry
  const r7 = await run(() => { NH.clearHeat(); const d = NH.MS.cur.danfo; d.hp = 0; NH.sim(.1);
    const dlg = NH.DLG.open, text = NH.DLG.lines.join(' '); NH.dlgSkip(); const s = T3.sheet(), hasRetry = !!(s && s.querySelector('[data-a="retry"]'));
    T3.click('[data-a="retry"]'); NH.sim(.1); const again = NH.MS.cur && NH.MS.cur.id === 'lag_01', retryLine = NH.DLG.open && /try am again/.test(NH.DLG.lines[0]);
    NH.dlgSkip(); return { dlg, text, hasRetry, again, retryLine, oi: NH.MS.cur && NH.MS.cur.oi, onFoot: !NH.player.veh, shiftOff: !NH.SHIFT.on }; });
  ok('wrecking the danfo fails the mission: Baba Driver scolds you', r7.dlg && /motor/.test(r7.text), r7);
  ok('retry restarts the mission at the park with a short line from Baba', r7.hasRetry && r7.again && r7.retryLine && r7.oi === 1 && r7.onFoot && r7.shiftOff, r7);

  // ---- fail: out of time
  const r8 = await run(() => { T3.board(); NH.MS.cur.deadline = NH.S.minutes + 1; NH.sim(2); const text = NH.DLG.lines.join(' '); const dlg = NH.DLG.open; NH.dlgSkip(); return { dlg, text, failed: !NH.MS.cur }; });
  ok('running out of in-game time fails the mission with Baba\'s complaint', r8.dlg && /Two hours/.test(r8.text) && r8.failed, r8);

  ok('no JS errors', !errors.length, errors);
  console.log(fails ? `${fails} FAILED` : 'ALL PASS'); await b.close(); process.exit(fails ? 1 : 0);
})();
