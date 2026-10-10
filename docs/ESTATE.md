# Land, property, night life, and the rich to play

**Date:** 2026-10-10 · **Branch:** `lagos-real-city` · Real-scale city only (`Scripts/mac.sh city`)

## What there is

| Thing | Where it lives | What it does |
|---|---|---|
| Places | `Data/estate.json` "places" (25) | Land, houses and flats for sale; bars, night clubs and strip clubs to go into and to buy; filling stations |
| People to play | `Data/estate.json` "people" (2) | Chief Dotun Bankole (Lekki Phase 1, N300bn, four cars) and Adaeze Nwosu (Eko Atlantic, N100bn, three cars) |
| The code | `ANHEstate` (`Gameplay/NHEstate.h`) | Stands the places, opens their menus, builds a club's room, puts a person in their life |
| Bank | `UNHHustleSubsystem::Bank` (64-bit), `Pay`, `Bankroll` | The pocket (`Cash`) stays 32-bit. Paying takes from the pocket if it covers it, else from the bank |
| Homes | `Scripts/build_estate_homes.py`, `import_estate_homes.py` | A modern two-storey house and a 22-storey tower made in Blender: `/Game/Estate/SM_Mansion`, `SM_Tower` |

Every person, estate and business is made up; street and area names are real. Nothing here is betting or gambling.

## Playing it

- **N** (or console `NHWho`) lists the people; Enter takes that life: their body, their bank balance, their home in
  their name, their cars on the verge outside. "The conductor" goes back to the game as it starts. `-NHPlayAs=chief`
  on the command line starts as one. The life is saved, and the game starts at their home the next time.
- A place is a board (for sale), a lit doorway (bar, club, strip club) or a canopy with pumps (petrol) on the verge of
  the nearest main road to where the data puts it. **E** there opens its menu, on the list menu.
  - For sale: buy it. Yours: rest till morning, make it home, sell it for four fifths.
  - Bar, club, strip club: go in (a gate fee, unless it is yours), buy off the card at the counter, go out. The
    business can be bought.
  - Petrol: fill the tank of the car you are in or have left by the pumps; buy a jerry can (20 litres, used from the bag
    standing by a car).
- Strip clubs are the red-light side: red lamps along the verge, a stage with poles, dancers. Nothing is explicit.
- Console: `NHPlace <id>` goes to a place's door; `NHPlaceUse <line>` chooses a line of its menu.

## Vehicles

- **Fuel:** every vehicle has a tank (60 litres; 90 for a long one, 12 for a bike). Ten litres a hundred kilometres,
  three for a bike. Empty, the engine gives nothing.
- **Dashboard** (bottom right while driving): speedometer, rev counter with the gear, fuel gauge and litres,
  odometer, and lights for the headlamps, the handbrake and a sick engine. The gear and revs are worked out from the
  speed (five gears, four on a bike); there is no gearbox to change yourself.
- **More in traffic:** a minivan, a work pickup and a yellow cab, on models the game already had.

## Homes: going in, saving, quick travel, the map

- A house or flat you own can be gone into (**E** at its board, "Go in"): one big room with a sitting area, a dining
  table, a kitchen and a bed. **E** inside: **Save the game**, **Sleep till morning** (which also saves), travel, or go out.
- A saved game starts at the home it was saved in. What is saved: money (pocket and bank), what you own, your home,
  who you are playing, the day and hour, missions done. Not saved: what is in the bag, cars, health.
- **Quick travel:** the phone's **Keys** app lists what you own; choosing one puts you at its door and moves the clock
  on by the drive (the distance and a third, at 50 km/h). The same choices are on the menu of any place you own. It
  is refused while you have wanted stars.
- **The map and minimap** mark your places with a gold house (home stays on the minimap's rim when it is far away) and
  the homes of the other people to play in white, with their names on the big map.
- `Scripts/mac.sh city -NHEstateTest -NHHomeTest` runs these.

## Garages, the mechanic, insurance, and what is inside a home

After the single-player apartment mod for GTA V the owner pointed at (none of its code is used):

- **Garage:** a home keeps your cars: six at a house or flat priced N500m or more, two at a smaller one. "Keep a car
  here" on the home's menu takes the car of yours standing by the board. Kept cars stand outside their home, are
  saved with the game, and a car bought on motorhaus.ng goes onto your home's books if there is room.
- **Mechanic:** the phone's Keys app lists your cars; Enter has the car brought to the kerb beside you for N25,000.
- **Insurance:** a wrecked kept car is claimed in the same list for a fiftieth of its value; it is back at home whole.
  Left / Right on a car sells it for half its value.
- **Inside:** a flat is a modern loft and a house a classic drawing room, both downloaded models (credits in
  `ASSETS.md`). Inside: save, sleep, **Wardrobe** (the clothes page) and **Watch television**.
- `Scripts/mac.sh city -NHRoomTest -NHNoSave` photographs both rooms.

## Trees

`ANHTrees` plants a palm or a shade tree on each of the 110,298 spots where the map had a green cone
(`Data/lagos_trees.json`), drawn out to 460 m (`-NHTreesFar=<cm>` changes it), and hides the cones. Measured at
1280x720 on the M1: 30 to 35 frames a second in Ikoyi, Victoria Island and Yaba, the same as without them.
`Scripts/mac.sh city -NHTreeTest -NHNoSave` repeats that.

## The phone

- The handset is drawn as a smartphone: rounded metal body, status bar with the game's time, signal and battery, the
  cut-out at the top and the bar at the bottom. The home screen is app icons on a wallpaper, four to a row, with a
  dock; Up / Down go a row, Left / Right an icon. Inside an app the rows are cards, the chosen one blue.
- **Waka**, a browser, with six made-up sites: eko-homes.ng (property for sale, pins it on the map), coastbank.ng
  (the bank balance; move money between the account and the pocket), motorhaus.ng (buy a vehicle, delivered to the
  kerb beside you), lagosafterdark.ng (bars and clubs with gate fees), pmswatch.ng (petrol price and stations),
  9jaheadlines.ng (the day's news from the game's own state).
- `Scripts/mac.sh city -NHPhoneTest`: pictures of the home screen and five pages in `Saved/NHPhone/`.

## Tests

- `Scripts/mac.sh city -NHEstateTest`: plays the chief, goes into a club and a strip club, buys land, visits the
  pumps, plays the madam; pictures in `Saved/NHEstate/`, lines starting `[estate]` in the log.
- `Scripts/mac.sh city -NHRideTest=okada` (or any type): gets on, rides, turns, brakes; `[ridetest]` lines say the
  wheels' turn, the lean, the rider's clip and the fuel; pictures in `Saved/NHRide/`.

## Not done, and where this differs from the Living Lagos brief

- **Rooms are built in code under the door**, when you go in, and taken down when you leave: one plan a kind of
  place, furniture shuffled by the place's name. The brief asked for streamed levels on data layers; this is not that.
- **No music** in the bars and clubs, and the people in them only stand or dance: they cannot be spoken to.
- **Every home has the same room inside**, whatever the building looks like outside, and it is built under the door like the clubs.
- **The Lekki house does not show.** The house is only stood where the roadside is clear, and the rows of shops the
  game puts along roads near the player fill the plot after it is chosen. The chief's home is its board among the
  shops; the Eko Atlantic tower, on open ground, stands.
- **A business you own earns nothing yet**; owning it only makes it free to use.
- **The city's own buildings are as they were.** Only the two new buildings have the extra detail.
- **No new car models.** The three added vehicles reuse the van, pickup and saloon models.
- **Single player only:** the bank, what is owned and the persona are the host's.
