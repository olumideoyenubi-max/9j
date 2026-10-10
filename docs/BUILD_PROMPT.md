# NAIJA HUSTLE: master build prompt (for Claude Code on the Mac)

Paste everything below the line into Claude Code in the Unreal project folder. It runs phase by phase, keeps
`PROGRESS.md` up to date, and picks up where it stopped when you type **continue**.

---

You're building **NAIJA HUSTLE**, an original GTA-style open-world game set in a fictional Lagos, in my Unreal
Engine 5.8.3 project at `~/Documents/Unreal Projects/MyProject`. The game code is the `NaijaHustleGame` plugin in
`Plugins/NaijaHustleGame`. The design lives in the GitHub repo **olumideoyenubi-max/9j**, branch
`claude/happy-turing-4xqato`. My machine is an **8 GB M1 Mac**, so memory and fps matter in every step.

## Ground rules (apply to every phase)

1. **Story first.** `docs/STORY.md` is the source of truth for characters, missions, choices, money and endings.
   `docs/STORE.md` covers the Gold Kobo store and `docs/HUSTLES.md` covers betting, stocks, crypto, love life and
   scams. If this prompt and those docs disagree, the docs win. Tell me about the conflict.
2. **Everything original.** Use no Rockstar or GTA names, logos or assets, and no real politicians, companies,
   banks, gangs or brands. Every name comes from the docs or is newly invented.
3. **Missions are short and punchy:**
   - 5–8 minutes each (missions 8 and 12 may run to 12)
   - at most 5 objectives
   - no more than 45 seconds of travel without something happening
   - dialogue of 8 lines or fewer per scene, always skippable
   - a checkpoint every 2 minutes
   - one standout moment per mission
4. **Data, not hard-coding.** Numbers (pay, odds, prices, limits, timings) go in `Data/naija_rules.json`, and
   content in JSON next to it (`characters.json`, `missions/*.json`). If you change the shape of
   `naija_rules.json`, update `web/tools/export-unreal.js` in the repo to match.
5. **Violence and adult themes stay in the game's world.** Fictional groups, no real-world methods, and consequences
   for the player. Night companions are never shown (a fade to black) and sit behind a content setting.
6. **8 GB budget:**
   - textures capped at 2K
   - a small texture streaming pool (`r.Streaming.PoolSize=1000`)
   - Lumen off if it costs too much (fall back to the lighting rig)
   - target 30+ fps in the editor's standalone game at 1080p
   - report fps and memory after every phase
7. **Every phase ends the same way:**
   1. Build:
      `"<engine>/Engine/Build/BatchFiles/Mac/Build.sh" MyProjectEditor Mac Development -Project="<path>/MyProject.uproject" -WaitMutex`
   2. Run the plugin's script tests.
   3. Run the phase's autoplay test twice in standalone (`UnrealEditor ... -game -windowed -log -ExecCmds=...`).
      Both runs must pass.
   4. Measure fps with `startfpschart` / `stopfpschart`.
   5. Save 2–3 screenshots to `Saved/Screenshots/NH/<phase>/`.
   6. Update `PROGRESS.md` (in the project root): what's done, what's next, any open problems.
   7. Commit and push to the repo (see Phase 0).
   8. Give me a 5-line summary, then **carry on to the next phase** unless that phase says WAIT.
8. **If something fails three times,** stop, write it in `PROGRESS.md`, and ask me.
9. **Resume.** When I type `continue`, read `PROGRESS.md` and pick up from the first unfinished step.

## Phase 0: Sync with the repo

1. Clone or pull `olumideoyenubi-max/9j` (branch `claude/happy-turing-4xqato`) to `~/Documents/9j`.
2. Compare `unreal/NaijaHustle/Plugins/NaijaHustleGame` in the repo with `Plugins/NaijaHustleGame` in my project.
   My local copy has fixes the repo doesn't (UE 5.8 / Mac / 8 GB memory fixes, the lighting rig fallback, and maybe
   NH debug commands and autoplay). **Keep my local fixes** and merge in anything newer from the repo.
