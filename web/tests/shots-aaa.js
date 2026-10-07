// Screenshots of the dusty-street look: Oke-Erupe at noon, a sprint, wheel dust, the helicopter (manual look): OUT=dir node shots-aaa.js
const { chromium, open } = require('./harness');
const OUT = process.env.OUT || '/tmp/claude-0/shots';
(async () => {
  const b = await chromium.launch();
  const { page, errors } = await open(b, { width: 1280, height: 760 }, { render3d: true });
  await page.evaluate(g => NH.applyGfx(g), process.env.GFX || 'high');
  const shot = async (name, f, frames = 4) => { await page.evaluate(f); await page.evaluate(n => { for (let i = 0; i < n; i++) NH.renderOnce(i / 30); }, frames); await page.screenshot({ path: `${OUT}/aaa-${name}.png` }); };
  const at = (x, y, yaw, min = 12.5 * 60) => `NH.resetWorld(); NH.seed(5); NH.S.minutes = ${min}; NH.WORLD.hour = -1; NH.player.veh = null; NH.player.x = ${x}; NH.player.y = ${y}; NH.CAMS.yaw = ${yaw}; NH.CAMS.dragT = performance.now() + 1e9; NH.sim(6);`;
  await shot('dusty-street', new Function(at(6 * 32 + 16, 35 * 32 + 16, Math.PI / 2)));
  await shot('dusty-buildings', new Function(at(6 * 32 + 16, 35 * 32 + 16, Math.PI * .8)));
  await shot('dusty-street2', new Function(at(3 * 32, 35 * 32 + 16, 0)));
  await shot('sprint', new Function(`${at(10 * 32, 35 * 32 + 16, 0)} NH.keys.ShiftLeft = true; NH.keys.KeyW = true; NH.sim(1.2);`), 30);
  await page.evaluate(() => { NH.keys.KeyW = false; NH.keys.ShiftLeft = false; });
  await shot('wheel-dust', new Function(`${at(8 * 32 + 16, 30 * 32 - 20, -Math.PI / 2)} const s = NH.roadSpotNear(NH.player, 0, 120); const v = NH.spawnVehicle('danfo', s.x, s.y, s.a, { parked: true }); NH.player.veh = v; v.driver = 'player'; NH.keys.KeyW = true; NH.sim(2.5); NH.keys.KeyW = false;`), 10);
  await shot('heli', new Function(`${at(300, 480, -Math.PI / 2)} NH.addHeat(3); NH.sim(6);`), 6);
  await shot('heli-night', new Function(`${at(300, 480, -Math.PI / 2, 22 * 60)} NH.addHeat(3); NH.sim(6);`), 6);
  await shot('market-noon', new Function(at(1000, 1300, -Math.PI / 2)));
  console.log(errors.length ? errors : 'no errors'); await b.close();
})();
