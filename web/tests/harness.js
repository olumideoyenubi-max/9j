// Shared Playwright harness for the browser demo. Wraps web/index.html in the same skeleton the
// artifact host adds, opens it, and clicks Start. Usage: const { open } = require('./harness');
// Three.js is served from a local copy of the npm package (set THREE_DIR, default below), so tests
// don't need the CDN; Chromium runs WebGL through SwiftShader.
const { chromium: pw } = require('playwright');
const fs = require('fs'), path = require('path'), os = require('os');
const SRC = path.join(__dirname, '..', 'index.html');
const THREE_DIR = process.env.THREE_DIR || '/tmp/claude-0/three-pkg/package';
const chromium = { launch: (o = {}) => pw.launch(Object.assign({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] }, o)) };
function build() {
  const out = path.join(os.tmpdir(), 'naija-hustle-test.html');
  fs.writeFileSync(out, '<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"></head><body>' + fs.readFileSync(SRC, 'utf8') + '</body></html>');
  return 'file://' + out;
}
async function open(browser, vp = { width: 1280, height: 760 }, opts = {}) {
  // reduced motion: the HUD's CSS fades composite slowly over a software-rendered WebGL canvas, which
  // would starve the frame loop (and the real-time toast/banner timers) in headless runs
  const ctx = await browser.newContext({ viewport: { width: vp.width, height: vp.height }, hasTouch: !!vp.touch, isMobile: !!vp.touch, reducedMotion: opts.render3d ? 'no-preference' : 'reduce' });
  await ctx.route(/cdn\.jsdelivr\.net\/npm\/three@[^/]+\/(.*)$/, route => {
    const rel = route.request().url().replace(/^.*\/npm\/three@[^/]+\//, ''); const f = path.join(THREE_DIR, rel);
    if (!fs.existsSync(f)) return route.fulfill({ status: 404, body: 'missing ' + rel });
    route.fulfill({ status: 200, contentType: 'application/javascript', headers: { 'access-control-allow-origin': '*' }, body: fs.readFileSync(f) });
  });
  await ctx.route(/fonts\.(googleapis|gstatic)\.com/, route => route.abort());
  const page = await ctx.newPage(); const errors = [];
  // deterministic runs: seeded Math.random
  await page.addInitScript(seed => { let s = seed >>> 0 || 1; Math.random = () => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; }; }, opts.seed || 12345);
  page.on('pageerror', e => errors.push(e.message)); page.on('console', m => m.type() === 'error' && !/ERR_FAILED|net::/.test(m.text()) && errors.push(m.text()));
  await page.goto(build()); await page.waitForFunction(() => window.NH && NH.three().ready, null, { timeout: 30000 }).catch(() => {}); await page.waitForTimeout(opts.wait || 600);
  // software WebGL is slow; logic/UI tests skip drawing the city so real-time timers keep up
  // (and let the GPU process drain the 3D frames the title screen already queued)
  if (!opts.render3d) await page.evaluate(() => window.NH && NH.skip3D(true));
  const settle = p => p.evaluate(() => { const t0 = performance.now(); let fast = 0, last = t0;
    return new Promise(res => { const f = now => { fast = now - last < 50 ? fast + 1 : 0; last = now; if (fast >= 8 || now - t0 > 20000) res(); else requestAnimationFrame(f); }; requestAnimationFrame(f); }); });
  if (!opts.noStart) { await page.click('#btn-play'); await page.waitForTimeout(400); if (!opts.render3d) await settle(page); if (!opts.live) await page.evaluate(() => NH.freeze()); }
  return { ctx, page, errors };
}
module.exports = { chromium, open };
