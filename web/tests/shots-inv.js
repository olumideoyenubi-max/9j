// Screenshots of the inventory screens, the quick wheel, a shop and the checkpoint (manual look): OUT=dir node shots-inv.js
const { chromium, open } = require('./harness');
const OUT = process.env.OUT || '/tmp/claude-0/shots';
(async () => {
  const b = await chromium.launch();
  for (const vp of [{ name: 'desktop', width: 1280, height: 760 }, { name: 'portrait', width: 390, height: 844, touch: true }, { name: 'landscape', width: 844, height: 390, touch: true }]) {
    const { page, errors } = await open(b, vp, { render3d: vp.name === 'desktop' });
    const shot = async (name, f, arg) => { await page.evaluate(f, arg); await page.evaluate(() => { if (!NH.three().ready) return; for (let i = 0; i < 3; i++) NH.renderOnce(i / 60); }); await page.waitForTimeout(350); await page.screenshot({ path: `${OUT}/inv-${vp.name}-${name}.png` }); };
    await page.evaluate(() => { NH.resetWorld(); NH.S.minutes = 15 * 60; NH.player.x = 300; NH.player.y = 480; NH.player.hp = 70; NH.S.cash = 23450; NH.S.outfits.push('fit_agbada_owambe');
      const v = NH.spawnVehicle('danfo', 340, 470, 0, { parked: true }); NH.player.x = v.x - 30; NH.player.y = v.y; NH.pressed.add('KeyF'); NH.sim(.1); NH.exitVehicle(); NH.player.x = 300; NH.player.y = 480; NH.sim(.1); });
    for (const tab of vp.name === 'desktop' ? ['items', 'cash', 'docs', 'clothing', 'vehicles', 'mission'] : ['items', 'docs', 'clothing']) await shot(tab, t => { NH.invOpen(t); NH.INV.tab = t; NH.invRender(); }, tab);
    await shot('wheel', () => { NH.invClose(); NH.invWheelOpen(); NH.INV.wsel = 2; NH.wheelRender(); });
    if (vp.name !== 'landscape') { await shot('shop', () => { NH.invWheelClose(false); NH.openShop('stall'); }); await shot('checkpoint', () => { NH.closeOverlays(); const v = NH.vehicles()[0]; NH.player.veh = v; v.driver = 'player'; NH.openCheckpoint(); }); }
    console.log(vp.name, errors.length ? errors : 'no errors'); await page.context().close();
  }
  await b.close();
})();
