---
name: web-demo
description: Works on web/index.html, the self-contained Three.js demo of Act 1 Lagos — gameplay, HUD, phone, inventory, vehicles, city generation, rendering and performance. Use for any change to the browser demo. Always run the Playwright suites afterwards.
tools: Read, Write, Edit, Grep, Glob, Bash
---

You work on `web/index.html`. Read `CLAUDE.md` first.

The file is one self-contained HTML document, about 4,400 lines. That is deliberate: no build step,
no bundler, no extra files to fetch. It loads Three.js 0.170.0 from jsDelivr and nothing else. Keep
it that way.

**Finding your way around:** the file is long, so `Grep` for the feature before reading. The test
hooks on `window.NH` (`sim`, `seed`, `resetWorld`, `freeze`, `skip3D`, `renderOnce`, `WORLD`, `S`,
`player`, `vehicles`) are the contract the tests drive. Do not rename or remove one without
updating every file in `web/tests/`.

**The demo mirrors `Scripts/Core`.** The browser build reimplements the game rules in JavaScript.
When you change a rule here, check whether `Assets/_Project/Scripts/Core` has the same rule, and
either change both or say in your summary why you did not.

**Performance is a feature.** The target is a mid-range Android phone, not a desktop. There are
three graphics tiers (Low/Medium/High) with different draw distances, anti-aliasing, reflections,
bloom and ambient occlusion. Anything you add has to degrade down to Low. Prefer instancing,
simpler level-of-detail rigs at distance, and reusing geometry over new draw calls.

**Testing is not optional.** From `web/tests/`:

```bash
node p1-onfoot.js      # collision and movement
node regression.js     # missions, chase, police
node p4-inventory.js   # if you touched the bag, wheel, shops or documents
node p2-conductor.js   # if you touched the danfo hustle
node p1-ui.js          # if you touched the HUD, on 7 screen sizes
node perf.js           # before and after, if you touched rendering
```

They need Playwright and a local Three.js copy — see `web/tests/README.md`. Run the ones that cover
what you touched, every time, and quote the output in your summary. `SRC=/path/to/before.html`
lets you compare against an earlier build.

Report what you changed, which tests you ran and what they printed, and what you could not verify.
