// Screenshots of the live street: crowds, rush-hour traffic with hawkers, the scenes and clutter (manual look): OUT=dir node shots-live.js
const { chromium, open } = require('./harness');
const OUT = process.env.OUT || '/tmp/claude-0/shots';
(async () => {
  const b = await chromium.launch();
  const { page, errors } = await open(b, { width: 1280, height: 760 }, { render3d: true });
  const shot = async (name, f) => { await page.evaluate(f); await page.evaluate(() => { for (let i = 0; i < 4; i++) NH.renderOnce(i / 60); }); await page.screenshot({ path: `${OUT}/live-${name}.png` }); };
  const at = (x, y, yaw, min = 13 * 60) => `NH.resetWorld(); NH.seed(5); NH.S.minutes = ${min}; NH.WORLD.hour = -1; NH.player.veh = null; NH.player.x = ${x}; NH.player.y = ${y}; NH.CAMS.yaw = ${yaw}; NH.CAMS.dragT = performance.now() + 1e9; NH.sim(8);`;
  await shot('crowd', new Function(at(300, 480, -Math.PI / 2)));
  await shot('market', new Function(at(1000, 1300, -Math.PI / 2)));
  await shot('rush', new Function(`${at(300, 480, 0, 8 * 60)} const s = NH.roadSpotNear(NH.player, 0, 200); const v = NH.spawnVehicle('sedan', s.x, s.y, s.a, { parked: true }); NH.player.veh = v; v.driver = 'player'; NH.sim(12); NH.CAMS.yaw = v.a;`));
  await shot('scenes-agbero', new Function(at(256, 300, Math.PI / 2)));
  await shot('owambe', new Function(`const sp = NH.liveSpots().party; ${at(0, 0, -Math.PI / 2, 5 * 1440 + 15 * 60)} NH.player.x = sp.x; NH.player.y = sp.y + 70; NH.sim(2);`));
  await shot('football', new Function(`const sp = NH.liveSpots().foot; ${at(0, 0, Math.PI)} NH.player.x = sp.x + 56; NH.player.y = sp.y; NH.sim(2);`));
  await shot('preacher', new Function(`const sp = NH.liveSpots().preach; ${at(0, 0, -Math.PI / 2)} NH.player.x = sp.x; NH.player.y = sp.y + 60; NH.sim(2);`));
  await shot('warden', new Function(`const sp = NH.liveSpots().warden; ${at(0, 0, -Math.PI / 2)} NH.player.x = sp.x + 10; NH.player.y = sp.y + 45; NH.sim(2);`));
  await shot('clutter', new Function(`const s = NH.STATICS[3]; ${at(0, 0, -Math.PI / 2)} NH.player.x = s.x / (1 / 8) + 10; NH.player.y = s.z / (1 / 8) + 70; NH.sim(2);`));
  console.log(errors.length ? errors : 'no errors'); await b.close();
})();
