// Screenshots of the vehicle and character art, for a manual look (not pass/fail): OUT=dir node shots-vis.js
const { chromium, open } = require('./harness');
const OUT = process.env.OUT || '/tmp/claude-0/shots';
const KINDS = ['tee', 'shirt', 'ankara', 'agbada', 'kaftan', 'wrapper', 'hijab', 'lady', 'school', 'uniform', 'conductor'];
(async () => {
  const b = await chromium.launch();
  const { page, errors } = await open(b, { width: 1280, height: 760 }, { render3d: true });
  const shot = async (name, setup, arg, clip) => {
    await page.evaluate(setup, arg); await page.evaluate(() => { for (let i = 0; i < 6; i++) NH.renderOnce(i / 60); });
    await page.screenshot({ path: `${OUT}/${name}.png`, clip }); };
  await page.evaluate(() => { NH.resetWorld(); NH.seed(5); NH.WORLD.noJams = true; NH.S.minutes = 15 * 60; NH.applyGfx('high'); });
  const ped = (k, x, y) => ({ x, y, a: Math.PI / 2, tx: 0, ty: 0, spd: 0, state: 'idle', t: 99, _kind: k, tie: k === 'wrapper' ? '#e53935' : null, tray: k === 'wrapper' ? 'orange' : null });
  await shot('lineup', K => { NH.teleport(300, 330); NH.clearVehicles(); const P = NH.peds(); P.length = 0; K.forEach((k, i) => P.push({ x: 262 + i * 7.5, y: 304, a: Math.PI / 2, tx: 0, ty: 0, spd: 0, state: 'idle', t: 99, _kind: k }));
    NH.player.x = 300; NH.player.y = 336; NH.player.a = -Math.PI / 2; NH.CAMS.yaw = -Math.PI / 2; NH.CAMS.dragT = performance.now() + 1e9; }, KINDS);
  await shot('faces', K => { const P = NH.peds(); P.length = 0; K.forEach((k, i) => P.push({ x: 286 + i * 5, y: 326, a: Math.PI / 2, tx: 0, ty: 0, spd: 0, state: 'idle', t: 99, _kind: k, tie: k === 'wrapper' ? '#e53935' : null }));
    NH.player.x = 300; NH.player.y = 345; }, ['lady', 'shirt', 'wrapper', 'hijab', 'tee', 'agbada'], { x: 340, y: 160, width: 600, height: 300 });
  await shot('danfo-door', () => { NH.peds().length = 0; NH.clearVehicles(); const d = NH.spawnVehicle('danfo', 300, 300, 0); d.driver = 'ai'; d._seed = .9; d.dest = 'Balo'; NH.vehicles().push(d);
    NH.player.x = 315; NH.player.y = 345; NH.player.a = -Math.PI / 2; NH.CAMS.yaw = -Math.PI / 2 - .5; });
  await shot('danfo-front', () => { NH.player.x = 360; NH.player.y = 320; NH.player.a = Math.PI; NH.CAMS.yaw = Math.PI + .5; });
  await shot('danfo-rear', () => { NH.player.x = 255; NH.player.y = 296; NH.player.a = 0; NH.CAMS.yaw = .35; });
  await shot('keke-side', () => { NH.clearVehicles(); const k = NH.spawnVehicle('keke', 300, 300, 0); k.driver = 'ai'; k._seed = .8; k.color = '#f5c400'; NH.vehicles().push(k);
    NH.player.x = 306; NH.player.y = 330; NH.player.a = -Math.PI / 2; NH.CAMS.yaw = -Math.PI / 2 - .7; });
  await shot('keke-rear', () => { NH.player.x = 270; NH.player.y = 306; NH.player.a = 0; NH.CAMS.yaw = .3; });
  console.log(errors.length ? 'ERRORS ' + errors.join('\n') : 'no errors');
  await b.close();
})();
