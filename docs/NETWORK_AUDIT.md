# Network-safety audit (Step 1, before any change)

**Date:** 2026-10-09 · **Code:** `unreal/NaijaHustle/Plugins/NaijaHustleGame` at `a10f47e` (branch `lagos-real-city`), about 14,200 lines of C++.

**Verdict:** the game is single-player code. Two components were written with networking in mind; everything
else assumes one machine and one player. Nothing below has been changed yet.

How this was done: every source file was searched for replication (`Replicated`, `DOREPLIFETIME`, RPCs,
`HasAuthority`, roles), for "the one player" (`GetFirstPlayerController`, `GetActorOfClass`), for state held in
game-instance subsystems and singleton actors, for saves and config writes, for random rolls, and for direct
calls from gameplay into the HUD. It is a reading of the code, not a network test: nothing has been run with
two machines.

## What is already network-aware

| System | State |
|---|---|
| `UNHAdvancedMovementComponent` | Built for it: saved moves, compressed flags, client prediction data, two properties replicated to simulated proxies. **But it is not on the player** (`ANHCharacter` uses the stock movement component) and it has never been run with a second machine. |
| `UNHVehicleDynamicsComponent` | Replicates the slide state to non-owners and checks authority in one place. The vehicle it sits on does not replicate (below), so this is unused in practice. |

## What is not network-safe

Severity: **A** = breaks or can be cheated as soon as there is a second player; **B** = works only for the host; **C** = cosmetic or easy.

| # | System | What is wrong | Sev. |
|---|---|---|---|
| 1 | **Money, cred, integrity, wanted level, clock, finished missions** (`UNHHustleSubsystem`) | A game-instance subsystem: one copy per machine, nothing replicated, no owner. Any code anywhere calls `Earn()` or `AddHeat()` directly (31 call sites in 5 files). On a client this would change only that client's numbers; there is no server to refuse it. It is also one wallet, not one per player. | A |
| 2 | **Saves** | `UNHHustleSubsystem` and `ANHPhone` each write a local save slot whenever they like. Online, the client's disk would be the source of truth for its money. Settings in `GameUserSettings.ini` are fine to stay local. | A |
| 3 | **Vehicles** (`ANHVehicle`) | Does not replicate at all (`bReplicates` is never set). It is moved by `SetActorLocationAndRotation` on whichever machine has the controller, with no prediction or correction. Health, lock state, stolen/owned/tracker flags, alarm, headlights, paint and the driver's body are plain members. Entering is `Possess()` called straight from the player controller. | A |
| 4 | **Car stealing** (`ANHCarTheft`) | A singleton actor holding one player's hotwire, and deciding thefts, heat and sales locally. Sells a car by destroying it and calling `Earn()`. 11 uses of "the first player". | A |
| 5 | **Phone and DropAm** (`ANHPhone`) | A singleton with one player's contacts, friendships, chats, ride and driver job. Transfers, loans, fares and tips all call `Earn()` locally. Spawns the ride vehicle and the waiting passenger itself. 14 uses of "the first player". | A |
| 6 | **Missions and the conductor job** (`ANHGameDirector`) | One mission state machine for "the player" (`PlayerPawn()` is the first controller). Spawns the mission bus, passengers and Baba Driver; pays out; pauses and slows the clock for one player's deadline. This is story logic living in the shared world. | A |
| 7 | **Traffic** (`ANHTraffic`) | Spawns and moves vehicles around "the first player", with unseeded random rolls. Each machine would make its own, different traffic. Needs to run on the server only, around every player. | A |
| 8 | **Player character** (`ANHCharacter`) | Skin choice, shoes, torch, roll and climb are local. Roll writes velocity and climb switches off collision and teleports: neither goes through the movement component's saved moves, so a server would snap the player back. The stock movement component replicates walking and jumping only. | A |
| 9 | **Player controller** (`ANHPlayerController`) | Gets in and out of vehicles, answers panels and runs 20-odd console commands (`NHCash`, `NHGoto`, `NHFinish`…) with no authority check. Online these are cheats. | A |
| 10 | **Pause and slow motion** | The map and menu call `SetPause`, the inventory wheel sets global time dilation. Both would freeze or slow everybody. | A |
| 11 | **Pedestrians** (`ANHPerson`) | Not replicated; walking is a local tick. Spawned by the director, the phone and the theft code. | B |
| 12 | **Game data** (`UNHGameData`) | Read-only JSON loaded per machine: safe, provided server and clients have the same files. Switching between the small and real city by level name is done in the game mode, which only exists on the server: clients would not switch. | B |
| 13 | **Lighting and weather** (`ANHLightingRig`) | The preset is a local member; the clock that drives it is the unreplicated subsystem. Two players would see different times of day. | B |
| 14 | **HUD messages** | 83 direct calls to `ANHHUD::Toast` / `Floater` from gameplay code. On a dedicated server there is no HUD, and on a listen server they would show on the host's screen only. | B |
| 15 | **Character effects** (`UNHCharacterEffectsComponent`) | Wounds, bruises, sweat and wetness are local material state; ticks by distance from "the local camera". Hits would need to come from the server. | C |
| 16 | **Vehicle paint and crash marks** (`UNHVehicleMaterialComponent`) | Impacts applied locally where the crash is simulated. | C |
| 17 | **Randomness** | 64 unseeded `FMath::Rand*` calls in gameplay (fares, driver reactions, lock states, traffic mix). Fine if they only ever run on the server; wrong anywhere else. | C |

