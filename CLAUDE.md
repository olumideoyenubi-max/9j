# Naija Hustle — working rules

Read this before changing anything. It applies to every session, human or agent.

## What this repo is

An original open-world game set in fictionalised Lagos, Port Harcourt and Abuja. Four surfaces:

| Path | What it is | Can it be built here? |
|---|---|---|
| `Assets/_Project/Scripts/Core` | Engine-free C# game rules. The real game logic. | Yes — `dotnet test Tools/CoreTests` |
| `Assets/_Project/Resources/Content/*.json` | All content data: districts, missions, vehicles, shops, contacts. | Yes — validated by the Core tests |
| `Assets/_Project/Scripts/Runtime` | Unity 6 runtime (MonoBehaviours). | No — no Unity editor in CI |
| `web/index.html` | Self-contained Three.js demo of Act 1 Lagos. | Yes — `web/tests/*.js` |
| `unreal/NaijaHustle` | UE5 vertical slice. | No |

## Hard rules

1. **Everything is fictional.** No real brands, companies, politicians, police units, songs or
   landmarks — not in code, data, comments or docs. The content test fails on known real-world
   names, and that test is not to be weakened. Invented look-alikes are the whole point.
2. **Content JSON is the source of truth.** Prices, rewards and heat values live in
   `Resources/Content/*.json`. `docs/GDD.md` describes that data; it does not define it. Change the
   JSON, then update the doc to match.
3. **Core logic stays engine-free.** Nothing in `Scripts/Core` may reference `UnityEngine`. It has
   to keep compiling and testing under plain `dotnet`.
4. **`web/index.html` is one file on purpose.** No build step, no bundler, no new files to load. It
   loads Three.js 0.170.0 from jsDelivr and nothing else.
5. **The browser demo mirrors Core.** When a rule changes in `Scripts/Core`, change it the same way
   in `web/index.html`, or say in the PR why it was left alone.
6. **Android-first budgets.** The target is a 3 GB Android phone. See `docs/TECH.md` for memory and
   draw budgets. A change that only runs well on a desktop is not finished.

## Before opening a PR

```bash
dotnet test Tools/CoreTests          # 37 tests: rules, content validation, full playthrough
cd web/tests && node p1-onfoot.js && node regression.js    # if web/index.html changed
```

Both run in CI too, but a PR that fails them wastes a review.

## House style

- Small PRs. One backlog item per branch, named `agent/<area>-<short-name>`.
- No new dependencies without saying why in the PR body.
- No secrets, keys or personal data in the repo, ever.
- British spelling in prose, as the rest of the docs use.
- Nigerian English in dialogue and in-game text — write it the way it is actually spoken, not a
  caricature.
- Never push to the default branch. Open a PR and let a human merge it.
