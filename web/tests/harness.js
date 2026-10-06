// Shared Playwright harness for the browser demo. Wraps web/index.html in the same skeleton the
// artifact host adds, opens it, and clicks Start. Usage: const { open } = require('./harness');
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path'), os = require('os');
const SRC = path.join(__dirname, '..', 'index.html');
function build() {
  const out = path.join(os.tmpdir(), 'naija-hustle-test.html');
  fs.writeFileSync(out, '<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"></head><body>' + fs.readFileSync(SRC, 'utf8') + '</body></html>');
  return 'file://' + out;
}
async function open(browser, vp = { width: 1280, height: 760 }, opts = {}) {
  const ctx = await browser.newContext({ viewport: { width: vp.width, height: vp.height }, hasTouch: !!vp.touch, isMobile: !!vp.touch });
  const page = await ctx.newPage(); const errors = [];
  // deterministic runs: seeded Math.random
  await page.addInitScript(seed => { let s = seed >>> 0 || 1; Math.random = () => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; }; }, opts.seed || 12345);
  page.on('pageerror', e => errors.push(e.message)); page.on('console', m => m.type() === 'error' && errors.push(m.text()));
  await page.goto(build()); await page.waitForTimeout(opts.wait || 1200);
  if (!opts.noStart) { await page.click('#btn-play'); await page.waitForTimeout(400); if (!opts.live) await page.evaluate(() => NH.freeze()); }
  return { ctx, page, errors };
}
module.exports = { chromium, open };
