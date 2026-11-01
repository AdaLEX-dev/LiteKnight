# Level Visual Overrides

## Reference evidence

- `RGRoomX.CreateFloor @0x0DEA99C` reads the singleton
  `MapManagerLevel.floor_list`; PatternRooms do not embed floor variants.
- `RGRoomX.CreateWall @0x0DEBE3C` reads `wall_list` and selects index 0/1 with a
  30/70 roll.
- `RGRoomX.CreateAisle` / `RGAisle.CreateFloor` / `CreateAisleWall` generate the
  corridor surfaces from the same manager lists (6/94 for aisle walls, 30/70 for
  sealed door gaps). The reference uses `UnityEngine.Random` there.
- `RGRoomX.Awake` resolves the empty `floor_group` and decorative
  `wall_group` by name. All 108 PatternRooms have an empty serialized
  `floor_group`.
- `BattleData.Init @0x124D338` sets `branchIndex = "A"`. Normal campaign
  `MapManager.GetMapManagerName @0x0D26DC4` builds `map_A<stage>`; stage 16 is
  `map_G1`.
- The current `map_catalogue.json` is a first-occurrence dedup by level string,
  producing a mixed A/B/C set. It is not the default normal route. The runtime
  now selects the theme from the recovered `map_A*`/`map_G1` assets, independent
  of that mixed catalogue.

## Recovered default route

| Level | Manager | Floors | Walls |
|---|---|---|---|
| 1-1…1-5 | `map_A1`…`map_A5` | `floor501`…`floor506` | `wall_bush1`, `wall504` |
| 2-1…2-3 | `map_A6`…`map_A8` | `floor201`…`floor206` | `wall201`, `wall202` |
| 2-4…2-5 | `map_A9`…`map_A10` | `floor101`…`floor106` | `wall101`, `wall102` |
| 3-1…3-5 | `map_A11`…`map_A15` | `floor401`…`floor406` | `wall401`, `wall402` |
| 3-6 | `map_G1` | `floor401`…`floor406` | `wall403`, `wall404` |

Alternate branches are also extracted (`map_B1..B15`, `map_C1..C10`) but the
default campaign selector uses branch A.

## Native implementation

- `tools/asset/build_visual_scene.py` extracts all manager themes into the
  canonical scene as `level_themes`, including the exact floor prefab order,
  wall prefab sprite parts (root/shadow/w2), offsets, scale, tint and sorting.
- `tools/asset/merge_canonical_weapons.py` distributes the shared theme and
  surface assets to every prepared PatternRoom/special room without rebuilding
  room geometry.
- `LevelSurfaceBuilder` (`src/game/room/LevelSurface.*`) implements the exact
  floor pattern/variant algorithm and the 30/70 wall selection.
- `Application::RefreshLevelSurfaces` selects the theme by
  `BiomeSystem.Current()->id` and branch A, generates surfaces once per
  scene/level/seed, suppresses baked `native_wall/` instances, and renders the
  generated floors and walls.
- Existing wall collision is theme-independent: every recovered wall prefab has
  the same 1×1 BoxCollider2D at offset (0,0.5). Floors have no collision.
- Room geometry, door gaps (18..22), aisle anchors, interior bounds and
  collision remain the recovered PatternRoom values.

### Determinism

The reference seeds PatternRoom RNG from time/network state
(`RandomBehaviour.SetUpRGRandom @0x0D31E18`), not from `map_random_seed`. The
native replay contract instead derives a stable room seed from the map seed,
scene id and campaign level. Floor and wall use separate derived streams so a
visual theme change cannot reorder gameplay RNG.

## Tests

- `tests/unit/level_surface_test.cpp` covers branch/level lookup, deterministic
  output, geometry invariance across themes, and 30/70 two-variant wall output.
- The 108-room load test still validates every PatternRoom geometry.
- `--devroom --map-seed` logs the selected theme and measured 60 Hz present
  pacing.

## Obstacle theme overrides

- Each `LevelTheme` now carries the recovered `obstacle_list` prefab groups.
- `Application` renders the active level's obstacle group at each
  `RGObjectSkin` marker (`obj_index`), suppressing the baked room variant.
- Collision geometry remains the room's recovered solid set, as required by
  the task. No arbitrary re-skin is applied where a theme has no entry.
- Tests cover branch/level lookup and deterministic surface output; the
  `--area r1_1` visual smoke confirms theme obstacle rendering.

## Remaining limitation

- `map_catalogue.json` remains the mixed first-occurrence audit table; the
  runtime theme selection no longer depends on its floor/wall lists.
- Obstacle collision is intentionally unchanged across themes; if a future
  theme has a different obstacle collider, it needs an explicit collision
  update rather than an implicit swap.
