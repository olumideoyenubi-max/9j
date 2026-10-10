# NAIJA HUSTLE — Game Design Document

> Working title. An original open-world action-adventure set across fictionalized versions of
> **Lagos**, **Port Harcourt** and **Abuja**. It uses no Rockstar/GTA names, logos or assets, and no real
> politicians, companies or gangs. Every character, brand, unit and organisation below is invented.

All numbers here (prices, rewards, heat values) are live data in
`legacy/unity/Assets/_Project/Resources/Content/*.json`. That data is the source of truth, and this document describes it.

---

## 1. Pitch

A young hustler from the Lagos mainland makes his way from danfo conductor to the oil-money
world of Port Harcourt and then into the power circles of Abuja. Rising, falling and choosing
who to become along the way. Loud, funny and full of satire, with plenty of heart.

**Pillars**
1. **Three cities, three personalities.** Each city has its own traffic, police, weather, radio, hustles and look.
2. **Hustle is gameplay.** Side jobs are real mechanics, not menus: conductor, crane, boat taxi, chauffeur.
3. **Satire with consequences.** Checkpoints, convoys, galas and social media are played for laughs, and your choices still leave a mark.
4. **Runs on the phone in your pocket.** 3 GB Android is the target, not an afterthought.

## 2. Protagonist & cast

| Character | City | Role |
|---|---|---|
| **Tunde Adeyemi** | Lagos → all | Lead (playable). Danfo conductor, mainland-born, quick-talking, broke, loyal. |
| **Baba Sule** | Lagos | Danfo driver and mentor. Thirty years on the Oshoja–Eko Crest route. |
| **Amaka Nwosu** | Lagos → all | Lead (playable). Balo Market phone-repair and hacking genius, Tunde's oldest friend and love interest. |
| **Kemi Lawson-Bright** | Lagos | Island party promoter. Owambe royalty. |
| **Shina Garage** | Lagos | Motor-park strongman and Lagos antagonist. |
| **Captain Ebi Tamuno** | Port Harcourt | Creek boatman, Baba Sule's cousin. |
| **Engr. Nkechi Amadi** | Port Harcourt | Oil-yard engineer and the only one reading safety reports. |
| **Mr. Pius Okorie** | Port Harcourt | "Logistics" manager at the fictional *Deltacrest Petroleum*. |
| **"Commodore" Dakoru** | Port Harcourt | Boss of the fictional *Silverline Boys* smuggling crew. |
| **Haruna "Protocol" Bello** | Abuja | Political fixer. Loyal to whoever paid last. |
| **Zainab Yusuf-Cole** | Abuja | Investigative journalist. |
| **Chief Barnabas "Big Bar" Okon** | Abuja | The fictional corrupt Big Man. |
| **Capt. Dayo Ige** | Abuja | Charter pilot who unlocks helicopters and jets. |

## 3. Story arc

> The current storyline is [STORY.md](STORY.md): 12 Lagos missions, two playable leads, unlockable and premium
> characters. The three-city arc below is the long-term plan for expansions.

