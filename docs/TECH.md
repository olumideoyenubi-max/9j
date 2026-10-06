# NAIJA HUSTLE — Technical Design

## 1. Targets
| | Minimum (Android) | Recommended | PC |
|---|---|---|---|
| RAM | **3 GB** | 6 GB | 8 GB |
| GPU | Mali-G52 / Adreno 610 | Mali-G76 / Adreno 640 | GTX 1050 |
| Frame rate | 30 fps | 30–60 fps | uncapped |
| Base install | **< 200 MB** (AAB) | | |

Engine: **Unity 6 LTS (6000.0)**, URP, IL2CPP, ARM64, Vulkan with GLES3 fallback.

## 2. Code architecture

```
Assets/_Project/Scripts/
  Core/      NaijaHustle.Core     — pure C#, noEngineReferences. All rules & state.
  Runtime/   NaijaHustle.Runtime  — MonoBehaviours: input, physics, streaming, UI.
  Editor/    NaijaHustle.Editor   — content validation, Addressables setup, debug menu.
Assets/_Project/Resources/Content/*.json — all game data (cities, missions, vehicles…)
Assets/_Project/Tests/EditMode/        — NUnit tests (run in Unity *and* via dotnet)
Tools/CoreTests/                       — dotnet harness compiling Core + tests outside Unity
```

- **`GameSession`** (Core) is the composition root. It owns the wallet, clock, wanted system,
  mission board, businesses, garage, wardrobe, phone apps, travel and save/load. Runtime code forwards
  ticks and gameplay events into it (`Missions.Report(new ObjectiveEvent(...))`,
  `CrimeReporter.Report(...)`) and listens to its C# events.
- Because Core has no `UnityEngine` dependency, the whole rule set (including a full 27-mission
  story playthrough) is tested in about a second with `dotnet test Tools/CoreTests`, and CI runs it on every push.
- **Content is data.** Designers edit JSON, and `ContentDatabase.Validate()` checks for duplicate ids,
  broken prerequisites, story-graph cycles, missing city data, 8–10 story missions per city,
  unknown vehicle targets and banned real-world brand names. It runs in tests, on boot in dev builds
  and from **Naija Hustle → Validate Content**.

## 3. Streaming: one city in memory

```
Base APK/AAB (<200 MB)                     Remote (CDN / Play Asset Delivery)
├── Boot scene (bootstrap, UI, phone)      ├── City_PortHarcourt  (~420 MB, label city-portharcourt)
├── Shared_Vehicles                        └── City_Abuja         (~400 MB, label city-abuja)
└── City_Lagos (local group)
```

- `CityStreamer` **unloads the current city scene and releases its bundles before** loading the
  next one, so peak memory is one city plus the boot scene.
- On unlock, `Addressables.GetDownloadSizeAsync(label)` → prompt (warn on mobile data) →
  `DownloadDependenciesAsync`. Downloads can run during the bus-ride or flight interlude.
- Inside a city, use **sub-scenes per district** (additive, loaded by distance) once the map grows
  past ~1.5 km². HLOD meshes stand in for unloaded districts.
- Audio: radio tracks are Vorbis, *Streaming* load type, one resident clip at a time.
- Run **Naija Hustle → Setup Addressables Groups** to create the groups (Lagos local, PH/Abuja remote, LZ4).
- For Google Play, add `com.unity.addressables.android` (Play Asset Delivery) so remote groups
  become *on-demand* asset packs instead of needing a separate CDN.

### Install budget (target)
| Item | Budget |
|---|---|
| Engine + IL2CPP code (ARM64) | ~45 MB |
| Boot scene, UI, fonts, phone | ~15 MB |
| Player, NPC base rigs, shared vehicles | ~35 MB |
| Lagos city (meshes ASTC 6×6, 1K max, baked lightmaps off → light probes) | ~90 MB |
| Audio (SFX + 1 Lagos station sample) | ~10 MB |
| **Total** | **~195 MB** |

## 4. Memory budget, low tier (3 GB device, ~1.1 GB usable by the app)
| Category | Budget |
|---|---|
| Textures (ASTC, mipmap streaming on) | 300 MB |
| Meshes | 120 MB |
| Audio | 25 MB |
| Animation | 40 MB |
| Managed heap + Addressables | 120 MB |
| Render targets (0.7 render scale) | 60 MB |
| Headroom | rest |

## 5. Performance rules
- `DeviceProfile` picks **Low / Mid / High** from RAM/VRAM. It sets render scale (0.7 / 0.85 / 1.0),
  shadow distance (25 / 45 / 80 m), LOD bias, far clip (300 / 450 / 800 m), traffic and pedestrian
  density. Create three Quality levels named Low, Mid, High in that order.
- Weather fog doubles as distance culling: harmattan and soot hide a short far plane.
- Vehicles use **raycast suspension** (no WheelColliders). Traffic AI is a cheap lane follower with
  one forward ray, and it's pooled.
- GPU instancing + SRP Batcher on, a single main light, and no real-time GI.
- Draw-call target: < 150 on Low. Use texture atlases for market stalls and signage.
- Avoid per-frame allocations: non-alloc physics queries, pooled UI rows, no LINQ in `Update`.

## 6. Layers & tags
| Layer | Use |
|---|---|
| 0 Default | world geometry (blocks sight lines) |
| 8 Vehicle | drivable vehicles (`VehicleInteractor.vehicleMask`) |
| 9 Witness | pedestrians, drivers, responders, CCTV (`CrimeReporter.WitnessMask`) |
| 10 Container | crane-liftable loads |
| 4 Water | lagoon / creek planes |

Tag the player rig `Player`.

## 7. Saves
JSON in `Application.persistentDataPath/save_N.json` (Newtonsoft), written via temp file and backup
copy. The app saves on pause, after every mission pass, and every 2 minutes when not wanted or on a mission.
`SaveData.Version` + `SaveData.Migrate` handle format changes.

## 8. Build settings (Android)
- Scripting backend IL2CPP, ARM64 only, managed stripping **Medium** (`link.xml` for Newtonsoft if needed).
- Texture compression ASTC. Split APKs by target architecture. Build **AAB**.
- Minimum API level 26, target the latest Play requirement.
- Graphics APIs Vulkan + OpenGLES3. Multithreaded rendering on.
- Optimized frame pacing on. Disable the unused built-in modules (VR, terrain physics if unused, etc.).

## 9. Testing
```
dotnet test Tools/CoreTests            # outside Unity (CI)
Window → General → Test Runner         # same tests inside Unity (EditMode)
```
