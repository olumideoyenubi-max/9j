// Render-cost probe: average time to draw one frame in a busy street scene (headless Chromium,
// software rendering, so absolute numbers are pessimistic; use it to compare before/after).
const { chromium, open } = require('./harness');
(async () => {
  const b = await chromium.launch();
  for (const vp of [{ name: 'desktop', width: 1280, height: 760 }, { name: 'phone', width: 390, height: 844, touch: true }]) {
    const { page } = await open(b, vp);
    const r = await page.evaluate(() => {
      NH.resetWorld(); NH.seed(3); NH.WORLD.noJams = true; NH.S.minutes = 13 * 60; NH.player.x = 900; NH.player.y = 560; NH.sim(8);
      const out = {};
      for (const [label, min] of [['noon', 13 * 60], ['night', 22 * 60]]) {
        NH.S.minutes = min; for (let i = 0; i < 10; i++) NH.renderOnce(i / 60);
        const t0 = performance.now(); const N = 120; for (let i = 0; i < N; i++) NH.renderOnce(i / 60); out[label] = +((performance.now() - t0) / N).toFixed(2);
      }
      out.vehicles = NH.vehicles().length; return out;
    });
    console.log(vp.name, JSON.stringify(r)); await page.context().close();
  }
  await b.close();
})();