3. Read `docs/STORY.md`, `docs/STORE.md`, `docs/HUSTLES.md` and `docs/GDD.md` in full before writing any code.
4. Copy the merged plugin back into the repo and push it. Push code, scripts, JSON and docs only: **no file over
   5 MB, no `.uasset`/`.umap`, no Megascans**. If GitHub won't let you push, run `gh auth login`. If that's not
   possible, write a patch to `~/Desktop/naija-hustle.patch` and tell me.
5. Build, then play mission 1 once to make sure nothing broke. Start `PROGRESS.md` with every phase below as a
   checklist.

## Phase 1: The two leads (Tunde and Amaka)

- **Characters data** (`characters.json`), with each character's name, age, home, ability, how they're
  unlocked and their default outfit:
  - **Tunde Adeyemi, 24:** a danfo conductor from Oshoja Junction
  - **Amaka Nwosu, 23:** a phone-repair genius from Balo Market
  - everyone in the cast table of STORY.md
- **Switching:** a switch button (Tab, and a phone shortcut) jumps between Tunde and Amaka in free roam, with a short
  camera pull-up and drop like GTA. The other lead keeps living where you left them. Missions can lock switching or
  switch for you.
- **Abilities**, each on a meter that fills with play:
  - **Tunde, Hustle Rush:** a burst of speed and toughness on foot, and boost and grip in a vehicle
  - **Amaka, Unlock:** within 30 m, hack a phone, a CCTV camera or a traffic light, shown with an on-screen marker
- **Leads and unlocks:**
  - Tunde and Amaka are playable from the start.
  - Chidi unlocks after mission 5, Baba Sule after mission 8, Zainab after mission 12 if she lives, and Sir Jaguar
    after the story if he was spared.
  - Kemi, the Kingmaker and Big Bar are premium (Phase 7).
- Blockout bodies are fine for now (colour and silhouette per character). Real models come in Phase 9.
- **Autoplay test:** switch 10 times, use each ability once, no crash, no lost state.

## Phase 2: The mission runner and shared systems

Build once, use in all 12 missions:

- **Mission runner:**
  - loads `missions/mNN.json`: objectives, triggers, checkpoints, dialogue lines, the standout moment, rewards
  - shows a job card at the start and the night-shift reward card at the end
  - logs the mission's time so the length rule can be checked
- **Story flags and Integrity:** a flag store (for example `found_shina_bag`, `spared_jaguar`, `zainab_alive`,
  `kept_ledger`) and an Integrity stat from −100 to +100, both saved.
- **Night-shift pay:**
  - After every mission, Tunde does a short end-of-mission danfo drive (skippable with a summary).
  - Mission 1 pays normal fares and tips.
  - From mission 2: `pay(n) = round(economy.missionPayBase × (1 + economy.missionPayGrowth)^(n − 2))` with
    `missionPayBase = 1000000` and `missionPayGrowth = 0.135`. Mission 2 pays ₦1,000,000 and mission 12 pays
    ₦3,547,796, with ₦22,420,357 in total.
  - Story-choice money is extra.
  - This replaces the fixed naira rewards. Cred, Gold Kobo and Integrity rewards stay.
- **Combat:** punches and kicks, a few fictional guns and blades, cover, health, and getting knocked down (not
  dying: you wake up at the clinic and lose some cash).
- **Enemy AI:** patrol, investigate, chase, attack, flee.
- **Wanted stars:** the Task Force on foot and in pickups.
- **Stealth:** crouch, sight cones, noise, takedowns from behind.
- **Vehicles:** the existing danfo, keke, okada and cars, plus a **speedboat** for Stilt Town Waters.
- **Smaller pieces:**
  - **Disguises:** an outfit changes who recognises you (security vest, rich returnee, waiter).
  - **Heist planning board:** pick roles, route, and loud or quiet.
  - **Phone:** texts, calls, *Yarns* posts, a map and the apps from Phases 7–8.
  - **Checkpoints:** restart from the last one, with the mission timer kept.
