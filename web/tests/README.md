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
node regression.js      # missions 1-2, chase, police still work
```
Requires Node with the `playwright` package available (`NODE_PATH=$(npm root -g)` if installed globally).
