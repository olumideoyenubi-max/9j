# What NAIJA HUSTLE takes from the GTA multiplayer frameworks

**Date:** 2026-10-10 · **Branch:** `lagos-real-city`

The user pointed at six references and asked for three things from them: a list of the systems the game is missing,
a design for multiplayer, and menus in their style. This is that, and what has been built from it so far.

None of their code is in the game and none can be: they are Lua, JavaScript and C++ written against GTA V's own
engine, and they need GTA V to run. What is taken is how they are put together.

| Reference | What it is | What is taken from it |
|---|---|---|
| FiveM (CitizenFX) | A multiplayer platform on top of GTA V: a server that owns the world, "resources" that add features, entity state the server replicates | The server decides, clients ask. A feature is a self-contained piece with its own data file. |
| RAGE Multiplayer C++ SDK (MIT) | The same idea with a C++ plugin interface: entities, pools of them, events | Entities that exist only near a player ("streamed in"), and events as the only way a client changes anything |
| QBCore | A roleplay framework on FiveM: one player record, a shared item list, jobs, and about forty feature resources | The player record, the shape of the item list, and the list of features below |
| Athena Framework | A roleplay framework on alt:V with a plugin system | Not read: the repository link no longer resolves (404 on 2026-10-10) |
| GTAVMenuBase | A library for GTA V mod menus | The list menu: banner, heading bar with "3 / 12", lit row, value at the right, help under the list |

## 1. Systems: what the frameworks have, and where the game stands

"Have" means it is in the game now; "part" that something like it exists but not as a system; "none" that it is not there.

| System (as QBCore names it) | In NAIJA HUSTLE | State |
|---|---|---|
| Player record: money, job, gang, metadata | Money, cred, integrity, wanted level on `ANHPlayerState`, replicated | Have, for the host; other players' saves are not done |
| Shared item list and inventory | `Data/items.json`, `UNHInventoryComponent`: weight limit, server-owned, replicated to its owner | **Built 2026-10-10** (below) |
| Weapons and ammunition as items | A weapon can only be held if it is carried; every shot uses a round | **Built 2026-10-10** |
| Shops | None. Items have prices, nothing sells them | none |
| Bank, cash and ATM | Cash only; transfers and loans on the phone | part |
| Phone | Contacts, chats, rides, radio, a driving job | Have |
| Vehicle keys, locks, hotwiring | Car stealing, keys, alarm, hotwire | Have |
| Garages and owned vehicles | One owned car, resprayed, not saved | part |
| Mechanic, paint shop, chop shop | All three | Have |
| Fuel | None | none |
| Police job, wanted level, jail | The Task Force and the army come for you; arrest costs bail. No jail, no playing as police | part |
| Ambulance, death, hospital | Health comes back by itself; at nothing you are arrested or robbed. No hospital, no being revived by another player | part |
| Jobs with ranks and pay | Conductor, ride driver. No ranks, no duty, no payslip | part |
| Gangs and territory | Area boys' streets and districts, as data. Nothing to join or hold | part |
| Houses and apartments | None | none |
| Clothing shop and wardrobe | A wardrobe on the pause menu, free | part |
| Radial menu and "look at it to act" | An inventory wheel (Tab) and E / F prompts | part |
| Player list, chat, voice, admin commands | None | none |
| Weather and time for everybody | The clock and the sky are on `ANHGameState`, replicated | Have |
| Several characters a player | Twelve bodies to choose, one save | part |

### Build order

Each is made server-owned from the start, so it does not have to be redone for multiplayer.

1. **Inventory and items.** Done.
2. **Shops**: a kiosk, a chemist, a buka and a man who sells what he should not. Buying is one server call that takes money and adds the item, or does neither.
3. **Ground items and giving**: a dropped thing lies where it fell and can be picked up; handing something to the player in front of you.
4. **Hospital and being down**: at no health a player is down, not teleported; another player with a first aid box can get them up; otherwise the hospital, and a bill.
5. **Jobs as data**: one `jobs.json` with ranks and pay, a duty switch, and the conductor and driver jobs moved onto it. Then a job that is the Task Force.
6. **Garages and owned vehicles saved**, then fuel.
7. **Player list, text chat, and admin commands** behind an admin flag.
8. **Houses**, then **gangs and territory**.