- **Autoplay test:** a scripted test mission that uses every system once.

## Phase 3: Act 1, the hustle (missions 1–4)

Build these exactly as in STORY.md, each as `missions/mNN.json` plus any code it needs:

1. **First Day on the Danfo** (Tunde): the existing mission, reshaped to the length rule. It opens with Shina's
   boys threatening Iya Tobi at dawn.
2. **Phone Pass** (Amaka, then Tunde): a Task Force raid on Balo Market, then an okada chase, then a rooftop scene
   where their love story starts. On the night shift Tunde finds the **₦1,000,000 bag** under the back seat. Set
   `found_shina_bag`, and the ₦350,000 debt is paid automatically. Shina starts hunting.
3. **The Yahoo Office** (Amaka): get into the scam crew's office on Lekka Strip by stealth and steal the laptops
   before the raid. **Choice:** return the money to the victims (+Integrity) or keep a 30% cut (−Integrity, cash).
   The laptops show the crew pays Shina "protection".
4. **Hot Bus Robbery** (Tunde): hijack Shina's levy danfo on the expressway and outrun the Red Cowries' bikes.

## Phase 4: Act 2, the streets fight back (missions 5–7)

5. **Red Cowries** (both): get into the cult's night meeting under the old stadium, rescue Chidi, fight out.
   **Choice 1:** spare Sir Jaguar or leave him to the rival cult (`spared_jaguar`). Unlock Chidi.
6. **Iya Tobi** (Tunde): a car chase out of Oshoja, then by **speedboat** to the Stilt Town Waters hideout to rescue
   his mother. Baba Sule hides her outside Lagos.
7. **Owambe Con** (Amaka): at Kemi's owambe in Eko Crest, a rich-returnee disguise and a phone hack on the
   Kingmaker's treasurer. The phone shows Big Bar is funding the Kingmaker through Shina and **Crestline Bank**.

## Phase 5: Act 3, the big score (missions 8–9)

8. **Crestline Job** (both, switching): the planning board (roles for Amaka, Chidi and Baba Sule; a route; loud or
   quiet), case the bank, the heist, the vault, and a getaway through the go-slow. Get the **ledger**. Unlock Baba
   Sule. Crestline Bank stock falls about 30% (Phase 8).
9. **Rally Day** (Amaka): work the Kingmaker's rally as "security", swap the speech, and get Zainab's camera into
   the VIP tent. It goes viral on *Yarns*, and Akinsola Holdings stock crashes.

## Phase 6: Act 4, the price (missions 10–12) and the endings

10. **The Offer** (Tunde): an unfinished Eko Crest tower, a rifle, Zainab crossing the plaza below. **Choice 2:**
    take the shot (`zainab_alive = false`, cash bonus), fake it (stage the hit and get her out), or turn on Big
    Bar's men and run.
11. **Amaka** (Tunde): a hostage trade on the Third Lagoon Bridge that becomes a chase. **Choice 3:** hand over the
    ledger, or keep it and fight (`kept_ledger`). Tunde tells Amaka he loves her either way.
12. **Big Man Down** (both, switching): defend Zainab's live broadcast from her back office in Balo Market against
    Shina, the Red Cowries and Big Bar's convoy. Who helps depends on the flags: Chidi and the drivers, Baba Sule,
    and Sir Jaguar if spared.

**Endings**, chosen at the end of mission 12:
- **The Broadcast:** Zainab is alive, the ledger was kept and Integrity is +30 or more.
- **The Big Man's Boy:** the shot was taken, or Integrity is −30 or less.
- **Gone Quiet:** anything else.

Each ending gets a short epilogue as in STORY.md, then credits, then free roam with a **mission replay** menu (with
medals for time and objectives). Unlock Zainab and Sir Jaguar by their conditions.

**Autoplay test:** play all 12 missions back to back with scripted choices that reach each ending (three runs).
Check every mission's time against the length rule and list any that run long. Check the total night-shift pay is
₦22,420,357 plus the choice money.

## Phase 7: The Gold Kobo store (mock purchases only)

