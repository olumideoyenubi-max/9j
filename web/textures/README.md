# Photo textures (optional)

Drop texture files here and the browser demo will use them in place of its procedural textures.
Every file is optional; any missing or broken file keeps the procedural version.
The files have to be served over HTTP next to `index.html`, for example as part of the artifact or by
`npx serve web`. Opening the page from `file://` skips them.

## Expected files

There are eight sets. Each set has a required albedo (colour) map and optional normal and roughness maps.

| Material | Albedo (needed) | Normal | Roughness | Used for |
|---|---|---|---|---|
| Weathered concrete | `concrete_wall_albedo.jpg` | `concrete_wall_normal.jpg` | `concrete_wall_rough.jpg` | building walls (detail layer over the painted facades) |
| Peeling plaster | `peeling_plaster_albedo.jpg` | `peeling_plaster_normal.jpg` | `peeling_plaster_rough.jpg` | building walls (the other half of the buildings) |
| Laterite dirt road | `laterite_road_albedo.jpg` | `laterite_road_normal.jpg` | `laterite_road_rough.jpg` | Mushin's unpaved streets and verges |
| Cracked asphalt | `cracked_asphalt_albedo.jpg` | `cracked_asphalt_normal.jpg` | `cracked_asphalt_rough.jpg` | paved roads |
| Rusty corrugated zinc | `rusty_zinc_albedo.jpg` | `rusty_zinc_normal.jpg` | `rusty_zinc_rough.jpg` | zinc roofs and sheds |
| Blue tarp | `blue_tarp_albedo.jpg` | `blue_tarp_normal.jpg` | `blue_tarp_rough.jpg` | tarp shades over stalls |
| Wooden planks | `wood_planks_albedo.jpg` | `wood_planks_normal.jpg` | `wood_planks_rough.jpg` | loose planks on the pavements |
| Rubble | `rubble_albedo.jpg` | `rubble_normal.jpg` | `rubble_rough.jpg` | rubble and broken-block piles |

## Format

- Use square, seamless (tileable) JPGs. 2048×2048 is ideal. The game scales them down to the graphics
  setting: 512 px on Low, 1024 px on Medium and 2048 px on High.
- Albedo maps should be sRGB with no baked lighting or shadows.
- Normal maps should be OpenGL style (+Y up, the green channel pointing up).
- Roughness maps should be greyscale, where white is rough and black is glossy.
- Walls and roads multiply the photo over the painted colour, normalised by the photo's average, so
  a fairly neutral, mid-grey-ish photo works best for `concrete_wall` and `peeling_plaster`.

## manifest.json (optional)

Without a manifest, the game tries the names above. If `concrete_wall_albedo.jpg` is missing it assumes
no textures were provided and stops looking. With a manifest it loads only the sets listed, and only the
maps each set lists:

```json
{
  "concrete_wall": ["albedo", "normal", "rough"],
  "peeling_plaster": ["albedo", "normal"],
  "laterite_road": ["albedo", "normal", "rough"],
  "rusty_zinc": ["albedo"]
}
```

All textures must be original or properly licensed, for example CC0 scans of your own photos.
Don't use logos, brand names or assets from other games.
