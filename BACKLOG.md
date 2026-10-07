# Backlog

The queue the backlog agent pulls from. Top unticked item under **Ready** is the next one picked up.

**How to use it:** put work you want done under **Ready**, most important first. Move anything you
want left alone into **Icebox**. Tick an item when its PR is merged. An item is ready when someone
reading only this file and `CLAUDE.md` could do it without asking you a question.

Each item names the area in brackets: `[core]`, `[content]`, `[web]`, `[runtime]`, `[unreal]`,
`[docs]`, `[ci]`.

---

## The one thing that matters

**One district, in high definition, with a story chapter played inside it.** Not three cities. The
game already has more breadth than a solo developer can finish to this visual bar; what it does not
yet have is one street that proves the bar. Everything under **Ready** either serves that slice or
is small enough not to compete with it.

The slice is Oke-Erupe at dusk, the conductor chapter, and nothing else.

## Ready

- [ ] `[web]` Fuel runs down but there is no warning before the engine cuts. Add a HUD warning at
      low fuel and a phone or radio nudge toward the nearest Gidi Fuel. Follow the UI rules in
      `CLAUDE.md` — translucent, one accent colour, contextual.
- [ ] `[web]` There is no pause menu and Escape does nothing. Add one on Escape and on a phone
      button, with Resume, Graphics and Restart, in the demo's existing HUD style.
- [ ] `[web]` Audit the slice district against the visual bar in `CLAUDE.md` and write the findings
      into `docs/SLICE.md`: where the kerbs are flat, what has no grime pass, what has no level of
      detail, which props repeat visibly. Report only — no fixes in that PR.
- [ ] `[content]` Port Harcourt has three ambient street events to Lagos's four. Add creek- and
      oil-city-flavoured ones to `world.json` so the city feels as alive as the mainland.
- [ ] `[content]` Radio stations have names and slots but thin between-song patter. Write more
      presenter lines per station, each in that station's own voice.
- [ ] `[core]` `WantedSystem` decay is the same in all three cities. Make the decay rate
      city-specific so Abuja heat sticks longer than Lagos, driven by data in `world.json`, and
      cover it in tests.
- [ ] `[docs]` `docs/TECH.md` does not mention the browser demo or the optional texture and model
      pipelines. Add a section on how the three builds relate and what each is for.

## In progress

<!-- The agent moves an item here when it opens a PR, with the PR number. -->

## Done

<!-- Tick and move items here once merged. -->

## Icebox

Not agent work, or not yet:

- `[runtime]` Unity scenes and prefabs — needs the Unity editor.
- `[unreal]` The UE5 slice — built in a local Unreal session, not in CI.
- Art, audio and 3D assets. Agents can write the loaders, the fallbacks and the budgets; the meshes
  and textures themselves come from Blender.
- Online multiplayer. Worth building, worth not starting until the single-player slice is finished —
  it doubles the surface area of everything above.