Follow `docs/STORE.md`:
- **Gold Kobo packs:** 100 for $0.99, 550 for $4.99, 1,200 for $9.99, 2,600 for $19.99 and 7,000 for $49.99.
- **Earned in game:** 5 per mission, 10 per gold medal, 20 for a 7-day streak, 50 for finishing the story.
- **What it buys:** naira packs, outfits, cars (including the **Gold Danfo** with double fares) and premium
  characters:
  - Kemi: 1,800
  - the Kingmaker: 2,500, after the story
  - Big Bar: 3,500, after the story
  Each premium character comes with their mansion safehouse, car and wardrobe.
- **How purchases work:**
  - Go through an `INHPurchaseProvider` interface, with a **mock** provider that always succeeds. No real store
    code, no real money.
  - **No loot boxes.**
  - A setting to switch purchases off.
- **Bought naira is tracked separately and can never be bet, invested or used in crypto.**

## Phase 8: Hustles and warnings (risk and reward)

Follow `docs/HUSTLES.md`, with all numbers under `risk` in `naija_rules.json`:
- **Odogwu Bet:**
  - shops at Oshoja, Balo Market and Lekka Strip, plus a phone app
  - Eko Premier League football with results at 22:00, accumulators, and virtual danfo races
  - 90% return
  - an itch meter
  - ₦200,000 a day limit
- **StockAm:** the six fictional companies, a daily random walk plus story events, a 1.5% fee and a 30-day chart.
- **CoinAm:** GidiCoin, SuyaCoin, the NairaMoon rug pull and the DoubleUp Club Ponzi. Stocks and crypto are capped
  at 50% of cash.
- **Love life:**
  - Amaka's relationship meter: dates, gifts, calls and promises
  - the *LoveAm* app with genuine matches and romance scammers
- **Night companions** on Lekka Strip and in Eko Crest:
  - talk, pay, fade to black
  - risks: a set-up robbery, a Task Force raid, Amaka finding out, an Integrity loss
  - off by default, with a content setting to turn them on
- **Street traps**, each with its visible tell: pickpocket, one-chance danfo, fake checkpoint, "account blocked" text,
  prize scam, fake job, romance scam.
- **Warnings come before the risk:**
  - Iya Tobi, Baba Sule, Amaka and the bus-stop preacher
  - Hustle FM jingles and Eko Talk callers
  - Crestline Bank security texts and *Yarns* victim posts
  - a contact texting the tell when you're about to fall for something
- **After a loss:** a one-line "What happened?" card, plus a phone stats page showing **money lost to scams and
  bets** next to money earned.
- **Autoplay test:**
  - Simulate 30 in-game days of betting and check the return is about 90%.
  - Check bought naira can't be bet.
  - Trigger each scam and check its warning shows first.

## Phase 9: The look (Lagos like the reference video)

**WAIT:** stop here and ask me to add surfaces from Fab (Megascans). Continue only when I type
**`surfaces added`**.

- Dusty Lagos streets: cracked asphalt, red laterite dust at the kerbs, open gutters, plastered and painted walls,
  rusty roofing sheets, faded hand-painted shop signs (fictional names only).
- Master materials with dust and wetness parameters driven by the weather.
- Warm haze in the dusty-noon preset, heat shimmer and a low sun.
- Every texture capped at 2K, with a small streaming pool. Measure memory before and after.
- **The danfo:**
  - a better yellow danfo with black stripes as a Chaos Vehicle
  - the blockout body stays if I haven't made a Blender model yet
  - tell me exactly what to export from Blender (scale, axes, wheel bones and their names) so I can follow the
    vehicle rig tutorial
- **Characters:** MetaHumans only if memory allows. Otherwise stylised low-poly people.

## Final report

When every phase is done (or I stop you), give me:
1. what's playable, mission by mission, with each mission's measured length
2. fps and memory at Oshoja Park, Balo Market and Eko Crest
3. the screenshots folder
4. open bugs and anything you skipped
5. what you pushed to the repo (commits) and what stayed local (assets)
