# Who comes when you have wanted stars

**Date:** 2026-10-09 · **Branch:** `lagos-real-city`

Everybody and everything here is made up: the Task Force (the game's police), where its stations are, and who runs
which streets. The lists are data, in `Data/response_areas.json`; change them there.

| Where you are | Who comes | When |
|---|---|---|
| Ordinary streets | The Task Force | 2 stars within 1.5 km of a station; 3 stars within 4 km; never beyond that |
| Area boys' streets | Area boys, with machetes. No Task Force. | 2 stars |
| Anywhere, at five stars | The army, whoever was coming before. Those already there stay and fight beside them. | 5 stars (`starsForArmy`) |
| High-class streets | The Task Force, from any distance | At any stars, but only after a witness calls: somebody who ran from it, within 80 m, and stayed alive 6 seconds. No witness, no Task Force. |

- **High-class:** Banana Island, Ikoyi, Victoria Island, Eko Atlantic, Lekki Phase I, Ikeja G.R.A, Osapa London, Dolphin Estate, Ikate.
- **Area boys':** Mushin, Ajegunle, Oshodi, Makoko, Bariga, Idi Oro, Amukoko, Orile Iganmu, Ijora, Iddo, Mile 12, Somolu, Ojuelegba, Alaba, Owode Onirin.
- **Stations** (at the middle of each district): Ikeja G.R.A, Yaba, Surulere, Ikoyi, Victoria Island, Lagos Island, Apapa, Isolo, Gbagada, Ojota, Lekki Phase I, Obalende, Ebute-Metta, Mile 2, Anthony.

## What they do

- The Task Force, in black, are driven in: a black mini van, then a black pickup, turn about, each from 80 to 105 m
  off with up to four men. It starts on open street-level ground, drives straight at you, and pulls up 17 m short or
  where something is in the way; the men get down one after another from both sides. One more officer than you have
  stars. The further the station, the longer the first takes (4 to 16 s).
- The army, in green, come the same way in a pickup and a six-wheeled troop truck, five men at a time, nine in all.
- Area boys still come on foot from about 50 m off, one every 2.5 seconds, one more than you have stars.
- The Task Force stop within 14 m, in sight of you, and shoot pistols: about one shot in three hits, for 10 of your
  100. Soldiers stop within 22 m and fire rifles two to three times as often: about one round in four hits, for 9.
  Area boys run in and cut: three swings in five hit, for 16. In a car, the car takes it.
- They can be shot. An officer or a soldier down adds a star and a half.
- What they came in stays where it stopped and can be driven off. Left alone, it is cleared away 45 seconds after
  they give up.
- Your stars only fade while none of them can see you. When the stars are gone they walk off.
- Your health comes back after six quiet seconds. At nothing: the Task Force arrest you and you pay bail; area boys
  beat you and take the money. Either way it is a tenth of what you carry (2,000 at least), the stars are cleared,
  and you are back where the day began. A health bar shows bottom left once you are hurt.

Console: `NHResponse` says what kind of streets you are on, the nearest station and who is coming.

## Checked

`Scripts/mac.sh play -NHResponseTest` and `Scripts/mac.sh city -NHResponseTest` give the player three stars (or
`-NHResponseStars=5`) and log what happens; a picture of the first vehicle goes to `Saved/NHResponse/`:

- Small level, three stars (2026-10-09, vehicles): a Task Force van sent from 57 m with 4 aboard, all 4 down from
  it within 10 s, the player arrested by 25 s.
- Small level, five stars: the army called at once, a pickup with 5 then a truck with 4, 9 soldiers there by 20 s,
  the player arrested before 25 s.

Before the vehicles (on foot):

- Small level (ordinary streets, station 0.1 km off): Task Force called at once, 3 there within 10 s, the player
  down to 20 health by 25 s, then arrested, 2,000 bail.
- Oshodi in the city (area boys' streets): area boys called, 4 there by 15 s, the player beaten by 20 s, 2,000 taken.
- Ikoyi reads as high-class, Yaba and Ketu as ordinary, Mushin as area boys'.

## Not checked, not built

- **The witness call in a high-class area has not been played**: only that the area is recognised.
- Not exercised: the 3-star rule far from a station, losing them by breaking line of sight, shooting them.
- The vehicles drive in a straight line, not along the roads, and stop at the first thing in their way; they have
  no driver you can see, no siren and no flashing lights, and they do not chase a car. Whether each model faces the
  way it drives has not been looked at in the game. The vehicle arrivals have not been played in the city level.
- No checkpoints or helicopter; the men do not take cover, and they walk in straight lines, so a wall stops them. They carry nothing you can see. There is no surrender or bribe.