## Systems the brief names that do not exist yet

- **Inventory:** there is an inventory wheel on the HUD with six fixed actions. No item list, weights or slots.
- **Weapons:** none. (Step 2 of the earlier brief, not started.)
- **Doors:** none; vehicles have no doors. Lights and horns exist (items 3 above).
- **Task Force units and checkpoints:** none; "wanted" is a star count that only cools down.
- **Sound:** none at all.

These are easier: they can be written server-authoritative from the start.

## What I recommend, in order

1. **Split ownership first.** Move per-player state (money, cred, wanted level, outfit, phone, mission progress) off the game-instance subsystem onto a replicated `ANHPlayerState`, and world state (clock, weather) onto a replicated `ANHGameState`. Keep `UNHHustleSubsystem` as the single-player save front end only.
2. **One doorway for money.** All 31 `Earn`/`AddHeat` call sites go through server-only functions on the player state, with a log line each (the RP server needs that log anyway).
3. **Vehicles.** Replicate `ANHVehicle`; make entering, leaving, locks, lights, horn and damage Server RPCs with replicated results; dormancy for parked ones. Movement is the hard part: the kinematic drive needs either owner-authoritative movement with server checks (quick, cheatable) or a proper predicted move (slow to build). I would start with the first for co-op and say so.
4. **Player moves.** Put `UNHAdvancedMovementComponent` on the character and fold roll and climb into its saved moves.
5. **Server-only world.** Traffic, pedestrians, theft outcomes and DropAm dispatch run only with authority, around every player rather than the first.
6. **Story mode apart.** `ANHGameDirector` becomes the story-mode director, made only in single player; the conductor job is split out as a shared-world job that any player can do.
7. **Messages.** Replace the 83 direct HUD calls with a client RPC on the player controller (`ClientToast`), so the server can speak to one player.
8. **Local-only screens.** Map, menu and wheel stop pausing or slowing the world when there is more than one player.
9. **Lock the console commands** behind single-player or an admin flag.

On the engine side: turn on push-model replication from the start; Iris is worth evaluating before the Replication Graph, since 5.x development is going into Iris, but neither is needed for 2–4 players. I have not tested either on this project.

## Two things to decide before I start

- **This is a rewrite of how state is owned, not a patch.** Items 1–3 and 5–6 touch most files. Single-player must keep working throughout, and the scripted playtests (self-test, autoplay) have not been rerun since the Lagos level work; they should be made to pass again first so there is something to catch regressions.
- **The level does not fit the Mac.** `L_Lagos_City` measures 7.9 GB on the 8 GB machine before any crowd is added. Step 2 (15–25 full NPCs plus crowds) cannot be measured honestly until the city streams in around the player. I would do that before Step 2, whatever happens with Step 1.
