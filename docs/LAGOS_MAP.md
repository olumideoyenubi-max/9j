# The Lagos map

The game's map uses real Lagos places and conventional danfo stops, laid out the way the real city sits:
Oshodi in the north-west, Mushin and Yaba on the mainland, the lagoon with Makoko and Third Mainland Bridge in
the middle, Lagos Island and Lekki in the east. It is a compressed Lagos (384 m × 256 m so far), so distances
are not to scale and many stops in between are left out.

Place names are real. People, businesses, brands, radio stations, agencies and police units stay invented.

## Stops

Stop ids in the data files did not change; only the names shown to the player did.

| Id | Stop | Was | Notes |
|---|---|---|---|
| `oshoja` | Oshodi | Oshoja Park | The motor park; agbero ticket ₦400 |
| `iya` | Charity | Iya Basira Junction | First stop east of Oshodi on the expressway |
| `second` | Anthony | Second Gate | Where the Yaba and CMS routes split |
| `lagoon` | Gbagada | Lagoon Drive | |
| `bridge` | Iyana Oworo | Bridge Foot | Mainland end of Third Mainland Bridge; agbero ticket ₦500 |
| `eko` | CMS | Eko Crest Terminus | Terminus on Lagos Island |
| `marketrd` | Fadeyi | Market Road | On Ikorodu Road |
| `balogate` | Yaba | Balo Gate | Agbero ticket ₦600 |
| `marketsq` | Tejuosho Market | Market Square | |
| `ebute` | Olosha | Ebute Corner | Mushin |
| `church` | Idi-Oro | Mainland Church | Mushin |

## Routes

| Route | Stops in order |
|---|---|
| Oshodi – Yaba | Oshodi, Charity, Anthony, Fadeyi, Yaba, Tejuosho Market |
| Oshodi – CMS Express | Oshodi, Anthony, Gbagada, Iyana Oworo, (Third Mainland Bridge), CMS |
| Mushin Loop | Olosha, Idi-Oro, Fadeyi, Charity |
| First Day (mission) | Oshodi, Yaba, Charity, back to Oshodi |

## Districts and roads

| Now | Was |
|---|---|
| Oshodi, Oshodi Motor Park | Oshoja Junction, Oshoja Motor Park |
| Mushin (the dusty district) | Oke-Erupe |
| Yaba (Tejuosho Market) | Balo Market |
| Jibowu Luxury Bus Park | Luxury Bus Park |
| Lagos Lagoon, Makoko | The Lagoon, Stilt Town Waters |
| Third Mainland Bridge | Third Lagoon Bridge |
| Lagos Island | Eko Crest |
| Lekki | Lekka Strip |
| Oshodi–Oworonshoki Expressway | Mainland Expressway |
| Ikorodu Road | Market Road |
| Gbagada Road | Lagoon Drive |
| Marina | Eko Crest Avenue |
| Mainland | unchanged (the Anthony and Gbagada area) |

## Where the map bends the real city

- The Mushin Loop is not a real danfo route; it links real Mushin stops to Fadeyi and Charity to keep the short loop.
- Real stops between these are skipped (for example Onipanu and Jibowu between Anthony and Yaba, and Adeniji Adele and Obalende before CMS).
- Lekki sits directly beside Lagos Island; Victoria Island and Ikoyi are not on the map yet.
- The luxury bus park is placed in the south-west corner, not where Jibowu really is.

## Not on the map yet

Ikeja, Ojota, Mile 2, Apapa, Surulere, Ojuelegba, Obalende, Victoria Island, Ikoyi, Ajah and the Lekki toll
gate. The vehicle rules call for an Ikorodu Road-style expressway, a Lekki-Epe-style expressway with a toll gate,
and a spare-parts market in the Ladipo area of Mushin; those need the map to grow.

The archived Unity project (`legacy/unity/`) and `docs/GDD.md` still use the old invented names.
