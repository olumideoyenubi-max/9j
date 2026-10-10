# Witnesses (living-Lagos brief, step 3: 3.2 and 3.1)

**Date:** 2026-10-10 · **Machine:** Apple M1, 8 GB · **Branch:** `lagos-real-city`

A crime no longer raises wanted stars by itself. It raises them only if somebody **sees or hears it and then
reports it**. Everybody and every agency here is made up; the numbers and the lines people say are data, in
`Data/law.json`.

## How it works

- **Who notices.** Anybody on foot within 25 m with a clear view saw it. A gunshot is also heard within 60 m, view
  or not.
- **What they do.** Each witness decides once (and not again for eight seconds):

  | | Ignore ("Na your business") | Film it for Yarns | Shout | Report |
  |---|---|---|---|---|
  | A small thing (a gun fired at nobody, a dented car) | 70 | 10 | 10 | 10 |
  | A serious one (somebody cut, shot or killed) | 10 | 15 | 15 | 60 |

  The victim is half again as likely to report; on high-class streets everybody is (×1.6); at night fewer do (×0.7).
- **A report is a phone call that takes six seconds.** Get within 2.5 m of the caller before it ends and they put
  the phone away ("Abeg! I no see anything o."). Putting them down stops it too, and is a crime of its own.
- **A crime counts once**, however many people report it: 0.6 stars for a gun fired, 0.5 for a damaged vehicle,
  1.2 for a blade, 1.5 for a bullet, 2.5 for a death. Somebody who only heard it reports an unknown suspect, for
  half; a later report from somebody who saw it adds the other half.
- **A crime nobody saw or heard changes nothing.**
- **Whoever was sent for you counts at once**, with no phone call: the Task Force, the army or the area boys seeing
  a crime is the report.
- **On area boys' streets nobody calls the Task Force.** The same people call the boys ("Area! Area! Come see o!"),
  it comes to the same stars, and the boys are who come (`docs/RESPONSE.md`).
- **Filming:** five seconds later a post from "Lagos Eye" appears on Yarns on the phone, naming the district.
- **Nobody in a crowd fights.** People ignore, film, shout, report or run. The only civilians who fight are the
  area boys sent on their own streets.

## Checked

`Scripts/mac.sh city -NHDamageTest` (Oshodi, area boys' streets) and `Scripts/mac.sh play -NHDamageTest` (the small
level, ordinary streets, 100 m from a station): two seconds of AK-47 into a dozen passers-by, then two machete
swings, then eight seconds for the calls.

| | Oshodi | Small level |
|---|---|---|
| Crimes | 6 | 8 |
| Seen or heard by nobody | 0 | 0 |
| Witnesses: ignored, filmed, shouted, began a report | 1, 2, 2, 8 | 9, 4, 1, 11 |
| Stars straight after the shooting | 0 | 0 |
| Calls finished, stopped | 7, 1 | 9, 2 |
| Stars from the first finished call | 7.9 | 7.9 |
| Stars from every later call | 0 | 0 |
| Stars eight seconds on | 5 | 5 |
| Who was sent | the army, 5 in a pickup | the army, 5 in a pickup |

A run with only the machete (one person cut, at Oshodi): 6 calls, 1.2 stars from the first and nothing from the
rest, 2 stars, area boys coming.

## Not done, not checked

- **A crime with nobody near was never tested**: every test stands the player in a crowd.
- The stopped calls in the tests were callers shot or cut down; nobody was stopped by walking up to them.
- The high-class and night multipliers, the heard-only half and the Yarns post were not looked at in a test.
- Stealing a car, hitting somebody with a vehicle and punching are not crimes here yet: only the three weapons are.
- Witnesses in vehicles do not count; only people on foot.
- No picture or clip for this step: there is nothing new on screen but the lines over people's heads.
- Next in the brief: 3.3, 3.4, 3.5, 3.6.
