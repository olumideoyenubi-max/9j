# GLB models (optional)

Drop `.glb` files here and the browser demo will draw them instead of its procedural meshes.
The game loads them with Three.js's GLTFLoader, and Draco-compressed files work too.
Each model is optional; anything missing or broken keeps the procedural version.
The files have to be served over HTTP next to `index.html`; `file://` skips them.

## Expected files

| File | What | Fitted into (length × height × width, metres) |
|---|---|---|
| `danfo.glb` | yellow minibus | 5.52 × 2.1 × 2.36 |
| `keke.glb` | three-wheeler tricycle | 3.1 × 1.95 × 1.4 |
| `okada.glb` | motorbike taxi | 2.6 × 1.3 × 0.7 |
| `stall.glb` | market stall (table, posts, canopy) | 3.2 × 2.7 × 1.6 |
| `chair.glb` | plastic chair | 0.5 × 0.9 × 0.5 |
| `tank.glb` | rooftop water tank | 1 × 1 × 1 |
| `generator.glb` | small petrol generator | 0.8 × 0.7 × 0.6 |
| `rubble.glb` | rubble / broken-block pile | 1.6 × 0.7 × 1.6 |
| `heli.glb` | police helicopter (main rotor drawn by the game) | 10 × 3 × 2.4 |

## How models are fitted

- **Orientation:** the model's front should face +Z, which is the glTF convention. The game turns it to its
  own forward axis. Y is up.
- **Scale and position:** the model is scaled uniformly to fit inside the box above, centred, and stood on
  the ground. Exact real-world size doesn't matter.
- **Vehicle colour:** any material whose name contains `paint`, `body` or `car_col` is tinted with each
  vehicle's livery colour, so make those parts white or light grey. Other materials keep their own
  colours and textures.
- **Wheels and rotors:** the game animates its own wheels on vehicles and its own spinning rotor on the
  helicopter. Model vehicles without wheels, or with static wheels, and leave the main rotor off the
  helicopter.
- **Optimisation:** keep each model light, ideally under 15k triangles and 2–3 materials. The meshes are
  merged per material and drawn instanced, so a whole street of danfos costs a few draw calls.
- **Materials and textures:** use standard PBR (metallic-roughness) materials, with textures embedded in the GLB.

## manifest.json (optional)

Without a manifest, the game tries the names above. If `danfo.glb` is missing it assumes no models were
provided and stops looking. With a manifest it loads only the keys listed, and each entry can be a file
name or an object with options:

```json
{
  "danfo": "danfo.glb",
  "keke": { "file": "keke_v2.glb" },
  "okada": { "file": "okada.glb", "rotY": 0 },
  "heli": { "file": "heli.glb", "scale": 0.01 }
}
```

`rotY` (radians) overrides the default turn of π/2 from +Z to +X. `scale` overrides the automatic fit.

All models must be original or properly licensed.
Don't use real brand names, logos or assets from other games.
