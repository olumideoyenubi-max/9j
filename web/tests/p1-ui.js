// Phase 1.3–1.8 — job card, notification queue, HUD layout (no overlaps, nothing off-screen),
// health/fuel bars + refuelling, district labels, touch + keyboard controls.
const { chromium, open } = require('./harness');
const results = []; const ok = (name, pass, info) => { results.push({ name, pass, info }); };
const { VIEWPORTS, layout } = require('./layout');
(async () => {
  const b = await chromium.launch();
  // ---------- 1.5 HUD layout across viewports, in a busy state ----------
  for (const vp of VIEWPORTS) {
    const { page, errors } = await open(b, vp);
    // busy state: driving a danfo on a timed mission objective, heat, a toast, conductor hustle box, prompt
    const states = {
      'on foot by a car (prompt) + expanded job card + toast': async () => page.evaluate(() => { NH.resetWorld(); NH.player.x = 300; NH.player.y = 480; NH.spawnVehicle('sedan', 330, 480, 0, { parked: true }); NH.sim(.2); NH.JOB.open = true; NH.JOB.until = performance.now() + 9e3; NH.ui.toast('Yarns: @DanfoDiaries posted about you — a long message to test wrapping in the narrow column'); }),
      'driving a timed mission with 3 stars': async () => page.evaluate(() => { NH.resetWorld(); NH.startMission('lag_01'); const d = NH.MS.cur.danfo; NH.player.veh = d; d.driver = 'player'; NH.MS.cur.oi = 3; NH.MS.cur.timer = 200; NH.addHeat(3); NH.sim(.2); NH.ui.toast('Wanted: Eko Task Force pickups on the way'); }),
      'conductor shift + JOB DONE card': async () => page.evaluate(() => { NH.resetWorld(); const v = NH.spawnVehicle('danfo', 300, 480, 0, { parked: true }); v.driver = 'player'; NH.player.veh = v; NH.startShift(); NH.sim(.2); NH.ui_card('JOB DONE!', 'First Day on the Danfo', '+₦8,000   +20 cred'); }),
    };
    for (const [label, setup] of Object.entries(states)) {
      await setup(); await page.waitForTimeout(250); await page.evaluate(() => NH.sim(1 / 60)); // one game step re-places the HUD, as every frame does in play
      const L = await layout(page);
      ok(`layout ${vp.name}: ${label}`, !L.overlaps.length && !L.off.length && !L.hscroll, L);
      if (label.startsWith('driving')) await page.screenshot({ path: `/tmp/claude-0/shots/p1-${vp.width}x${vp.height}.png` });
    }
    ok(`no JS errors ${vp.name}`, !errors.length, errors);
    await page.context().close();
  }

  // ---------- 1.3 job card ----------
  { const { page } = await open(b, VIEWPORTS[0]);
    const r = await page.evaluate(async () => {
      const wait = ms => new Promise(r => setTimeout(r, ms)); const card = document.querySelector('#job'), more = document.querySelector('#job-more');
      NH.resetWorld(); NH.player.x = 600; NH.player.y = 900; NH.sim(.1); NH.JOB.sig = ''; NH.sim(.05);
      const openedOnNew = !more.hidden; NH.JOB.until = performance.now() - 1; NH.sim(.05);
      const collapsed = more.hidden, h = card.getBoundingClientRect().height;
      const title = document.querySelector('#job-title').textContent, line = document.querySelector('#job-line').textContent, d = document.querySelector('#job-dist').textContent;
      card.click(); NH.sim(.05); const expanded = !more.hidden;
      await wait(4300); NH.sim(.05); const autoCollapsed = more.hidden;
      const r = card.getBoundingClientRect(), st = document.querySelector('#stars').getBoundingClientRect();
      return { openedOnNew, collapsed, h: Math.round(h), title, line, d, expanded, autoCollapsed, belowStars: r.top >= st.bottom, left: Math.round(r.left) };
    });
    ok('job card: compact title + distance under the stars, top-left', r.collapsed && r.h < 62 && r.title && r.line && /\d+ (m|km)$/.test(r.d) && r.belowStars && r.left <= 20, r);
    ok('job card: tap expands, auto-collapses after 4s', r.expanded && r.autoCollapsed, r);
    await page.context().close(); }

  // ---------- 1.4 notifications ----------
  { const { page } = await open(b, VIEWPORTS[0]);
    const r = await page.evaluate(async () => {
      const wait = ms => new Promise(r => setTimeout(r, ms)); const box = document.querySelector('#toasts');
      // the live loop pumps the queue; keep the world frozen but let toasts run
      const shown = []; let maxAtOnce = 0;
      new MutationObserver(() => { maxAtOnce = Math.max(maxAtOnce, box.children.length); for (const c of box.children) if (!shown.includes(c.textContent)) shown.push(c.textContent); }).observe(box, { childList: true });
      NH.NQ.q.length = 0; if (NH.NQ.el) NH.NQ.el.remove(); NH.NQ.cur = null; NH.NQ.el = null; NH.resetWorld();
      NH.ui.toast('first'); NH.ui.toast('second'); NH.ui.toast('first'); // duplicate key collapses
      NH.addHeat(2);                  // queues "Wanted: ... on the way" (only valid while wanted)
      NH.clearHeat(true);             // ...but heat is cleared before it shows -> replaced by "You lost them"
      NH.ui.toast('third');
      await wait(16500);
      const tb = box.getBoundingClientRect(), mm = document.querySelector('#minimap').getBoundingClientRect();
      return { shown, maxAtOnce, belowMinimap: tb.top >= mm.bottom - 1, rightCol: tb.left >= innerWidth / 2 };
    });
    const noStale = !r.shown.some(t => t.startsWith('Wanted:'));
    ok('notifications: one at a time, oldest first, deduped', r.maxAtOnce === 1 && r.shown[0] === 'first' && r.shown[1] === 'second' && r.shown.indexOf('third') > r.shown.findIndex(t => t.startsWith('You lost them')) && r.shown.filter(t => t === 'first').length === 1, r);
    ok('notifications: contradictory/stale messages skipped', noStale && r.shown.some(t => t.startsWith('You lost them')), r);
    ok('notifications: live in the top-right column below the minimap', r.belowMinimap && r.rightCol, r);
    await page.context().close(); }

  // ---------- 1.6 health/fuel bars, fuel drain, refuel ----------
  { const { page } = await open(b, VIEWPORTS[0]);
    const r = await page.evaluate(() => {
      NH.resetWorld(); const st = NH.STATIONS[0];
      const v = NH.spawnVehicle('sedan', 300, 480, 0, { parked: true }); v.driver = 'player'; NH.player.veh = v; v.fuel = 40; NH.sim(.1);
      const labels = [...document.querySelectorAll('#hud-bl .stat')].filter(e => !e.hidden).map(e => e.querySelector('.lbl').textContent);
      // a blind drive through random traffic can hit someone and get the player busted mid-test; this check is about fuel, so keep the police out of it
      const calm = () => { if (NH.stars()) NH.clearHeat(false); };
      const f0 = v.fuel; NH.keys.KeyW = true; NH.sim(6, calm); NH.keys.KeyW = false; NH.sim(2, calm); const f1 = v.fuel;
      v.fuel = 0; v.v = 0; NH.keys.KeyW = true; NH.sim(2, calm); const movedEmpty = Math.abs(v.v); NH.keys.KeyW = false;
      v.x = st.x; v.y = st.y; v.v = 0; v.fuel = 10; NH.sim(.05);
      const ctxLabel = NH.actionContext() && NH.actionContext().label; const cash0 = NH.S.cash;
      NH.pressed.add('KeyE'); NH.sim(1 / 60);
      return { labels, f0, f1: +f1.toFixed(1), movedEmpty, ctxLabel, fuelAfter: v.fuel, paid: cash0 - NH.S.cash, fval: document.querySelector('#fval').textContent };
    });
    ok('bars: ❤ Health and ⛽ Fuel labels shown when driving', r.labels.includes('❤ Health') && r.labels.includes('⛽ Fuel'), r);
    ok('fuel drains while driving; empty tank = no power', r.f1 < r.f0 && r.movedEmpty < 1, r);
    ok('refuel at a Gidi Fuel station (₦25 per %)', /^Refuel/.test(r.ctxLabel) && r.fuelAfter === 100 && r.paid === 2250, r);
    await page.context().close(); }

  // ---------- 1.7 / 3.1 place names: a fade-in banner on entering an area, never over the player ----------
  for (const vp of [VIEWPORTS[0], VIEWPORTS[2], VIEWPORTS[4]]) {
    const { page } = await open(b, vp);
    const seen = []; await page.evaluate(() => { NH.BANNER.cur = ''; NH.BANNER.shown = {}; }); // judge entering each area from a fresh state
    for (const [x, y, want] of [[176, 330, 'OSHOJA MOTOR PARK'], [900, 1300, 'BALO MARKET'], [2350, 560, 'EKO CREST'], [176, 330, 'OSHOJA MOTOR PARK']]) {
      await page.evaluate(([x, y]) => { NH.player.veh = null; NH.player.x = x; NH.player.y = y; NH.teleport(x, y); for (let i = 0; i < 30; i++) NH.renderOnce(i / 60); NH.sim(1 / 60); }, [x, y]);
      await page.evaluate(() => NH.sim(1 / 60));
      const st = await page.evaluate(() => { const b = document.querySelector('#banner'), r = b.getBoundingClientRect();
        const p = NH.screenOf(NH.player.x, NH.player.y, NH.heightAt(NH.player.x, NH.player.y) + 1);
        return { shown: !b.hidden, text: b.querySelector('b').textContent, coversPlayer: !b.hidden && p.x > r.left - 6 && p.x < r.right + 6 && p.y > r.top - 6 && p.y < r.bottom + 6 }; });
      seen.push(Object.assign({ want }, st));
    }
    await page.waitForTimeout(2900); const hiddenLater = await page.$eval('#banner', e => e.hidden);
    const firstThree = seen.slice(0, 3);
    ok(`place banner ${vp.name}: shows the area name, never over the player, fades out`, firstThree.every(s => s.shown && s.text === s.want && !s.coversPlayer) && hiddenLater, seen);
    ok(`place banner ${vp.name}: no repeat when bouncing back within 15s`, !seen[3].shown || seen[3].text !== 'OSHOJA MOTOR PARK' || seen[3].text === seen[2].text, seen[3]);
    await page.context().close();
  }

  // ---------- 1.8 controls ----------
  { const { page } = await open(b, VIEWPORTS[2]); // touch phone
    const btns = await page.$$eval('#tbtns button', bs => bs.filter(b => b.offsetParent).map(b => b.textContent));
    const stickVisible = await page.$eval('#stick', e => getComputedStyle(e).display !== 'none' && e.getBoundingClientRect().width > 50);
    await page.evaluate(() => { NH.resetWorld(); NH.player.x = 300; NH.player.y = 480; NH.spawnVehicle('sedan', 330, 480, 0, { parked: true }); NH.sim(.1); });
    const promptText = await page.$eval('#prompt', e => e.textContent);
    await page.click('#prompt'); await page.evaluate(() => NH.sim(1 / 60));
    const inCar = await page.evaluate(() => !!NH.player.veh);
    const gas = await page.$('#tb-gas'); const gb = await gas.boundingBox();
    await page.mouse.move(gb.x + gb.width / 2, gb.y + gb.height / 2); await page.mouse.down();
    const speed = await page.evaluate(() => { NH.sim(1.5); return NH.player.veh.v; });
    const zone = await page.$('#stick'); const zb = await zone.boundingBox();
    await page.mouse.up();
    await page.mouse.move(zb.x + zb.width / 2, zb.y + zb.height / 2); await page.mouse.down(); await page.mouse.move(zb.x + zb.width / 2 + 45, zb.y + zb.height / 2, { steps: 4 });
    const steer = await page.evaluate(() => { const a0 = NH.player.veh.a; NH.touch.held.gas = true; NH.sim(1); NH.touch.held.gas = false; return { tx: NH.touch.x, turned: NH.player.veh.a - a0 }; });
    await page.mouse.up();
    ok('touch: joystick + GAS, BRAKE, ACTION, EXIT/ENTER, PHONE', stickVisible && ['GAS', 'BRAKE', 'ACTION', 'PHONE'].every(t => btns.some(x => x === t || x === 'RUN' && t === 'GAS')) && btns.some(x => x === 'ENTER' || x === 'EXIT'), { btns, stickVisible });
    ok('touch: prompt says "Tap" and tapping it does the action', /^Tap · (Steal|Enter)/.test(promptText) && inCar, { promptText, inCar });
    ok('touch: GAS accelerates, joystick steers', speed > 60 && steer.tx > .5 && Math.abs(steer.turned) > .2, { speed, steer });
    await page.context().close(); }
  { const { page } = await open(b, VIEWPORTS[0]); // desktop keyboard
    // the player's own car, so getting in isn't theft (no police to interfere with the driving check)
    await page.evaluate(() => { NH.resetWorld(); NH.seed(77); NH.player.x = 300; NH.player.y = 480; NH.player.owned = NH.spawnVehicle('sedan', 330, 480, 0, { parked: true }); NH.sim(.1); });
    const promptHtml = await page.$eval('#prompt', e => e.innerHTML);
    const touchHidden = await page.$eval('#tbtns', e => getComputedStyle(e).display === 'none');
    await page.keyboard.press('KeyE'); const inCar = await page.evaluate(() => { NH.pressed.add('KeyE'); NH.sim(1 / 60); return !!NH.player.veh; });
    const wsad = await page.evaluate(() => { NH.keys.KeyW = true; NH.sim(1); NH.keys.KeyW = false; NH.keys.Space = true; NH.sim(1.5); NH.keys.Space = false; return NH.player.veh ? NH.player.veh.v : null; });
    ok('desktop: "E" prompt, E gets in, W drives, Space brakes, touch controls hidden', /<kbd>E<\/kbd>/.test(promptHtml) && inCar && wsad !== null && Math.abs(wsad) < 5 && touchHidden, { promptHtml, inCar, wsad, touchHidden });
    await page.context().close(); }

  await b.close();
  let fails = 0; for (const t of results) { if (!t.pass) fails++; console.log((t.pass ? 'PASS ' : 'FAIL ') + t.name + (t.pass ? '' : '  ' + JSON.stringify(t.info))); }
  console.log(fails ? `${fails} FAILED` : 'ALL PASS'); process.exit(fails ? 1 : 0);
})();
