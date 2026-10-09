# Who comes when you have wanted stars

**Date:** 2026-10-09 · **Branch:** `lagos-real-city`

Everybody and everything here is made up: the Task Force (the game's police), where its stations are, and who runs
which streets. The lists are data, in `Data/response_areas.json`; change them there.

| Where you are | Who comes | When |
|---|---|---|
| Ordinary streets | The Task Force | 2 stars within 1.5 km of a station; 3 stars within 4 km; never beyond that |
| Area boys' streets | Area boys, with machetes. No Task Force. | 2 stars |
| High-class streets | The Task Force, from any distance | At any stars, but only after a witness calls: somebody who ran from it, within 80 m, and stayed alive 6 seconds. No witness, no Task Force. |

- **High-class:** Banana Island, Ikoyi, Victoria Island, Eko Atlantic, Lekki Phase I, Ikeja G.R.A, Osapa London, Dolphin Estate, Ikate.
- **Area boys':** Mushin, Ajegunle, Oshodi, Makoko, Bariga, Idi Oro, Amukoko, Orile Iganmu, Ijora, Iddo, Mile 12, Somolu, Ojuelegba, Alaba, Owode Onirin.
- **Stations** (at the middle of each district): Ikeja G.R.A, Yaba, Surulere, Ikoyi, Victoria Island, Lagos Island, Apapa, Isolo, Gbagada, Ojota, Lekki Phase I, Obalende, Ebute-Metta, Mile 2, Anthony.

## What they do

- They arrive on foot from about 50 m off, one every 2.5 seconds: as many Task Force as you have stars (six at
  five stars), one more area boy than stars. The further the station, the longer the first takes (4 to 16 s).
- The Task Force stop within 14 m, in sight of you, and shoot: about one shot in three hits, for 10 of your 100.
  Area boys run in and cut: three swings in five hit, for 16. In a car, the car takes it.
- They can be shot. A Task Force officer down adds a star and a half.
- Your stars only fade while none of them can see you. When the stars are gone they walk off.
- Your health comes back after six quiet seconds. At nothing: the Task Force arrest you and you pay bail; area boys
  beat you and take the money. Either way it is a tenth of what you carry (2,000 at least), the stars are cleared,
  and you are back where the day began. A health bar shows bottom left once you are hurt.

Console: `NHResponse` says what kind of streets you are on, the nearest station and who is coming.

## Checked

`Scripts/mac.sh play -NHResponseTest` and `Scripts/mac.sh city -NHResponseTest` give the player three stars and log
what happens:

- Small level (ordinary streets, station 0.1 km off): Task Force called at once, 3 there within 10 s, the player
  down to 20 health by 25 s, then arrested, 2,000 bail.
- Oshodi in the city (area boys' streets): area boys called, 4 there by 15 s, the player beaten by 20 s, 2,000 taken.
- Ikoyi reads as high-class, Yaba and Ketu as ordinary, Mushin as area boys'.

## Not checked, not built

- **The witness call in a high-class area has not been played**: only that the area is recognised.
- Not exercised: the 3-star rule far from a station, losing them by breaking line of sight, shooting them.
- They come on foot: no Task Force vehicles, sirens, checkpoints or helicopter; they do not take cover, and they
  walk in straight lines, so a wall stops them. They carry nothing you can see. There is no surrender or bribe.
