# Browser demo tests

Headless Chromium (Playwright) tests for `web/index.html`. Each test drives the real game through
the `window.NH` test hooks (`sim`, `seed`, `resetWorld`, `freeze`), so runs are deterministic and
much faster than real time.

```bash
cd web/tests
node p1-collisions.js   # vehicles never end up inside buildings/stalls (80 random drives)
node p1-onfoot.js       # player can't walk into walls, exits land on free ground, head-on crash bounces
node p1-traffic.js      # 20 x 60s of traffic: no overlaps, no AI crashes, junction give-way, no gridlock
node p1-ui.js           # HUD layout on 7 screen sizes, job card, notifications, fuel, labels, controls
node p2-conductor.js    # passengers, routes, CALL, change, comfort, full bus, missed stops, agberos, summary, go-slow
node p3-first-day.js    # "First Day on the Danfo" end to end: dialogue, route, agbero, summary, fail + retry, unlock
node p4-inventory.js    # inventory: Tab tap/hold, bag button, wheel, items, capacity, shops, documents, checkpoint, cash, clothing, vehicles, save
node p5-live.js         # crowds, rush hour, hawkers, kerb stops, lane splitting, honks, scenes, clutter, sound
node p6-look.js         # photo textures + GLB models over HTTP (generated fixtures), fallbacks, helicopter, sprint camera, dust
node p7-hd.js           # 2x atlases on High, normal maps, glass, shop interiors, ground decals, god rays, heat shimmer
node regression.js      # missions 1-2, chase, police still work
```
Requires Node with the `playwright` package available (`NODE_PATH=$(npm root -g)` if installed globally).
The page loads Three.js from jsDelivr; the harness serves those requests from a local copy of the
npm package instead (`npm pack three@0.170.0`, extract it, and point `THREE_DIR` at the `package`
folder; default `/tmp/claude-0/three-pkg/package`). Chromium renders WebGL with SwiftShader.

```bash
node perf.js            # average frame time (software WebGL, so numbers are pessimistic)
node shots-inv.js       # screenshots of the inventory tabs, quick wheel, a shop and the checkpoint (desktop, portrait, landscape)
node shots-hd.js        # screenshots of shop interiors (day/night), ground decals, sunset/sunrise and noon roads
node shots-aaa.js       # screenshots of the dusty street, a sprint, wheel dust and the helicopter (GFX=high|medium|low)
node shots-live.js      # screenshots of the crowd, rush hour with hawkers, and each street scene
node shots-vis.js       # screenshots of the danfo, keke and a line-up of characters into $OUT (look at them, not pass/fail)
```

Set `SRC=/path/to/other.html` to run any test against another build (for before/after comparisons).

Unreal export (shares the city with the UE5 slice; not a test):
```bash
node ../tools/export-unreal.js   # writes lagos_city.json + naija_rules.json into unreal/NaijaHustle/Plugins/NaijaHustleGame/Data
```
