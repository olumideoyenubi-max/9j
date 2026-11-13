# Naija Hustle — working rules

Read this before changing anything. It applies to every session, human or agent.

## What this repo is

An original open-world game set in fictionalised Lagos, Port Harcourt and Abuja. Five surfaces:

| Path | What it is | Can it be built here? |
|---|---|---|
| `Assets/_Project/Scripts/Core` | Engine-free C# game rules. The real game logic. | Yes — `dotnet test Tools/CoreTests` |
| `Assets/_Project/Resources/Content/*.json` | All content data: districts, missions, vehicles, shops, contacts. | Yes — validated by the Core tests |
| `Assets/_Project/Scripts/Runtime` | Unity 6 runtime (MonoBehaviours). | No — no Unity editor in CI |
| `web/index.html` | Self-contained Three.js demo of Act 1 Lagos. | Yes — `web/tests/*.js` |
| `unreal/NaijaHustle` | UE5 slice, with gameplay ported into the `NaijaHustleGame` plugin. | No |

`web/tools/export-unreal.js` writes the shared city and rules into the Unreal plugin's `Data`
folder, so the two 3D builds describe the same Lagos. Run it after changing the city layout or the
rules, and say in the PR that you did.

## Hard rules

1. **Everything is fictional.** No real brands, companies, politicians, police units, songs or
   landmarks — not in code, data, comments, docs or textures. The content test fails on known
   real-world names, and that test is not to be weakened. Invented look-alikes are the whole point:
   research the real place, ship the invented version.
2. **Content JSON is the source of truth.** Prices, rewards and heat values live in
   `Resources/Content/*.json`. `docs/GDD.md` describes that data; it does not define it. Change the
   JSON, then update the doc to match.
3. **Core logic stays engine-free.** Nothing in `Scripts/Core` may reference `UnityEngine`. It has
   to keep compiling and testing under plain `dotnet`.
4. **`web/index.html` is one file, and every asset it loads is optional.** No build step, no
   bundler, no required downloads. It loads Three.js 0.170.0 from jsDelivr, and it will use photo
   textures from `web/textures/` and GLB models from `web/models/` **when they are served over
   HTTP** — each one optional, with the procedural version kept whenever a file is missing or
   broken. Opening the page from `file://` skips them and the demo still has to work. Never make an
   external asset load-bearing.
5. **The browser demo mirrors Core.** When a rule changes in `Scripts/Core`, change it the same way
   in `web/index.html`, or say in the PR why it was left alone.
6. **Android-first budgets.** The target is a 3 GB Android phone. See `docs/TECH.md` for memory and
   draw budgets. A change that only runs well on a desktop is not finished.

## The visual bar

The game is aiming at a photoreal street. That makes placeholder geometry the main risk, so:

- **Never ship blocky placeholder geometry as a finished deliverable.** If a rough pass is all there
  is time for, say so explicitly and name what is missing. Silent placeholder work is the problem,
  not roughness itself.
- **Scale first.** Check doors, windows, kerbs, counters and seats against a 1.8 m human reference
  before detailing anything.
- **Build modular.** A kit on a grid — wall, window, doorframe, roof corner, storefront panel —
  instanced, not modelled one building at a time. One geometry, many transforms.
- **Detail is baked, not simulated.** Author fine detail offline and bake it to normal and roughness
  maps. No live cloth sim, no per-strand hair, no full subsurface scattering in a real-time build.
- **The grime pass is what makes it a place.** Streaks under windows, rust at fixings, dirt where
  hands and feet land, half-torn posters. Kerbs and pavements are never flat or perfectly straight.
- **Level of detail is mandatory** for anything that appears more than a few times: full rig near
  the camera, simpler mesh at mid distance, billboard far away.
- **Characters:** quads on the face, edge loops following the facial muscles — ocular, oral,
  nasolabial. Hair gets density, clump and a noise pass; perfect uniformity kills it.
- **Animation:** no foot sliding, ever. Animate the pelvis first to set the centre of gravity, then
  the limbs. Pin hands and feet with IK when riding or driving. Layer animations for crowds so
  pedestrians don't move in lockstep.
- **UI stays out of the way.** Diegetic first (the in-world phone for the map), then a subtle
  marker over an interactable, then minimal non-diegetic. Thin sans-serif, translucent blurred
  cards rather than solid boxes, neutral greys for ~90% with one accent reserved for interactive
  elements and warnings. Fade the UI out while exploring and bring it back near an interactable.

When the visual bar and the Android budget genuinely conflict, say so in the PR and give the
trade-off. Do not quietly drop the bar, and do not quietly blow the budget.

## Before opening a PR

```bash
dotnet test Tools/CoreTests          # rules, content validation, full playthrough
```

If `web/index.html` changed, from `web/tests/`:

```bash
node p1-onfoot.js && node regression.js       # always
node p4-inventory.js                          # if you touched the bag, shops or documents
node p2-conductor.js                          # if you touched the danfo hustle
node p5-live.js                               # if you touched crowds, hawkers or traffic
node p6-look.js                               # if you touched textures, models or the camera
node p7-hd.js                                 # if you touched materials or lighting
node perf.js                                  # before and after, if you touched rendering
```

They need Playwright and a local Three.js copy — see `web/tests/README.md`. CI runs each suite as
its own job, so a failure names itself in the Actions tab. A PR that fails them wastes a review.

Never weaken, skip or delete a test to make a build green. If a test is genuinely wrong, say so in
the PR and leave it failing for a human.

## House style

- Small PRs. One backlog item per branch, named `agent/<area>-<short-name>`.
- No new dependencies without saying why in the PR body.
- No secrets, keys or personal data in the repo, ever.
- British spelling in prose, as the rest of the docs use.
- Nigerian English in dialogue and in-game text — written the way it is actually spoken, never a
  caricature. Satire is aimed at power and bureaucracy, never at ordinary people's poverty and never
  at an ethnic or religious group.
- Never push to the default branch. Open a PR and let a human merge it.
