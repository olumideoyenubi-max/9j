# The agent crew

Background agents that carry Naija Hustle forward between your own sessions. They work the same way
a contributor would: branch, change, test, pull request. Nothing reaches the default branch without
you merging it.

## What runs, and what it costs

| Workflow | When | Spends model tokens? |
|---|---|---|
| `core-tests.yml` | every push and PR | no |
| `web-tests.yml` | push/PR touching `web/`, plus nightly for the slow suites | no |
| `claude.yml` | when you write `@claude` in an issue or PR comment | yes |
| `agent-backlog.yml` | Mondays 06:07 Lagos, or on demand | yes |

The two test workflows are the safety net and they are free apart from Actions minutes. Only the two
Claude workflows cost tokens, and both are capped with `--max-turns`. If the weekly backlog run is
too slow or too fast for you, change the `cron` line in `agent-backlog.yml` — or delete the
`schedule:` block entirely and run it by hand from the Actions tab when you want work done.

## Setup, once

1. Install the [Claude GitHub App](https://github.com/apps/claude) on this repository.
2. Add a repository secret so the workflows can authenticate:
   - `ANTHROPIC_API_KEY` — an API key from the [Claude Console](https://platform.claude.com), billed
     per token; or
   - `CLAUDE_CODE_OAUTH_TOKEN` — generated with `claude setup-token`, which bills against your Claude
     subscription instead. If you use this one, change the `anthropic_api_key:` line in
     `claude.yml` and `agent-backlog.yml` to `claude_code_oauth_token: ${{ secrets.CLAUDE_CODE_OAUTH_TOKEN }}`.

   The repository is public, so GitHub withholds secrets from pull requests opened from forks. Agent
   runs only happen on branches in this repository.
3. That is it. Add work to `BACKLOG.md` and the Monday run picks up the top item.

## How work gets in

`BACKLOG.md` is the queue. Put what you want done under **Ready**, most important first. The backlog
agent takes the top unticked item, does that one item, opens a PR, and moves the item to **In
progress** with the PR number. You review and merge, or close it and reword the item.

An item is ready when someone who has read only `BACKLOG.md` and `CLAUDE.md` could do it without
asking you a question. Vague items produce vague pull requests.

To jump the queue, go to **Actions → Backlog agent → Run workflow** and type the item in the box.

## The specialists

`.claude/agents/` holds four agents you can hand work to in a Claude Code session, on your laptop or
inside a workflow run. Each one knows its corner of the repo and which tests to run:

- **content-writer** — the content JSON: districts, missions, hustles, vehicles, shops, radio,
  ambient events. The "make the world bigger" agent.
- **core-rules** — the engine-free C# in `Scripts/Core` and its tests. The "change how the game
  works" agent.
- **web-demo** — `web/index.html`. Gameplay, HUD, rendering and performance in the browser build.
- **qa** — runs every suite, triages failures, reports. Never edits anything.

In a session: *"use the content-writer agent to add creek events to Port Harcourt"*.

## What agents are not for

Scenes, prefabs, 3D models, textures, animation and audio. Those need the Unity editor and a human
eye, and they are parked in the **Icebox** in `BACKLOG.md`. The crew is for code, data, tests and
docs — which is most of what is left before the art goes in.

## Working on an 8 GB machine

Let CI carry the weight. Playwright plus a Chromium download plus Unity plus a `dotnet` build is a
lot to keep on a laptop with little free storage. The `dotnet test` run is light and worth having
locally; the browser suites are better left to `web-tests.yml`, which runs them on every push that
touches `web/`. Push the branch, read the run, fix, push again.
