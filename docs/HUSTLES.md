# NAIJA HUSTLE: Risk and Reward

> Ways to make or lose money between missions, and the people and phone messages that warn you. Every company,
> coin, bookmaker, app and person is fictional. The game shows consequences, never real-world methods.

## The idea

Lagos throws opportunities at you. Some pay, most are traps, and the people around you try to warn you.
Losses feel real but never end the game: you can always earn it back on the danfo.

## Rules for every risky hustle

- **Only earned naira can be risked.** Naira bought with Gold Kobo is tracked separately and can't be used for
  betting, stocks or crypto. That keeps it simulated gambling with no real-money route.
- **No Gold Kobo** is ever bet, invested or won through these.
- **Honest odds:** every outcome is decided by the published rules below, seeded and logged. No hidden rigging
  beyond what the game shows (the scams are the rigging, and they're signposted).
- **Daily limits:**
  - betting stake max ₦200,000 per in-game day
  - crypto and stocks max 50% of your cash
- **Content settings** can switch off night companions, and betting as a whole.

## 1. Betting: *Odogwu Bet* shops and app

Shops sit at Oshoja, Balo Market and Lekka Strip, with an app on the phone.

- **Football:** the fictional *Eko Premier League*: Balo Rangers, Oshoja United, Eko Crest Royals, Lekka Sharks,
  Stilt Town Mariners and Mainland Stars.
  - Results come from team strength plus randomness and are revealed at 22:00 game time.
  - Singles pay the shown odds. Accumulators of 3–6 picks pay huge odds that rarely land.
- **Virtual danfo races:** a 30-second race on the shop TV.
- **The house always wins:** expected return is 90% on singles and lower on accumulators.
- **Itch meter:** rises with every bet and falls slowly over days without betting.
  - High itch: the bet shop texts you "free bet" offers.
  - Higher itch: Iya Tobi and Amaka get worried and say so.

## 2. Stocks: the *StockAm* app (fictional Eko Stock Exchange)

| Company | Moves with |
|---|---|
| Gidi Fuel | Fuel prices and go-slow events |
| Crestline Bank | Story: falls about 30% after the Crestline Job, recovers slowly |
| Deltacrest Petroleum | Big swings |
| Thunder Motors (okadas and kekes) | Steady |
| Balo Foods | Steady, pays small dividends |
| Akinsola Holdings | The Kingmaker's company: crashes after Rally Day |

- Prices move once per in-game day on a random walk with slight upward drift, plus story events. Players who
  connect the story to the market can profit (and the game lets them).
- A 1.5% broker fee on each buy and sell. Prices are shown as a 30-day chart.

## 3. Crypto: the *CoinAm* app

| Coin | What it is |
|---|---|
| **GidiCoin** | Volatile but real: big daily swings, slight upward drift |
| **SuyaCoin** | A meme coin: spikes when *Yarns* talks about it, crashes after |
| **NairaMoon** | A pump-and-dump: it climbs for a few days, then the founders vanish (a rug pull) and it goes to zero. The timing is random, and the warning signs are visible (see below). |
| **DoubleUp Club** | Not a coin, a "guaranteed 40% every week" scheme. It pays the first few weeks, then freezes withdrawals and disappears. |

## 4. Love life

- **Amaka** is the main love story (STORY.md). A **relationship meter** rises with:
  - dates: the suya spot, the Lekka beach, the open-air cinema, owambe dancing
  - gifts
  - answering her calls
  - keeping promises

  It falls when you miss dates, ignore calls or get caught cheating. A high meter unlocks extra scenes and a
  bonus to her Unlock ability. A low one has her cold in missions, and she can leave in the bad ending.
- **The *LoveAm* dating app:** chat with matches.
  - Some are genuine (a date, a little Integrity, nothing more).
  - Some are **romance scammers**: they move fast, won't video call, and soon need money for "school fees" or "a
    customs fee". If you pay, the money's gone.

## 5. Night companions (can be switched off)

- On Lekka Strip and around Eko Crest at night, some adult NPCs offer company.
- **It's never shown:** a conversation, a payment, a fade to black, and it restores a little stress.
- **The risks are the story:**
  - a set-up where you're robbed of cash and phone
  - a Task Force raid (+1 star)
  - Amaka finds out from a *Yarns* post: big relationship loss
  - small Integrity loss

## 6. Thieves and scammers on the streets

| Trap | What happens | The tell |
|---|---|---|
| **Pickpocket** | In crowds at Balo Market, someone bumps you and some cash goes. Catch them within 10 s to get it back | A brief "bump" sound and someone walking off fast |
| **"One chance" danfo** | You flag down a bus with almost no passengers, they rob you and dump you at the roadside | An empty bus at night, with "passengers" who all sit near the door |
| **Fake checkpoint** | Fake officers stop you and demand a "settlement" | No proper uniform or vehicle, and they refuse a station visit |
| **"Your account is blocked" text** | A message on the phone asks you to "verify" by sending money or a code. If you do, part of your bank balance goes | Urgency, a strange sender, bad spelling. Crestline Bank says it never asks for codes |
| **"You won a prize"** | Pay a "processing fee" to receive a prize | You never entered anything |
| **Fake job offer** | A "processing fee" for a job abroad | The fee comes first |
| **Romance scam** | See LoveAm above | Never on video, always a crisis |

Some losses can be recovered in a short side mission: Amaka traces the scammer's phone and you get the money back.

## 7. The warnings: NPCs and the phone

Warnings come **before** the risk, and again after a loss. They're short, in character, and never preachy.

- **People:**
  - **Iya Tobi:** "That betting shop no be your friend. Your papa lost a whole bus fare there one night."
  - **Baba Sule:** "Any bus wey empty for night, no enter am. One chance no get second chance."
  - **Amaka:** "If dem no fit video call, na scammer. I don fix too many phones for people wey learn am the hard way."
  - **The bus-stop preacher:** shouts about "money wey dey double itself".
- **Radio:** *Hustle FM* public-service jingles ("Your bank no go ever ask for your PIN o!") and *Eko Talk*
  callers telling their own loss stories.
- **The phone:**
  - *Crestline Bank* sends real-sounding security tips.
  - **Yarns** shows victims' posts after a rug pull, a "DoubleUp" collapse or a one-chance robbery.
  - When you're about to fall for something, a contact texts a tell-tale sign. Amaka: "Who send you this? Check
    the sender."
- **After a loss:**
  - a short "What happened?" card that explains the trick in one line
  - a nudge from a contact
  - itch and stress going up
- **A phone stats page shows "Money lost to scams and bets"** next to "Money earned", so players see their own
  pattern.

## Data

All numbers (odds, house edge, drift, fees, limits, scam chances, warning lines) live in `naija_rules.json` under
`risk`, so the browser game and Unreal share them and they can be tuned without new builds.
