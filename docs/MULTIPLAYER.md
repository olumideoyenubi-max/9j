# NAIJA HUSTLE: Getting server-ready

> The plan for taking NAIJA HUSTLE from single-player to GTA RP-style servers: a shared Lagos where many players
> live, work, hustle and roleplay together. The game isn't on servers yet. This is how we build now so it can
> get there without a rewrite.

## The two ways to play

| | **Story** | **Lagos Life (RP servers)** |
|---|---|---|
| Who you are | Tunde and Amaka | Your own character, made in a character creator |
| Players | 1, or 2 in co-op later (one plays Tunde, one plays Amaka) | Many on one server (start with 32) |
| What you do | The 12 missions in STORY.md | Free roam: jobs, interactions, hustles, roleplay (LIFE.md, HUSTLES.md) |
| Money | Night-shift pay and story money | Earned on the server only |

Tunde, Amaka and the story cast stay story characters, so a server isn't full of Tundes. They can appear on servers
as NPCs.

## Rules for code we write now

These apply to every system from today, even while there's only one player.

1. **The server decides.** Money, damage, wanted stars, purchases, mission results, scams and bets are worked out
   on the server. The client only asks ("I want to buy this") and shows the answer. With one player, the player's
   own machine is the server, so nothing changes for single-player.
2. **State goes in the right place:**

   | State | Where it lives |
   |---|---|
   | Cash, Cred, Integrity, needs, respect, inventory, outfits, unlocks | The player's PlayerState (a replicated component) |
   | Clock, weather, power cuts, market prices, bet results | The GameState |
   | Story flags and mission progress | The player's state in story mode |
   | Vehicles, doors, pickups, NPCs | Replicated actors |
   | Menus, HUD, wheels | The client only, showing replicated state, never holding it |

3. **Replicate from the start:** replicated properties with change notifications, and server RPCs for every
   action (`ServerBuy`, `ServerInteract`, `ServerEmote`). No important state in static globals or the
   GameInstance.
4. **Random results come from the server's seed,** so every player sees the same bet result, stock price and
   scam.
5. **Saving goes through one interface.** Today it writes the local save slot. On servers it will write to a
   database, without changing game code.
6. **Animations and interactions are networked:** montages are started by the server and played on every
   client, and emotes and dances sync, so a whole owambe can dance together.
7. **Keep it cheap:** replicate only what other players need, use relevancy (distant players don't get updates),
   and keep the update rate low for crowds and parked cars.
8. **Test with two:** each autoplay test also runs as a listen server with one extra client, which works on the
   Mac with the normal Unreal engine.

## Economy on servers

- **Gold Kobo is cosmetic-only on servers:** outfits, car paint, emotes, dances and apartment decor. **No buying
  naira or cars on servers**, so nobody can pay to win.
- Bought naira and premium story characters stay in story mode.
- Betting, stocks and crypto use server money with the same limits as HUSTLES.md. Night companions are off on
  servers.

## What RP servers add (later)

- **Character creator:** face, body, hair, outfits, name, home area.
- **Player jobs**, including roles other players depend on:
  - danfo, okada and keke drivers
  - mechanics
  - clinic medics, who revive downed players
  - Task Force officers who can stop, search and arrest (with rules)
  - market traders
- **Chat:** proximity voice chat, text chat and RP commands such as `/me` and `/do`.
- **Property:** rent a room, a shop or a stall, and keep it between sessions.
- **Moderation:**
  - server rules
  - player reports
  - admin tools: kick, ban, spectate, teleport, give and take
  - logs of every money change
- **Safe zones** (hospitals, the park) where fighting is off.

## Milestones

| # | Milestone | Where |
|---|---|---|
| 1 | **Server-ready code:** all systems replicate, and the autoplay tests pass with 2 players in a listen server | The Mac, now, alongside the build prompt |
| 2 | **Lagos Life mode:** a character creator and free roam with jobs and interactions, playable with friends over a listen server | The Mac |
| 3 | **Dedicated server build** for Linux | A cloud build machine. Dedicated servers need Unreal built from source, which needs far more memory and disk than an 8 GB Mac has. |
| 4 | **Accounts and saving** in a database, with log-in and anti-cheat checks | Cloud |
| 5 | **Moderation and admin tools** | Cloud |
| 6 | **Closed test** with 16–32 players | Cloud hosting |
