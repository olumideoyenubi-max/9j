// Shared HUD overlap checker: pieces that must never overlap each other or leave the screen.
const VIEWPORTS = [
  { name: 'desktop 1280x760', width: 1280, height: 760 }, { name: 'desktop 1024x640', width: 1024, height: 640 },
  { name: 'phone portrait 360x740', width: 360, height: 740, touch: true }, { name: 'phone portrait 412x915', width: 412, height: 915, touch: true },
  { name: 'phone landscape 740x360', width: 740, height: 360, touch: true }, { name: 'phone landscape 844x390', width: 844, height: 390, touch: true },
  { name: 'tablet 820x1180', width: 820, height: 1180, touch: true },
];
const PIECES = ['#cash', '#stars', '#unit', '#job', '#clock', '#minimap', '#district', '#radio', '#toasts .toast', '#hud-bl', '#prompt', '#cpanel', '#tbtns', '#stick', '#card'];
async function layout(page) {
  return page.evaluate(PIECES => {
    const R = []; for (const sel of PIECES) { const el = document.querySelector(sel); if (!el) continue;
      const cs = getComputedStyle(el); if (el.closest('[hidden]') || cs.display === 'none' || cs.visibility === 'hidden') continue;
      const r = el.getBoundingClientRect(); if (r.width < 1 || r.height < 1) continue;
      R.push({ sel, x0: r.left, y0: r.top, x1: r.right, y1: r.bottom }); }
    const overlaps = [], off = [];
    for (let i = 0; i < R.length; i++) {
      const a = R[i]; if (a.x0 < -0.5 || a.y0 < -0.5 || a.x1 > innerWidth + .5 || a.y1 > innerHeight + .5) off.push(a.sel);
      for (let j = i + 1; j < R.length; j++) { const b = R[j];
        if (a.x0 < b.x1 - 1 && b.x0 < a.x1 - 1 && a.y0 < b.y1 - 1 && b.y0 < a.y1 - 1) overlaps.push(a.sel + ' × ' + b.sel); } }
    return { overlaps, off, n: R.length, hscroll: document.documentElement.scrollWidth > innerWidth };
  }, PIECES);
}
module.exports = { VIEWPORTS, PIECES, layout };
