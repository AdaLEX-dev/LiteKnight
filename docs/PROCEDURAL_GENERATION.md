# Procedural Generation — Evidence Status and Plan

**Status:** **UNKNOWN / NOT IMPLEMENTED.**
**Scope:** reference map/room generation. This document does not invent an algorithm.

---

## 1. Confirmed evidence

- `MapManager.Start` (`0xD26A10`) seeds `MapManager.rg_random` from
  `RGGameInfo.map_random_seed` (`+0x1C`) through virtual `SetRGRandomSeed`
  (vtable `+0x1E8`).
- `MapManagerLevel.SetRGRandomSeed` (`0x103B1D8`) adds a LongMap branch:
  Normal mode, `RGGameProcess.this_index` 1..15, `CompareFactor(10)` active,
  `Range(0,100) <= 49` → increment `map_long` (`+0xC0`) by 1.
- `MapManagerLevel` fields include `room_count`, `this_room_x/y`, `the_room`,
  `room_relation`, `map_list`, `map_long`, `special_rooms`, `room_start_list`,
  `room_end_list`, `floor_list`, `wall_list`, `obstacle_list`, `enemy_infos`,
  `branch_room_list`, `usedRoom`.
- `RGRoomX` fields include `room_width`, `room_height`, `room_type`, `entrance`,
  `map`, `floor_list`, `wall_group`, `floor_group`, aisles/doors, offsets.
- `r1_1` prefab is `Assets/RGPrefab/Room/PatternRoom/r1_1.prefab`; its root has
  `RGRoomX` + `EnemyMaker` and 178 transforms.
- `EnemyMaker` owns `rg_random`, room size, level, `the_room`, `enemy_group`,
  enemy counts, and badass rates.

## 2. Unknown

- `MapManagerLevel.CreateMap` (`0x1039898`) and `CreateRoom` algorithm.
- How `room_relation`, `map_list`, `branch_room_list`, and `usedRoom` build the
  dungeon graph.
- Template selection and placement rules for `r1_1` and other pattern rooms.
- Floor/obstacle prefab selection; wall-variant random selection seed.
- Runtime entrance bit assignment only partly decoded (neighbor relation checks
  are known; the actual `r1_1` instance values are not).
- Higher-level door event-to-room-transition listener and special-room
  distribution.
- Exact use of `map_long`, `room_count`, `special_rooms`, `room_start_list`,
  `room_end_list`.
- Enemy spawn table selection from `EnemyMaker`/`MapManagerLevel.enemy_infos`.

## 3. Native plan (after evidence)

1. Represent room templates as converted JSON descriptors (nodes, transforms,
   sprites, colliders, doors, spawn points).
2. Represent the map graph explicitly (rooms + connections).
3. Implement seed → map RNG stream → room selection/placement only after the
   reference algorithm is reconstructed.
4. Keep `r1_1` as a fixed prefab-loaded slice; do not claim procedural generation.
5. Add deterministic tests only when the reference seed→layout mapping is known.

## 4. Blockers

- No reconstructed generation algorithm.
- No runtime oracle to compare layouts for fixed seeds.
- The fixed `r1_1` wall/door/aisle geometry is recovered; map-level template
  selection, entrance assignment, and transition orchestration remain unknown.

**Do not implement BSP/graph/template generation as if it were reference behavior
without evidence.**