## 2. Multiplayer: how it is to work

The game is being made for a server that owns the world and a handful of players, the way a FiveM or RAGE MP
server does, using Unreal's own replication rather than anything of theirs.

| Rule | In the frameworks | In NAIJA HUSTLE |
|---|---|---|
| The server decides | Clients trigger server events; the server checks and changes state | A client's machine may only call a `Server` function on something it owns. The function checks everything again. Nothing is taken on a client's word: not money, not items, not a hit. |
| One record a player | `PlayerData` | `ANHPlayerState` for money and wanted level, the inventory on the player's character. Replicated to the owner only where it is nobody else's business (what you carry). |
| World state is the server's | State bags, OneSync | The clock and sky on `ANHGameState`. Traffic, pedestrians, the Task Force and vehicles are still made on each machine: they are next (see `NETWORK_AUDIT.md`, items 3, 5 and 7). |
| Only what is near exists | Entity streaming, culling by distance | World Partition already loads the city by distance. Traffic and pedestrians are made in a bubble round "the player": that has to become "round every player", on the server. |
| A feature is its own piece with its own data | Resources | A component or actor each (`UNHInventoryComponent`, `ANHResponse`, `ANHCarTheft`), with a JSON file in `Data/`. |
| Messages go to one player | `TriggerClientEvent` to a source | Still 80-odd direct calls to the HUD. They become a client call on the player controller. |
| Menus do not stop the world | Every menu runs with the game going | The bag does not pause. The map and the pause menu still do, and the wheel still slows time: with a second player they must not. |
| Admin powers are a permission | ACE permissions | The console commands (`NHCash`, `NHGoto`, `NHGive`...) have no check. `NHGive` refuses on a client; the rest must be locked before anybody else joins. |

What this means in order: finish the audit's list (vehicles replicated, traffic and people server-only, messages to
one player, menus that do not pause), then run two machines for the first time. **Nothing has yet been run with two
players.**

## 3. Menus

A list menu in the frameworks' style is in the HUD (`ANHHUD::DrawListMenu`): a banner with the title, a bar with a
heading and "3 / 12", rows with the chosen one lit and a value at the right, and a help panel under the list. It
shows ten rows and scrolls.

- **The bag (B)** is drawn with it: what you carry, how many, the weight, what Enter does with it. Left / Right
  switches between using and dropping.
- **The pause menu, its pages and the phone are still drawn their own way.** Moving them onto the list menu is the
  next menu job.

## Built so far: the inventory

- `Data/items.json`: 17 things (three weapons, two kinds of rounds, food and drink, bandage and first aid box,
  phone, torch, master key, repair kit, jerry can), shaped like QBCore's shared item list with naira prices and a
  health value. The starting kit and the 30 kg limit are in the same file.
- `UNHInventoryComponent` on the player's character. Things are added and removed only on the server; the list is
  replicated to the owner; using and dropping are `Server` calls the server checks.
- A weapon can only be taken in the hand if it is in the bag. A pistol shot uses a pistol round and an AK-47 shot a
  rifle round; with none left the trigger clicks and nothing is fired.
- Food, drink, a bandage and the first aid box give back health when used, and are not used up at full health.
- Console: `NHBag`, `NHGive <item> <how many>`. Test: `Scripts/mac.sh play -NHBagTest`.

Not done: nothing is saved, so every start gives the starting kit again; a dropped thing is gone, not on the
ground; nothing sells or gives items; the torch, phone, master key, repair kit and jerry can are carried but the
game's own torch, phone, theft and mechanic do not ask for them yet; the wheel (Tab) still offers the three weapons
directly, and refuses the ones you do not carry.
