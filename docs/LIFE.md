# NAIJA HUSTLE: Life in Lagos (interactions and animations)

> The RP layer: the everyday things you can do with people, places and vehicles between missions, and the
> animations that make them feel alive. Like the other docs, every name, brand and place is fictional.

## The idea

In GTA roleplay the fun is in the small stuff: talking your way out of a stop, buying from a hawker in traffic,
dancing at a party, working a shift. Lagos is full of that. Every person and many objects can be interacted with,
every interaction has an animation, and the city remembers what you did.

## 1. How you interact

- **Look and press:** look at a person, vehicle or object and a small prompt shows its main action (**E**).
- **Interaction wheel:** hold **E** for every option on that target, for example on a hawker:
  - buy
  - haggle
  - chat
  - ask for gist
  - rob
- **Emote wheel:** hold **G** for your own gestures and dances (section 3), usable anywhere, including from a
  vehicle seat (upper-body versions).
- **All options come from data** (`interactions.json`), so new ones need no code. Each option lists its target,
  conditions (time, money, Integrity, relationship), animation, dialogue lines and result.

## 2. People

| Action | What it does |
|---|---|
| **Greet** | The right greeting for who they are: a handshake with a finger snap for friends, a bow for elders. To your own elders (Iya Tobi, Baba Sule) Tunde prostrates and Amaka kneels, as Yoruba and Igbo manners expect. Greeting elders properly raises respect. |
| **Chat** | Two or three lines that change with the time, the weather, your Cred and the story so far |
| **Ask for gist** | Rumours that point to side jobs, hidden stashes, a scam to avoid or a story hint |
| **Ask directions** | Marks the place on your map |
| **Flirt** | Only with adults, only where it fits. Can annoy Amaka if she hears about it (HUSTLES.md) |
| **Beg, bribe, give money** | Get past an agbero, help someone in need (+Integrity), "settle" a checkpoint |
| **Insult, shove, fight** | Starts a fight. Crowds gather, film it on their phones, and it can end up on *Yarns* |
| **Rob** | Hands up, cash grab, a wanted star. People in that area remember you. |
| **Hire** | Hire a bus boy, a lookout, a driver for a job |

- **People remember you.** Every named NPC and each area keep a **respect** score. High respect gets discounts,
  free gist and help in fights. Low respect gets you higher prices, people refusing to talk, and calls to the Task
  Force.
- **People react:**
  - they turn their heads when you pass, step out of the way, complain if you bump them
  - they film fights and crashes on their phones
  - they run from gunfire and crowd round an accident
  - they shout greetings at Tunde once he's famous

## 3. Emotes and dances

| Group | Emotes |
|---|---|
| **Gestures** | wave, hands up, point, thumbs up, clap, facepalm, "shine your eye" (point to the eye), "you dey mad?" (palms up), salute, shrug, beckon, "comot" (wave away) |
| **Poses** | sit (anywhere flat, or on benches and kerbs), lean on a wall or car, squat, arms crossed, hands on hips, phone call, selfie, smoke-free "chilling" idle |
| **Dances** | Shaku Shaku, Zanku (legwork), Etighi, Galala, Skelewu, Gbese, owambe two-step, and spraying money |

- Dances can be done with others: NPCs nearby join in at parties and owambes.
- Emotes are unlocked as you go (some are store items, some come from missions or from people who teach you).

## 4. Daily life

Three light needs, never punishing: they nudge you into the city's life.

| Need | Goes down when | Fix it |
|---|---|---|
| **Hunger** | Time passes, you sprint and fight | Eat at a bukka (amala, jollof, pounded yam), a suya spot, a puff-puff or corn hawker, or cook at home |
| **Energy** | You stay awake too long | Sleep at a safehouse, or nap on the danfo's back seat |
| **Stress** | Fights, chases, losses, go-slow | Eat, sleep, dance, watch football at the viewing centre, spend time with Amaka |

- High needs give small bonuses (faster Hustle Rush, better haggling). Low ones give small penalties (shaky aim,
  slower meters). Nobody starves.
- **Power cuts:** the light goes in an area at random. Streets go dark, generators start up and you can buy fuel
  for one at home.

## 5. Jobs (RP shifts)

Short, repeatable loops (5–10 minutes) that pay in naira and respect. The danfo shift already exists.

| Job | Who | The loop |
|---|---|---|
| **Danfo conductor or driver** | Tunde | Exists: fares, change, the agbero |
| **Okada rider** | Tunde | Carry one passenger fast through traffic. Tips for speed, losses for scares. Avoid the Task Force on banned roads. |
| **Keke driver** | Either | Three passengers, short hops around Balo Market |
| **Phone repair** | Amaka | A mini-game at her stall: open, find the fault, fix, close. Some phones hold scam evidence. |
| **Roadside mechanic** | Either | Diagnose by sound, swap the part, test drive |
| **POS agent** | Amaka | Cash out for customers. Spot fake transfer alerts (a link to HUSTLES.md scams). |
| **Market trader** | Either | Set prices, haggle with buyers, restock before the evening rush |
| **Delivery rider** (*ChopRun*) | Either | Food delivery on a bike: hot, fast, to the right gate |
| **Tow truck** | Tunde | Clear broken-down cars from the expressway, negotiate the fee |

