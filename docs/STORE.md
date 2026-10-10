# NAIJA HUSTLE: Store and Gold Kobo

> Design for the in-game store. All brands, cars, clothes and characters are fictional. Real-money purchases go
> through the platform's own store: App Store, Google Play, Steam or the Epic Games Store.

## Gold Kobo

**Gold Kobo** is the store currency. You buy it with real money, and you can also earn small amounts by playing,
so every item is reachable without paying (just slower). Naira, the in-game money, stays the currency of the
streets: fares, shops, the tailor, fuel and bribes.

| Pack | Gold Kobo | Price (USD tier) |
|---|---|---|
| Pocket Change | 100 | $0.99 |
| Small Chops | 550 (incl. 50 bonus) | $4.99 |
| Hustler | 1,200 (incl. 200 bonus) | $9.99 |
| Big Boy | 2,600 (incl. 600 bonus) | $19.99 |
| Odogwu | 7,000 (incl. 2,000 bonus) | $49.99 |

Local prices (naira and others) come from each platform's price tiers.

**Earning it in the game:**
- 5 per story mission
- 10 for a gold medal on a mission
- 20 for a 7-day login streak
- 50 for finishing the story

## What you can buy

### Money (naira)
| Item | Gold Kobo |
|---|---|
| ₦50,000 | 100 |
| ₦250,000 | 450 |
| ₦1,000,000 | 1,500 |

### Clothes
Store outfits are exclusives. The tailor still sells the everyday outfits for naira.

| Outfit | Gold Kobo | Respect |
|---|---|---|
| Island Drip (designer-ish two-piece) | 150 | +12 |
| Gold-trim Agbada with fila | 300 | +25 |
| Aso-oke Owambe set (two colourways) | 350 | +28 |
| Lekka Night Out (shades, chains, white kicks) | 250 | +15 |
| Amaka: Balo Queen (ankara jumpsuit and gele) | 300 | +25 |
| Amaka: Tech Boss (bomber, cargo, headset) | 200 | +14 |

Respect helps when you talk past gates, beg the agbero or face down a checkpoint.

### Cars
| Car | Type | Gold Kobo |
|---|---|---|
| Kamsi LE Sport | Fast saloon | 400 |
| Pathmaster V8 Royale | Big Man SUV, armoured | 1,200 |
| Eko Crest GT | Two-door supercar | 2,000 |
| Gold Danfo "Odogwu Express" | A danfo with chrome and sound system that earns double fares on shifts | 800 |
| Thunder 250 Okada | Fastest bike in Lagos | 350 |

Bought cars appear in your garage, and you can call them to you from the phone.

### Premium characters (the richest people in Lagos)
Each one comes with their own mansion safehouse, signature car and wardrobe, and is playable in free roam and on
side hustles. The 12-mission story is always played as Tunde and Amaka.

| Character | Gold Kobo | Comes with |
|---|---|---|
| **Kemi Lawson-Bright**, party queen | 1,800 | Eko Crest penthouse, white Eko Crest GT, 4 owambe outfits. Ability: *Guest List*, walks past any gate. |
| **Hon. Dapo "Kingmaker" Akinsola**, politician | 2,500 (after the story) | Lekka Strip mansion, convoy SUV with siren, agbada wardrobe. Ability: *Convoy*, traffic pulls over for you. |
| **Chief Barnabas "Big Bar" Okon**, the Big Man | 3,500 (after the story) | Eko Crest tower and helipad, armoured Pathmaster, private helicopter. Ability: *Settle*, wanted level clears for naira. |

Characters you unlock through the story (Baba Sule, Chidi, Zainab, Sir Jaguar) are free.

## Rules

- **Bought naira can't be bet.** Naira from Gold Kobo packs is tracked separately and can't go into betting,
  stocks or crypto ([HUSTLES.md](HUSTLES.md)).
- **The story never needs a purchase.** Every mission can be finished with what the game gives you.
- **No loot boxes:** you always see exactly what you're buying.
- **Every real-money purchase is confirmed,** with the price shown in local currency. Purchases are restored on
  a new device.
- **Parental controls:** purchases can be switched off, and the platform's own limits apply.
- **Receipts are checked** with the platform before items are granted.

## How it's built

| Piece | Unreal | Browser demo |
|---|---|---|
| Gold Kobo balance and owned items | Saved with the game (`NHHustleSubsystem`), mirrored from the platform's receipts | Saved in local storage |
| Buying Gold Kobo | Unreal's in-app purchase support for Apple, Google, Steam and Epic, with receipts verified by our server | A mock store (no real money) to test the flow |
| Store screen | A **Store** app on the in-game phone, plus a button on the pause menu | Same, in the phone |
| Item list and prices | `store.json` next to `naija_rules.json`, so prices can change without a new build | Same file |
| Premium characters | A character switcher with Tunde, Amaka, unlocked and owned characters | Same |

A store that takes real money also needs a server for receipt checks, the platform developer accounts
(Apple, Google, Steam or Epic) and the age-rating questions about in-game purchases. That comes before launch,
not before the gameplay.
