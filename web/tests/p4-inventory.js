// Inventory: Tab tap / hold (full screen and quick wheel), the bag button on touch, 30% speed while open,
// starting items and their effects, capacity and bag upgrades, shops, documents and the checkpoint,
// cash / bank / transactions, clothing, vehicles + waypoint, mission items, save and load, layout.
const { chromium, open } = require('./harness');
let fails = 0; const ok = (name, cond, info) => { console.log((cond ? 'PASS ' : 'FAIL ') + name + (cond ? '' : '  ' + JSON.stringify(info))); if (!cond) fails++; };
(async () => {
  const b = await chromium.launch(); const { page, errors } = await open(b);
  const run = (f, a) => page.evaluate(f, a);
  await run(() => { window.T4 = {
    fresh() { NH.resetWorld(); NH.seed(4); NH.invClose(); NH.closeOverlays(); delete NH.S.inv; delete NH.S.wheel; delete NH.S.bag; delete NH.S.bank; delete NH.S.docs; NH.S.garage = []; NH.S.done = NH.S.done.filter(x => x !== 'lag_01');
      NH.applyState(null); NH.S.cash = 50000; NH.player.veh = null; NH.player.x = 300; NH.player.y = 480; NH.player.hp = 100; },
    sheet: () => document.querySelector('#sheet:not([hidden])'),
    click(sel, root = '#inv') { const e = document.querySelector(root + ' ' + sel); if (e) e.click(); return !!e; },
  }; });

  // ---- starting items
  const r1 = await run(() => { T4.fresh(); return { inv: NH.S.inv.map(e => e.id + 'x' + e.n), used: NH.slotsUsed(), cap: NH.bagSlots(), wheel: NH.S.wheel.slice(), bank: NH.S.bank }; });
  ok('starting items: 3 pure water, 2 gala, puff-puff, energy drink, first-aid kit, toolkit, jerrycan, phone', ['waterx3', 'galax2', 'puffx1', 'energyx1', 'firstaidx1', 'toolkitx1', 'jerrycanx1', 'phonex1'].every(s => r1.inv.includes(s)) && r1.inv.length === 8, r1);
  ok('nylon bag holds 12 slots; the wheel starts with 8 items', r1.cap === 12 && r1.used === 8 && r1.wheel.length === 8 && r1.wheel.every(Boolean), r1);

  // ---- Tab: tap opens the full screen, hold opens the wheel; the world slows to 30%; Esc and tap-outside close
  const tapTab = async () => { await page.keyboard.down('Tab'); await page.waitForTimeout(60); await page.keyboard.up('Tab'); };
  await tapTab();
  const r2 = await run(() => ({ open: NH.INV.open, vis: !document.querySelector('#inv').hidden, scale: NH.timeScale(), tabs: [...document.querySelectorAll('#inv-tabs button')].map(x => x.textContent),
    cards: document.querySelectorAll('#inv-body .icard:not(.empty)').length, blur: getComputedStyle(document.querySelector('#inv')).backdropFilter }));
  ok('tapping Tab opens the full inventory with six tabs', r2.open && r2.vis && r2.tabs.join() === 'Items,Cash,Documents,Clothing,Vehicles,Mission' && r2.cards === 8, r2);
  ok('while it is open the game runs at 30% speed behind a blurred, darkened backdrop', r2.scale === .3 && /blur/.test(r2.blur), r2);
  await tapTab(); const r3 = await run(() => ({ open: NH.INV.open, scale: NH.timeScale() }));
  await page.keyboard.press('KeyI'); const iOpen = await run(() => NH.INV.open); await page.keyboard.press('Escape'); const escClosed = await run(() => !NH.INV.open);
  await page.keyboard.press('KeyI'); await page.mouse.click(4, 380); const outsideClosed = await run(() => !NH.INV.open);
  ok('Tab again, I, Esc and a tap outside the panel all close it; speed returns to normal', !r3.open && r3.scale === 1 && iOpen && escClosed && outsideClosed, { r3, iOpen, escClosed, outsideClosed });
  // hold Tab → wheel, point at slot 1 (pure water, top), release → used
  await run(() => { NH.player.hp = 60; });
  await page.keyboard.down('Tab'); await page.waitForTimeout(400);
  const w1 = await run(() => ({ wheel: NH.INV.wheel, slots: document.querySelectorAll('#wring .wslot').length, scale: NH.timeScale(), box: (() => { const r = document.querySelector('#wring .wslot[data-w="0"]').getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; })() }));
  await page.mouse.move(w1.box.x, w1.box.y); await page.keyboard.up('Tab');
  const w2 = await run(() => ({ wheel: NH.INV.wheel, hp: NH.player.hp, water: NH.invCount('water'), toast: document.querySelector('#toasts').textContent + NH.NQ.q.map(n => n.text).join('|') }));
  ok('holding Tab opens the 8-slot quick wheel (game at 30%)', w1.wheel && w1.slots === 8 && w1.scale === .3, w1);
  ok('releasing Tab over pure water drinks it: +10 health, one fewer sachet, a toast', !w2.wheel && w2.hp === 70 && w2.water === 2 && /\+10 health/.test(w2.toast), w2);
  await page.keyboard.down('Tab'); await page.waitForTimeout(400); await page.keyboard.press('Digit2'); await page.keyboard.up('Tab');
  const w3 = await run(() => ({ hp: NH.player.hp, gala: NH.invCount('gala'), wheel: NH.INV.wheel, anim: !!NH.lookOf(NH.player).use }));
  ok('number keys pick a wheel slot (2 = gala, +15 health) and play an eating animation', w3.hp === 85 && w3.gala === 1 && !w3.wheel && w3.anim, w3);

  // ---- disabled in dialogue
  const r4 = await run(() => { NH.sayLines('baba', ['Test line.']); const a = NH.invOpen(), w = NH.invWheelOpen(); NH.dlgSkip(); return { a, w, open: NH.INV.open || NH.INV.wheel }; });
  ok('the inventory and wheel stay shut during dialogue', !r4.a && !r4.w && !r4.open, r4);

  // ---- item effects
  const r5 = await run(() => { T4.fresh(); const out = {};
    NH.player.hp = 40; NH.useItem('puff'); out.puff = NH.player.hp; NH.useItem('firstaid'); out.aid = NH.player.hp;
    out.fullBlock = !NH.useItem('water') && NH.invCount('water') === 3;
    NH.useItem('energy'); out.boost = NH.BUFF.sprintUntil - NH.simClock(); out.buffHud = (NH.sim(.05), !document.querySelector('#buff').hidden);
    // sprint is faster while boosted
    NH.keys.ShiftLeft = true; NH.keys.KeyW = true; const x0 = NH.player.x, y0 = NH.player.y; NH.sim(.5); const fast = Math.hypot(NH.player.x - x0, NH.player.y - y0);
    NH.BUFF.sprintUntil = 0; const x1 = NH.player.x, y1 = NH.player.y; NH.sim(.5); const slow = Math.hypot(NH.player.x - x1, NH.player.y - y1); NH.keys.ShiftLeft = NH.keys.KeyW = false;
    out.fast = Math.round(fast); out.slow = Math.round(slow);
    // toolkit: next to a damaged vehicle, not from inside it
    const v = NH.spawnVehicle('sedan', NH.player.x + 30, NH.player.y, 0, { parked: true }); v.hp = 20; NH.player.x = v.x - 26; NH.player.y = v.y;
    NH.useItem('toolkit'); out.hp = v.hp; out.kit = NH.invCount('toolkit');
    // jerrycan: in the vehicle
    v.fuel = 20; NH.player.veh = v; v.driver = 'player'; NH.useItem('jerrycan'); out.fuel = v.fuel; out.can = NH.invCount('jerrycan');
    NH.addItem('toolkit'); out.kitInCar = !NH.useItem('toolkit');
    NH.player.veh = null; v.driver = null; return out; });
  ok('puff-puff +20, first-aid kit +50, no wasting water at full health', r5.puff === 60 && r5.aid === 100 && r5.fullBlock, r5);
  ok('energy drink: 60s sprint boost shown on the HUD, and sprinting is faster', Math.abs(r5.boost - 60) < .2 && r5.buffHud && r5.fast > r5.slow * 1.15, r5);
  ok('toolkit repairs 40% of damage when standing next to the vehicle (not from inside)', r5.hp === 20 + 100 * .4 && r5.kit === 0 && r5.kitInCar, r5);
  ok('jerrycan refuels the current vehicle by 50%', r5.fuel === 70 && r5.can === 0, r5);
  const r6 = await run(() => { NH.invOpen('items'); NH.INV.sel = 'phone'; NH.invRender(); T4.click('[data-act="use"]'); const phone = !document.querySelector('#phone').hidden; NH.closeOverlays(); return { phone, inv: NH.INV.open }; });
  ok('using the phone item opens the phone', r6.phone && !r6.inv, r6);

  // ---- full screen actions: drop, assign to wheel
  const r7 = await run(() => { T4.fresh(); NH.invOpen('items'); T4.click('[data-item="gala"]'); T4.click('[data-act="drop"]');
    const dropped = NH.items().find(i => i.kind === 'loot' && i.item === 'gala'), gala = NH.invCount('gala');
    T4.click('[data-item="firstaid"]'); T4.click('[data-act="assign"]'); T4.click('[data-slot="0"]');
    const wheel = NH.S.wheel.slice(); NH.INV.sel = 'phone'; NH.invRender(); const phoneDrop = document.querySelector('#inv [data-act="drop"]').disabled;
    NH.invClose(); // walk away and come back: the gala is picked up again
    NH.player.x += 60; NH.sim(.1); NH.player.x = dropped.x; NH.player.y = dropped.y; NH.sim(.1);
    return { dropped: !!dropped, gala, wheel, phoneDrop, back: NH.invCount('gala') }; });
  ok('DROP puts the item on the ground; walking back over it picks it up again', r7.dropped && r7.gala === 1 && r7.back === 2, r7);
  ok('ASSIGN TO WHEEL moves the item into the chosen slot; the phone can’t be dropped', r7.wheel[0] === 'firstaid' && r7.wheel.includes('water') && r7.phoneDrop, r7);

  // ---- capacity, bag full, upgrades
  const r8 = await run(() => { T4.fresh(); const out = {};
    out.addMany = NH.addItem('water', 7) && NH.addItem('gala', 8) && NH.slotsUsed() === 8; // 10 water = 1 slot, 10 gala = 1 slot
    NH.S.inv = [['water', 30], ['gala', 30], ['puff', 30], ['firstaid', 3], ['toolkit', 2], ['phone', 1]].map(([id, n]) => ({ id, n })); out.fill = true; out.used = NH.slotsUsed();
    out.blocked = !NH.addItem('energy');
    NH.items().push({ x: NH.player.x + 40, y: NH.player.y, kind: 'loot', item: 'energy', n: 1, armed: true }); NH.player.x += 40; NH.sim(.1);
    out.toast = NH.NQ.q.map(n => n.text).join('|') + (NH.NQ.cur ? NH.NQ.cur.text : ''); out.still = NH.items().some(i => i.kind === 'loot' && i.item === 'energy');
    out.buyFull = NH.buyItem('jerrycan');
    NH.S.cash = 30000; out.bp = NH.buyItem('bag:backpack'); out.cap1 = NH.bagSlots(); out.gmg = NH.buyItem('bag:gmg'); out.cap2 = NH.bagSlots(); out.cash = NH.S.cash; out.again = NH.buyItem('bag:backpack');
    return out; });
  ok('12 slots: a full bag refuses more and a pickup says "Bag full"', r8.addMany && r8.fill && r8.used === 12 && r8.blocked && /Bag full/.test(r8.toast) && r8.still && /Bag full/.test(r8.buyFull), r8);
  ok('backpack (20 slots, ₦8,000) and Ghana-Must-Go bag (30 slots, ₦15,000) from market stalls', r8.cap1 === 20 && r8.cap2 === 30 && r8.cash === 30000 - 8000 - 15000 && /already/.test(r8.again), r8);

  // ---- shops
  const r9 = await run(() => { T4.fresh(); const out = { kiosks: NH.KIOSKS.length, med: NH.MED_SHOPS.length };
    const k = NH.KIOSKS[0]; NH.player.x = k.x; NH.player.y = k.y + 10; out.near = NH.nearShop(); const c = NH.actionContext(); out.label = c && c.label;
    if (c) c.run(); const s = T4.sheet(); out.rows = s ? [...s.querySelectorAll('.srow b')].map(x => x.textContent) : []; out.prices = s ? [...s.querySelectorAll('[data-buy]')].map(x => x.textContent) : [];
    const cash = NH.S.cash; T4.click('[data-buy="gala"]', '#sheet'); out.paid = cash - NH.S.cash; out.gala = NH.invCount('gala'); out.cashShown = /₦/.test(T4.sheet().textContent);
    NH.closeOverlays();
    const m = NH.MED_SHOPS[0]; if (m) { NH.player.x = m.x; NH.player.y = m.y; out.medNear = NH.nearShop(); }
    NH.openShop('medicine'); out.medPrices = [...T4.sheet().querySelectorAll('[data-buy]')].map(x => x.dataset.buy + '=' + x.textContent); NH.closeOverlays();
    NH.openShop('stall'); out.stall = [...T4.sheet().querySelectorAll('[data-buy]')].map(x => x.dataset.buy + '=' + x.textContent); NH.closeOverlays();
    out.stallTile = (() => { for (let y = 0; y < 2000; y += 8) for (let x = 0; x < 2000; x += 8) if (NH.tileAt(x, y) === NH.T.STALL) { NH.player.x = x; NH.player.y = y + 9; return NH.nearShop(); } })();
    return out; });
  ok('kiosks are shops: walk up, ACTION, item list with naira prices, BUY, cash shown', r9.kiosks > 5 && r9.near === 'kiosk' && /kiosk/i.test(r9.label) && r9.rows.includes('Gala') && r9.prices.includes('₦200') && r9.paid === 200 && r9.gala === 3 && r9.cashShown, r9);
  ok('Trust In God Medicine Store sells the first-aid kit (₦3,000) and energy drink (₦500)', r9.med > 0 && r9.medNear === 'medicine' && r9.medPrices.includes('firstaid=₦3,000') && r9.medPrices.includes('energy=₦500'), r9);
  ok('market stalls: water ₦50, puff-puff ₦100, jerrycan ₦2,500, toolkit ₦5,000, bags', r9.stallTile === 'stall' && ['water=₦50', 'puff=₦100', 'jerrycan=₦2,500', 'toolkit=₦5,000', 'bag:backpack=₦8,000', 'bag:gmg=₦15,000'].every(x => r9.stall.includes(x)), r9);

  // ---- documents and the checkpoint
  const r10 = await run(() => { T4.fresh(); const out = {}; NH.invOpen('docs'); out.missing = document.querySelectorAll('#inv .doc.miss').length; out.how = [...document.querySelectorAll('#inv .doc .how')].map(x => x.textContent).join(' | '); NH.invClose();
    const v = NH.spawnVehicle('danfo', 600, 200, 0, { parked: true }); NH.player.veh = v; v.driver = 'player';
    NH.openCheckpoint(); const s = T4.sheet(); out.noDocs = [...s.querySelectorAll('[data-c]')].map(x => x.dataset.c); NH.closeOverlays();
    NH.clearHeat(); const r = NH.resolveCheckpoint('Refuse', 0); out.refuse = r.kind; NH.sim(.05); out.stars = NH.stars(); NH.clearHeat();
    const c0 = NH.S.cash; NH.resolveCheckpoint('Settle', 0); out.settled = c0 - NH.S.cash; NH.clearHeat();
    // the booth sells a licence and papers
    NH.player.veh = null; v.driver = null; NH.player.x = NH.BOOTH.x; NH.player.y = NH.BOOTH.y + 20; out.booth = NH.nearShop(); NH.actionContext().run();
    T4.click('[data-doc="licence"]', '#sheet'); T4.click('[data-doc="papers"]', '#sheet'); out.docs = Object.assign({}, NH.S.docs); NH.closeOverlays();
    NH.player.veh = v; v.driver = 'player'; NH.openCheckpoint(); out.withDocs = [...T4.sheet().querySelectorAll('[data-c]')].map(x => x.dataset.c); out.pass = NH.resolveCheckpoint('Papers', 0).kind; NH.closeOverlays();
    v.stolen = true; out.stolen = NH.papersOk(); NH.player.veh = null; v.driver = null;
    NH.invOpen('docs'); out.missingAfter = document.querySelectorAll('#inv .doc.miss').length; NH.invClose(); return out; });
  ok('DOCUMENTS: licence, union card and papers start missing (greyed) with how to get them', r10.missing === 3 && /ticket booth/.test(r10.how) && /First Day/.test(r10.how), r10);
  ok('checkpoint without papers: settle (₦1,000) or refuse and get a wanted star', r10.noDocs.includes('Settle') && r10.noDocs.includes('Refuse') && !r10.noDocs.includes('Papers') && r10.stars === 1 && r10.settled === 1000, r10);
  ok('licence + papers from the park booth; then the checkpoint waves you through', r10.booth === 'booth' && r10.docs.licence && r10.docs.papers && r10.withDocs.includes('Papers') && !r10.withDocs.includes('Refuse') && r10.pass === 'WavedThrough' && r10.missingAfter === 1, r10);
  ok('papers don’t cover a stolen vehicle', r10.stolen.lic && !r10.stolen.pap && r10.stolen.stolen, r10);

  // ---- cash tab
  const r11 = await run(() => { T4.fresh(); NH.S.log = []; NH.S.cash = 10000; NH.buyItem('gala'); NH.invOpen('cash');
    T4.click('[data-dep="5000"]'); const afterDep = { cash: NH.S.cash, bank: NH.S.bank }; T4.click('[data-wd="1000"]');
    const rows = [...document.querySelectorAll('#inv .tx .w')].map(x => x.textContent); const big = [...document.querySelectorAll('#inv .money b')].map(x => x.textContent);
    for (let i = 0; i < 14; i++) NH.S.log.unshift({ t: 600, n: 10, why: 'x' + i }); NH.invRender(); const n = document.querySelectorAll('#inv .tx').length; NH.invClose();
    return { afterDep, cash: NH.S.cash, bank: NH.S.bank, rows, big, n }; });
  ok('CASH: cash on hand and bank, deposit / withdraw, and the last 10 transactions', r11.afterDep.cash === 4800 && r11.afterDep.bank === 17500 && r11.cash === 5800 && r11.bank === 16500 && r11.rows[0] === 'Withdrawal from bank' && r11.rows.includes('Bought Gala') && r11.n === 10, r11);
  const r11b = await run(() => { // a conductor shift lists fares, Baba Driver's cut and agbero tickets separately
    NH.resetWorld(); NH.S.log = []; const v = NH.spawnVehicle('danfo', 300, 300, 0, { parked: true }); NH.player.veh = v; v.driver = 'player';
    NH.startShift('market', { owner: 'Baba Driver', cut: .35 }); NH.SHIFT.fares = 2000; NH.SHIFT.agbero = 500; const c0 = NH.S.cash; NH.endShift(); NH.closeOverlays();
    NH.player.veh = null; v.driver = null; return { rows: NH.S.log.slice(0, 4).map(l => l.why + ':' + l.n), net: NH.S.cash - c0 }; });
  ok('shift pay shows up as fares, Baba Driver’s cut and agbero tickets', r11b.rows.some(r => /^Fares/.test(r)) && r11b.rows.includes("Baba Driver's cut:-700") && r11b.rows.includes('Agbero tickets:-500') && r11b.net === 800, r11b);

  // ---- clothing
  const r12 = await run(async () => { T4.fresh(); NH.S.outfits = ['fit_street_basic', 'fit_agbada_owambe']; NH.S.outfit = 'fit_street_basic'; NH.invOpen('clothing');
    const rows = document.querySelectorAll('#inv .fit').length; T4.click('[data-fit="fit_agbada_owambe"]');
    await new Promise(r => setTimeout(r, 300)); const cv = document.querySelector('#inv-prev'); const prev = !!cv && cv.width > 0;
    const stillWearing = NH.S.outfit; T4.click('[data-equip]'); const wearing = NH.S.outfit; T4.click('[data-fit="fit_kaftan_white"]'); const locked = document.querySelector('#inv [data-equip]').disabled; NH.invClose();
    return { rows, prev, stillWearing, wearing, locked }; });
  ok('CLOTHING: tapping an outfit previews it on a turning model; EQUIP wears it in the world', r12.rows === 7 && r12.prev && r12.stillWearing === 'fit_street_basic' && r12.wearing === 'fit_agbada_owambe' && r12.locked, r12);

  // ---- vehicles + waypoint, mission items
  const r13 = await run(() => { T4.fresh(); const v = NH.spawnVehicle('keke', 700, 480, 0, { parked: true }); NH.player.x = v.x - 20; NH.player.y = v.y; NH.pressed.add('KeyF'); NH.sim(.1); const inK = NH.player.veh === v;
    NH.exitVehicle(); NH.player.x = 300; NH.player.y = 480; NH.invOpen('vehicles'); const rows = document.querySelectorAll('#inv .vrow').length, map = !!document.querySelector('#inv-map');
    T4.click('[data-track="0"]'); const wp = NH.waypoint(); NH.invClose(); NH.updateJobCard(); const card = document.querySelector('#job-line').textContent;
    NH.player.x = v.x; NH.player.y = v.y + 20; NH.sim(.1); const cleared = !NH.waypoint(); return { inK, rows, map, wp: wp && Math.round(wp.x), vx: Math.round(v.x), card, cleared, garage: NH.S.garage.length }; });
  ok('VEHICLES: a vehicle you drove is listed with a mini map; TRACK sets a waypoint the job card follows', r13.inK && r13.rows === 1 && r13.map && r13.wp === r13.vx && /Keke/.test(r13.card) && r13.cleared, r13);
  const r14 = await run(() => { T4.fresh(); NH.invOpen('mission'); const empty = /No mission items/.test(document.querySelector('#inv-body').textContent); NH.invClose();
    const v = NH.spawnVehicle('danfo', 300, 300, 0, { parked: true }); NH.player.veh = v; v.driver = 'player'; NH.startShift('market', { owner: 'Baba Driver', mission: true });
    NH.invOpen('mission'); const names = [...document.querySelectorAll('#inv-body .icard span')].map(x => x.textContent), drop = !!document.querySelector('#inv-body [data-act="drop"]'); NH.invClose(); NH.endShift(); NH.closeOverlays(); NH.player.veh = null;
    return { empty, names, drop }; });
  ok('MISSION: keys and the money bag show during a shift and can’t be dropped', r14.empty && r14.names.some(n => /keys/.test(n)) && r14.names.some(n => /money bag/.test(n)) && !r14.drop, r14);

  // ---- save and load
  const r15 = await run(() => { T4.fresh(); NH.S.cash = 50000; NH.buyItem('bag:backpack'); NH.addItem('firstaid', 2); NH.S.bank = 777; NH.S.docs.licence = true; NH.S.outfits.push('fit_ankara_full'); NH.S.outfit = 'fit_ankara_full';
    const v = NH.spawnVehicle('sedan', 640, 480, 1, { parked: true, color: '#7b1e1e' }); NH.player.x = v.x - 24; NH.player.y = v.y; NH.pressed.add('KeyF'); NH.sim(.1); v.x = 660; NH.exitVehicle(); NH.S.wheel[7] = 'firstaid';
    NH.save(); const snap = JSON.parse(localStorage.getItem('naijahustle.v1'));
    // wipe and reload
    NH.S.inv = []; NH.S.bank = 0; NH.S.docs = {}; NH.S.garage = []; NH.S.bag = 'nylon'; NH.resetWorld(); NH.applyState(snap); NH.garageRestore();
    const back = NH.vehicles().find(x => x.gid === snap.garage[0].gid);
    return { aid: NH.invCount('firstaid'), bag: NH.S.bag, bank: NH.S.bank, lic: NH.S.docs.licence, fit: NH.S.outfit, wheel7: NH.S.wheel[7], veh: back && { type: back.type, x: Math.round(back.x), color: back.color } }; });
  ok('inventory, bag, bank, documents, clothing, wheel and vehicles save and load', r15.aid === 3 && r15.bag === 'backpack' && r15.bank === 777 && r15.lic && r15.fit === 'fit_ankara_full' && r15.wheel7 === 'firstaid' && r15.veh && r15.veh.type === 'sedan' && Math.abs(r15.veh.x - 660) < 30 && r15.veh.color === '#7b1e1e', r15);
  const r16 = await run(() => { const old = { cash: 9000, done: ['lag_01'], outfits: ['fit_street_basic'], outfit: 'fit_street_basic' }; for (const k of ['inv', 'wheel', 'bag', 'bank', 'docs', 'garage']) delete NH.S[k];
    NH.applyState(old); return { inv: NH.S.inv.length, docs: NH.S.docs, cap: NH.bagSlots() }; });
  ok('an old save without an inventory gets the starting items, and First Day grants the union card + papers', r16.inv === 8 && r16.docs.union && r16.docs.papers && !r16.docs.licence && r16.cap === 12, r16);

  ok('no JS errors (desktop)', !errors.length, errors);
  await page.context().close();

  // ---- touch: the bag button (tap = full screen, long-press = wheel), layout in portrait and landscape
  for (const vp of [{ name: 'phone portrait', width: 390, height: 844, touch: true }, { name: 'phone landscape', width: 844, height: 390, touch: true }, { name: 'tablet', width: 820, height: 1180, touch: true }]) {
    const { page: p, errors: e2 } = await open(b, vp);
    await p.evaluate(() => { NH.resetWorld(); NH.player.x = 300; NH.player.y = 480; NH.player.hp = 50; NH.closeOverlays(); });
    const bag = await p.$('#tb-bag'), bb = await bag.boundingBox();
    const overlap = await p.evaluate(() => { const r = q => document.querySelector(q).getBoundingClientRect(), a = r('#tb-bag'), out = [];
      for (const q of ['#tb-phone', '#tb-action', '#tb-enter', '#stick', '#hud-bl', '#minimap']) { const el = q === '#tb-phone' ? document.querySelector('#tbtns [data-b="phone"]') : document.querySelector(q); if (!el || !el.offsetWidth) continue; const c = el.getBoundingClientRect(); if (a.left < c.right && c.left < a.right && a.top < c.bottom && c.top < a.bottom) out.push(q); }
      return { out, onScreen: a.right <= innerWidth && a.bottom <= innerHeight && a.left >= 0 && a.top >= 0 }; });
    await p.mouse.move(bb.x + bb.width / 2, bb.y + bb.height / 2); await p.mouse.down(); await p.waitForTimeout(80); await p.mouse.up();
    const full = await p.evaluate(() => { const pan = document.querySelector('.inv-panel').getBoundingClientRect(), body = document.querySelector('#inv-body');
      const tapTargets = [...document.querySelectorAll('#inv-tabs button, #inv .ibtns button, #inv-x')].map(x => x.getBoundingClientRect()).filter(r => r.width).map(r => Math.min(r.width, r.height));
      const spill = [...document.querySelectorAll('.inv-panel *')].filter(e => e.offsetWidth && !e.closest('.inv-tabs') && e.getBoundingClientRect().right > pan.right + 1).map(e => e.className || e.tagName).slice(0, 4);
      return { open: NH.INV.open, spill, fits: !spill.length && pan.right <= innerWidth + 1 && pan.bottom <= innerHeight + 1 && pan.left >= -1 && pan.top >= -1, hscroll: body.scrollWidth > body.clientWidth + 2, minTap: Math.min(...tapTargets) }; });
    let hs = false; for (const tab of ['cash', 'docs', 'clothing', 'vehicles', 'mission']) hs = hs || await p.evaluate(t => { NH.INV.tab = t; NH.invRender(); const pan = document.querySelector('.inv-panel').getBoundingClientRect(), body = document.querySelector('#inv-body');
      return body.scrollWidth > body.clientWidth + 2 || [...document.querySelectorAll('#inv-body *')].some(e => e.offsetWidth && e.getBoundingClientRect().right > pan.right + 1) ? t : false; }, tab);
    const bb2 = await (await p.$('#tb-bag')).boundingBox(); const bagFree = await p.evaluate(([x, y]) => !!document.elementFromPoint(x, y).closest('#tb-bag'), [bb2.x + bb2.width / 2, bb2.y + bb2.height / 2]);
    await p.mouse.click(bb2.x + bb2.width / 2, bb2.y + bb2.height / 2); const closed = await p.evaluate(() => !NH.INV.open) && bagFree;
    // long-press → wheel, slide to slot 0 (pure water), let go
    await p.mouse.move(bb.x + bb.width / 2, bb.y + bb.height / 2); await p.mouse.down(); await p.waitForTimeout(550);
    const wOpen = await p.evaluate(() => NH.INV.wheel); const s0 = await (await p.$('#wring .wslot[data-w="0"]')).boundingBox();
    await p.mouse.move(s0.x + s0.width / 2, s0.y + s0.height / 2, { steps: 5 }); await p.mouse.up();
    const used = await p.evaluate(() => ({ hp: NH.player.hp, wheel: NH.INV.wheel }));
    ok(`${vp.name}: bag button sits in the button cluster without overlapping anything`, !overlap.out.length && overlap.onScreen, overlap);
    ok(`${vp.name}: tapping the bag opens the full screen; it fits, no sideways scroll, tap targets ≥ 40px; tapping again closes`, full.open && full.fits && !full.hscroll && !hs && full.minTap >= 40 && closed, { full, hs, closed });
    ok(`${vp.name}: long-press opens the wheel; sliding to a slot and letting go uses it`, wOpen && used.hp === 60 && !used.wheel, { wOpen, used });
    ok(`${vp.name}: no JS errors`, !e2.length, e2);
    await p.context().close();
  }
  console.log(fails ? `${fails} FAILED` : 'ALL PASS'); await b.close(); process.exit(fails ? 1 : 0);
})();