## 6. The city

| Where | What you can do |
|---|---|
| **In go-slow** | Hawkers walk between the cars. Wind down the window and buy pure water, plantain chips, a phone charger or a newspaper. Window boys wipe your screen and want ₦100. |
| **Filling station** (*Gidi Fuel*) | Fuel up, queue in a scarcity, buy black-market fuel at a premium |
| **Vulcanizer and car wash** | Fix a flat, wash the car (a clean car gets better fares) |
| **Barber and tailor** | Haircuts and made-to-measure outfits, with a mirror view |
| **Owambe** | Dance, spray money on the celebrants, eat, meet people who give jobs |
| **Viewing centre** | Watch the Eko Premier League with a crowd that cheers and argues |
| **Street football** | Play a short 3-a-side game with kids on a closed street |
| **The park** | Play draughts or ludo with Baba Sule and the drivers, for small stakes |
| **Bukka and beer parlour** (*Ogogoro Ojo's*) | Eat, sit, hear gist, start or stop a fight |
| **Home** | Sleep, change clothes, stash cash, cook, and watch *Eko News* report on what you've done |

## 7. Stops and arrests

The Task Force and the fake checkpoints from HUSTLES.md play out as scenes:

1. You're waved down. Pull over, or run (wanted stars).
2. **Hands up** or keep them on the wheel, then show your papers.
3. Answer questions on the wheel: be polite, argue, "settle", or refuse and ask to go to the station.
4. A search, a fine, a bribe, a warning or an arrest. Arrest plays a handcuff scene, a ride in the back of the
   pickup and a release from the station with a fine.

How you behave changes the outcome, and real officers are different from fake ones (the tells are in HUSTLES.md).

## 8. Vehicles

- **Getting in and out** with an animation per vehicle:
  - slide the danfo's side door and jump in
  - swing a leg over the okada and kick-start it
  - climb into the keke
  - open a car door
  - steal a car by breaking the window or hot-wiring it
- **The conductor:**
  - hangs out of the danfo's open door shouting the route ("Oshoja! Balo! Enter with your change!")
  - bangs the side to tell the driver to go
  - counts and folds money
- **Passengers** board, squeeze in, pass fares forward and climb out.
- **Okada passengers** sit behind you, and you can carry two at a squeeze.
- **Driving gestures:** honk patterns, waving a hand out of the window, insulting other drivers in go-slow.
- **Breakdowns:** push the car, get out and open the bonnet, call a mechanic.

## 9. Animation plan (Unreal)

| What | How |
|---|---|
| **Locomotion** | Epic's free **Game Animation Sample** (on Fab) gives walk, run, sprint, turns, stops, jumps and traversal with motion matching. On the 8 GB M1, measure its cost. If it's too heavy, use the same animations in a classic state machine with blend spaces. |
| **Skeleton** | Everything on the UE5 mannequin skeleton, retargeted with the **IK Retargeter** to each character |
| **Extra moves** | Free **Mixamo** animations (download FBX at 30 fps, without skin) for gestures, sitting, fighting and dances, then retarget |
| **Nigerian moves** (prostrating, kneeling, Shaku Shaku, Zanku, Etighi, the conductor door hang, spraying money) | Recorded on a phone and turned into motion with a video mocap tool, or hand-keyed with **Control Rig** in Sequencer |
| **Layering** | Montages with layered blend per bone, so you can talk on the phone, eat or count money while walking |
| **IK** | Foot IK on kerbs and stairs, hand IK on door handles and steering wheels, look-at so heads turn towards the player |
| **Faces** | Simple talking mouth movements now. MetaHuman faces later, only if memory allows. |
| **Crowds** | The Animation Budget Allocator, update rate optimisation, animation sharing for crowds, and low-detail animation at a distance, so hundreds of people stay cheap |
| **Placeholders** | Every interaction names its animation in data. Until the real animation exists, a stand-in (a mannequin pose or nod) plays, so systems can be built and tested first. |

**Budget for the 8 GB Mac:**
- animation memory under 150 MB
- at most 40 fully animated characters near the player; the rest use crowd animation
- 30+ fps at Balo Market at its busiest

## 10. Multiplayer

NAIJA HUSTLE is heading for RP servers. Every interaction here is built server-ready from the start, as
[MULTIPLAYER.md](MULTIPLAYER.md) describes, so it works the same with one player or many.

## Data

| File | What it holds |
|---|---|
| `interactions.json` | Every option on the wheel: target, conditions, animation, dialogue lines and result |
| `emotes.json` | Emotes and dances: animation, loop or one-shot, how each is unlocked |
| `jobs.json` | The RP jobs: pay, timing and rules |
| `naija_rules.json` → `life` | The numbers: needs rates, respect gains and losses, prices, power-cut chance |