- **Act 1, Lagos ("The Hustle").** Conductor → errand runner → Island party fixer. Tunde
  refuses (or doesn't) to collect Shina Garage's levies, wins back Baba Sule's danfo and flees
  on the night bus east. *Rise: street cred. Fall: run out of town.*
- **Act 2, Port Harcourt ("Garden City").** Boat taxis, crane work and tanker escorts lead
  to evidence of waste dumping and sabotage. The trail goes offshore to Platform Ekene-9, and the
  signatures lead to Abuja. Mr. Pius offers a fortune to make it disappear. *Rise: oil money.
  Fall: betrayed, hunted.*
- **Act 3, Abuja ("The Capital").** Chauffeur and estate-security gigs open doors. Tunde
  infiltrates a "transparency" gala, chases a 17-SUV convoy, survives Haruna's ambush and
  escapes a locked-down city by helicopter, then holds the line for Zainab's live broadcast. *Ending
  chosen by the player (flags + Integrity).*

## 4. The three cities

Each city is a separately streamed region (§9). District names are fictional. The "inspired by"
notes are for the art team only and never appear in game.

### 4.1 Lagos: "The Hustle" (starting city, ships in the base install)
| District | Inspired by | Vibe |
|---|---|---|
| Eko Crest | Victoria Island / Ikoyi | Glass towers, beach clubs, nightlife |
| Lekka Strip | Lekki | New estates, toll gate, long racing straights |
| Balo Market | Balogun | Wall-to-wall stalls, alleys, Task Force raids |
| Oshoja Junction | Oshodi | Motor-park chaos, danfo routes, area boys |
| Stilt Town Waters | Makoko | Stilt houses, canoes, lagoon chases |
| Third Lagoon Bridge | Third Mainland Bridge | Go-slow by day, racetrack by night |

**Vehicles:** danfo, keke, okada, Kamsi LE sedan, speedboats. **Ambient events:** go-slow
(3× traffic), owambe street parties, area-boy tolls, Task Force market raids.
**Police:** *Eko Task Force* (bikes → pickups → Rapid Response → Joint Patrol + helicopter);
*Lagoon Wardens / Marine Task Force* on water.

### 4.2 Port Harcourt: "Garden City" (mid-game, downloaded on unlock)
| District | Inspired by | Vibe |
|---|---|---|
| Garden Reserve | GRA | Leafy oil-money mansions |
| Mile One Market | Mile 1 | Covered market that floods in the rains |
| Mile Three Motor Park | Mile 3 | Buses to everywhere, mechanic village |
| Silverline Waterfront | Waterfront & creeks | Jetties, boat taxis, mangrove maze |
| Delta Basin Industrial Yard | Industrial zones | Pipelines, tank farms, gantry cranes, flares |
| Platform Ekene-9 | Offshore platforms | Steel island reached by vessel or helicopter |

**Vehicles:** tankers, pickups, cargo trucks, speedboats, boat taxis, supply vessel, gantry crane,
forklift. **Atmosphere:** harmattan haze, heavy rain and thunderstorms, and **black soot**: dark
particulate fog, falling "black snow" particles and a screen-grime overlay. Soot is most common in
the dry season and on flare nights. **Police:** *Garden City Patrol* on land, and on water the **Marine
Patrol** (skiff → gunboats → flotilla), so creek chases feel different from road chases.

### 4.3 Abuja: "The Capital" (late-game, downloaded on unlock)
| District | Inspired by | Vibe |
|---|---|---|
| Central Axis | CBD / government district | Fictional *Grand Unity Hall*, *Federal Unity Secretariat*, *Assembly Dome* |
| Maiden Hills Estates | Maitama | Embassies, gated mansions, guards everywhere |
| Asoro Gardens | Asokoro | Hillside villas of the powerful |
| Wuze Market | Wuse | The capital's most honest place |
| Iron Crown Outskirts | Hills & highways; Aso Rock / Zuma-style landmark | The fictional *Iron Crown Rock* monolith, empty highways |

**Vehicles:** convoy SUVs, sleek coupés, the Kite H-2 helicopter, the Skylord jet (late unlock).
**Ambient events:** convoy road blocks (everyone pulls over), peaceful protests, gala nights.
**Police:** *Capital Guard*, which escalates fast (1.4× heat) to armored SUVs and helicopters.
Busted costs 15% of cash.

## 5. Missions

Each city has **9 story missions** (target range 8–10, enforced by content validation) plus side
missions. Objectives are data-driven verbs: `GoTo, EnterVehicle, Deliver, Escort, Chase, LoseHeat,
Collect, Stealth, OperateCrane, Defend, Photograph, Talk, Wait, Choose`. Optional objectives pay a
bonus. Timed objectives fail the mission.

### Lagos
| # | Mission | Giver | Core verbs |
|---|---|---|---|
| 1 | First Day on the Danfo | Baba Sule | conductor tutorial: collect fares, beat the clock |
| 2 | Go-Slow Getaway | Baba Sule | okada chase through traffic |
| 3 | Balo Market Run | Amaka | timed delivery through alleys (bonus: no cracked screens) |
| 4 | Owambe Crasher | Kemi | talk past the gate, stealth, photograph fake aso-ebi |
| 5 | Area Toll | Shina Garage | **choice:** collect the levy or refuse; escape |
| 6 | Stilt Town Waters | Amaka | speedboat escort, lose the Lagoon Wardens |
| 7 | Third Lagoon Bridge | Kemi | midnight high-speed chase on the bridge |
| 8 | Shina's Garage | Baba Sule | stealth, recover papers, hold the gate |
| 9 | Night Bus East | Baba Sule | escort to the bus park → **unlocks Port Harcourt** |

### Port Harcourt
| # | Mission | Giver | Core verbs |
|---|---|---|---|
| 1 | Garden City Welcome | Capt. Ebi | boat-taxi tutorial |
| 2 | Rain on Mile One | Capt. Ebi | flooded-market pickup run |
| 3 | Crane School | Engr. Nkechi | **crane mechanic**: stack 5 containers |
| 4 | Convoy of Tankers | Mr. Pius | escort cargo trucks, defend against hijack |
| 5 | Soot and Secrets | Engr. Nkechi | stealth + photograph dumping in soot haze |
| 6 | Creek Chase | Capt. Ebi | speedboat chase, lose Marine Patrol |
| 7 | Yard on Fire | Engr. Nkechi | **sabotage-and-rescue**: save workers, crane clears debris, defend |
| 8 | Platform Ekene-9 | Capt. Ebi | **offshore infiltration** (stealth, photograph, escape) |
| 9 | Oil Money | Mr. Pius | **choice:** take the deal or keep the evidence → **unlocks Abuja + flights** |

### Abuja
| # | Mission | Giver | Core verbs |
|---|---|---|---|
| 1 | Welcome to the Capital | Haruna | VIP chauffeur run (bonus: no scratches) |
| 2 | Estate Rounds | Haruna | estate-security patrol, catch a thief |
| 3 | Wuze Market Whispers | Zainab | lose any tail, meet the source |
| 4 | Black Tie, Big Lies | Haruna | **gala infiltration**: dress code matters |
| 5 | Siren Season | Zainab | **convoy chase**: find the right SUV |
| 6 | Rock and a Hard Place | Haruna | ambush at Iron Crown Rock; **choice** |
| 7 | The Ledger | Zainab | stealth into Big Bar's villa |
| 8 | Rotor Wash | Capt. Dayo | **helicopter escape** |
| 9 | Big Man Down | Zainab | protect the live broadcast; **choose your ending** |

Infiltration missions (platform, gala, villa) are action-adventure set pieces: stealth cones,
photos and escapes. They contain no real-world procedures.

## 6. Side hustles

Repeatable shifts that pay per job, with a streak bonus and five skill levels (+15% pay per level).
Job "quality" (0–1) is measured from play. A botched job pays nothing and resets the streak.

| Hustle | City | Quality measured from |
|---|---|---|
| Danfo Conductor | Lagos | smooth driving + punctuality between stops; conductor shouts on HUD |
| Okada Dispatch | Lagos | delivery time, food intact |
| Creek Boat Taxi | Port Harcourt | ride comfort, timing |
| Yard Crane Shift | Port Harcourt | placement distance, alignment, load swing |
| Warehouse Forklift | Port Harcourt | pallets moved against the clock |
| VIP Chauffeur | Abuja | smoothness, punctuality, no damage |
| Estate Security | Abuja | patrol points, intruders caught |

## 7. Systems

### 7.1 Wanted ("heat")
- Five stars. Each crime adds heat (`world.json → crimes`) × the city multiplier (Lagos 1.0, PH 1.15, Abuja 1.4).
- Crimes only count if **witnessed** (pedestrians, drivers, responders, CCTV on the Witness layer),
  unless you're already wanted.
- Out of responders' sight, a search timer runs. Each full timer drops one star, and safe zones
  speed it up 3×. Being seen resets the timer. Stars flash on the HUD while the search runs.
- Responders spawn by star tier from the city's response table, with **separate water tables** (Marine Patrol in PH).
- **Busted:** lose a share of cash (8% / 10% / 15%), lose some street cred, and any active mission fails.

### 7.2 Checkpoints and "settle" (satire)
Four choices at any city or highway checkpoint:
- **Comply:** time passes and Integrity goes up. If you're carrying mission cargo, it's found (+2 stars).
- **Settle:** pay the city's rate (₦1k Lagos / ₦2k PH / ₦10k Abuja) and lose Integrity. Chance it's
  **filmed** and trends on *Yarns* (−clout, +1 star). In Abuja there's a real chance the officer is an
  **Integrity Unit sting** (+3 stars).
- **Talk:** outfit Respect + street cred + luck vs. the city's difficulty.
- **Floor it:** +2 stars and a spike strip.

Settling is never the free option. It is framed as satire, and it has consequences for heat, clout,
Integrity and the ending.

### 7.3 Economy (naira)
- Starting cash ₦5,000. Mission pay runs from ₦8k (Lagos #1) to ₦500k (Abuja finale).
- **Safehouses** per city (free starter room → stilt house / jetty house → penthouse / villa /
  mansion), with garage slots, boat docks and helipads.
- **Businesses** (3 per city) earn income per in-game day into a capped cash box you must visit
  to collect. Each is gated by a mission. Examples: *Mainland Shine Car Wash*, *Creek Express Boats*, *Capital Lounge*.
- **Car customization:** paint, rims, engine, brakes, armor, horn, tint, plus class-restricted items
  (danfo stickers, "convoy" siren on SUVs/sedans). Mods change top speed, handling and durability.
- **Outfits:** streetwear, ankara, kaftan, agbada, suits, workwear. Respect feeds checkpoint "Talk"
  and gala/protocol checks.

### 7.4 Soft stats
- **Street Cred** (per city, 0–1000): unlocks dialogue and lowers prices with contacts.
- **Integrity** (−100 … +100): moved by settling, mission choices and helping people. Drives the ending.
- **Clout:** followers on *Yarns*. Viral posts move it up or down.

### 7.5 The phone
| App | What it does |
|---|---|
| **Gist** | Chat app. Mission givers text you, with an "Accept" button that starts the mission. Unread badges. |
| **KoboPay** | Balance and transfers to contacts (₦50 fee). Occasional satirical "network wahala": pending… reversed… fee still charged. |
| **Yarns** | Satirical social feed reacting to what you do: viral checkpoint videos, chase gossip, corporate PR spin after the yard fire. All accounts fictional. |
| **MapAm** | Map, waypoints, interstate travel booking. |
| **Camera** | Used for Photograph objectives. |

### 7.6 Interstate travel
| Mode | Cost | Time | Notes |
|---|---|---|---|
| Luxury bus | ₦25–30k | 10–11 h | Cutscene with 3 random mini-events (preacher, medicine seller, tyre burst, suya stop, armrest war, Nollywood "Part 2"…) with small choices |
| Road | free | 9–10 h | Your own car; 4–5 highway checkpoints; the only option while wanted |
| Flight | ₦150–180k | ~1 h | Unlocks after PH finale (`flight_unlocked`) |

### 7.7 Radio
Each city has its own stations. Lagos: *Hustle FM 98.7* (Afrobeats), *Owambe Gold 101.5*
(juju/fuji/highlife), *Eko Talk 88.1* (call-in satire). PH: *Garden Groove 101.1*, *Creek Talk 95.3*.
Abuja: *Capital Calm 92.9*, *Protocol AM 720*. All music must be original or properly licensed.

### 7.8 Weather and time
24-hour clock (1 real second = 2 game minutes by default). Seasons: Nov–Feb dry/harmattan, Mar–Oct
rainy. Per-city weighted weather tables: Abuja dry season is mostly harmattan haze, and PH has soot
haze and storms.

## 8. Controls
- **Android:** floating left joystick, right-side swipe to look, context buttons (Enter/Talk
  appears only when relevant), plus Sprint/Jump/Attack, Brake/Horn in vehicles, Ascend/Descend for
  helicopter and crane, Phone, Camera and Radio.
- **PC:** WASD + mouse, F interact, Space jump/handbrake, Shift sprint, H horn, Tab phone, C camera,
  R radio, E/Q ascend/descend. **Gamepad** is fully mapped.

## 9. Content and originality rules
1. No real politicians, companies, gangs, security agencies or brands. Use fictional stand-ins (see the
   `NoRealWorldBrandsInContent` test, which fails the build on known names).
2. City names are real. Districts, buildings and landmarks are fictionalized.
3. Satire punches up at power and systems, not at ordinary people, ethnic groups or religions.
4. Crime mechanics are arcade abstractions. No real-world how-to (oil theft, sabotage, evasion).
5. Bribery ("settle") is never consequence-free and is framed as satire.
