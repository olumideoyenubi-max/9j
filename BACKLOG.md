# Backlog

The queue the backlog agent pulls from. Top unchecked item under **Ready** is the next one picked up.

**How to use it:** put work you want done under **Ready**, most important first. Move anything you
want left alone into **Icebox**. Tick an item when its PR is merged. An item is ready when someone
reading only this file and `CLAUDE.md` could do it without asking you a question.

Each item names the area in brackets so the agent knows which part of the repo it is touching:
`[core]`, `[content]`, `[web]`, `[runtime]`, `[docs]`, `[ci]`.

---

## Ready

- [ ] `[ci]` Run the browser-demo tests on pull requests, not just the Core tests. (Done by this
      PR's `web-tests.yml` — tick it once that has merged and passed a run.)
- [ ] `[content]` Port Harcourt has fewer ambient street events than Lagos. Add creek- and
      oil-city-flavoured ones to `portharcourt.json` so the city feels as alive as the mainland.
- [ ] `[content]` Radio stations have station names and slots but thin between-song patter. Write
      more presenter lines per station, each in that station's voice.
- [ ] `[web]` The demo has no pause menu. Add one on Escape (and a phone button) with Resume,
      Graphics, and Restart — matching the existing HUD styling.
- [ ] `[web]` Fuel runs down but there is no low-fuel warning before the engine cuts. Add a HUD
      warning and a radio/phone nudge toward the nearest Gidi Fuel.
- [ ] `[core]` `WantedSystem` decay is the same in all three cities. Make decay rate city-specific
      so Abuja heat sticks longer than Lagos, driven by data in `world.json`, and cover it in tests.
- [ ] `[docs]` `docs/TECH.md` does not mention the browser demo at all. Add a short section on how
      it relates to the Unity build and what it is for.

## In progress

<!-- The agent moves an item here when it opens a PR, with the PR number. -->

## Done

<!-- Tick and move items here once merged. -->

## Icebox

- `[runtime]` Unity scenes and prefabs — needs the Unity editor, so not agent work yet.
- `[unreal]` UE5 vertical slice — same.
- Art, audio and 3D assets — not agent work.
