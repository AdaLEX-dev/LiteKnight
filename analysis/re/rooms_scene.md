# Room, Scene, and Asset Lifecycle Reconstruction

**Scope:** `r1_1` reconstruction test room and the engine-level room lifecycle.
This is not a full procedural-generation reconstruction. The native
`r1_1_visual` scene is a controlled reverse-engineering test chamber assembled
from real `r1_1` data; it is not proven to be the game's canonical starting level
or character-select scene.

---

## 1. Asset identity

Bundle manifest entries:

```text
Assets/RGPrefab/Room/PatternRoom/r1_1.prefab
Assets/RGPrefab/Enemy/2-castle/e_slime01.prefab
Assets/RGPrefab/Player/c00.prefab
Assets/RGPrefab/Player/c01.prefab
Assets/RGPrefab/Weapon/weapon_000.prefab
Assets/RGPrefab/Bullet/bullet_0.prefab
```

`room/r1_1` bundle SHA-256 prefix: `cef33f8731952a95...` (full hash in
`analysis/assets/extraction_manifest.json`).

The room bundle contains 178 GameObjects. The root is `r1_1` (path ID
`-7759221202696162998`) with:

- `RGRoomX`
- `EnemyMaker`

## 2. Room hierarchy (CONFIRMED from serialized Unity objects)

Counts from the root hierarchy:

| Component/script | Count |
|---|---:|
| Transform | 178 |
| SpriteRenderer | 109 |
| MonoBehaviour | 63 |
| BoxCollider2D | 12 |
| PlatformEffector2D | 8 |
| Animator | 4 |
| `RGObjectSkin` | 49 |
| `RGAisle` | 4 |
| `RGDoor` | 4 |
| `RGDoorTrigger` | 4 |
| `RGRoomX` | 1 |
| `EnemyMaker` | 1 |

Named groups/objects include:

```text
r1_1
├── wall_group        (skin_wall × 49, RGObjectSkin)
├── floor_group       (floor × 4)
├── enemy_group
├── aisle_n/e/w/s     (RGAisle)
├── door × 4          (RGDoor, Animator, BoxCollider2D, PlatformEffector2D)
├── door_trigger × 4  (RGDoorTrigger, BoxCollider2D, trigger)
├── bullet_collider × 4 (BoxCollider2D, PlatformEffector2D)
├── b_point
└── props (p, d, shadow, wall, floor …)
```

## 3. Collision geometry

The 12 `BoxCollider2D` objects are all on doors/door triggers/bullet colliders:

| Object | Center (x,y) | Size | Trigger | Effector |
|---|---|---:|---:|---:|
| door | 0.0, 0.5 | 5.0×1.0 | no | yes |
| door_trigger | 20.0, 14.15 | 5.0×1.5 | yes | no |
| bullet_collider | 20.0, 12.5 | 5.0×1.0 | no | yes |
| door | 0.0, 0.5 | 1.0×5.0 | no | yes |
| door_trigger | 13.65, 20.5 | 1.5×5.0 | yes | no |
| bullet_collider | 12.0, 20.5 | 1.0×5.0 | no | yes |
| door | 0.0, 0.5 | 1.0×5.0 | no | yes |
| door_trigger | 26.35, 20.5 | 1.5×5.0 | yes | no |
| bullet_collider | 28.0, 20.5 | 1.0×5.0 | no | yes |
| door | 0.0, 0.5 | 5.0×1.0 | no | yes |
| door_trigger | 20.0, 26.85 | 5.0×1.5 | yes | no |
| bullet_collider | 20.0, 28.5 | 5.0×1.0 | no | yes |

Approximate playable bounds from these colliders:

```text
x ∈ [-2.5, 28.5]
y ∈ [-2.0, 29.0]
```

Door/aisle geometry is symmetrical around the 20×20 room center. The wall group
has 49 sprite variants and no BoxCollider2D components in the serialized room.
Wall collision is **runtime-generated**, not part of the serialized 12 colliders:
`RGRoomX.CreateWall` (`0xDEBE3C`) instantiates prefabs from
`MapManagerLevel.wall_list` (`+0x100`) around the room perimeter, parents them to
`RGRoomX.wall_group` (`+0x68`), and assigns `RGRoomX.EdgeLayer` (layer 17, `Edge`).
The wall prefabs (`wall101`, `wall201`) carry a 1×1 non-trigger `BoxCollider2D`
with offset `(0,0.5)`.

Serialized `RGRoomX` values from UnityPy `parse_as_dict`:

```text
room_width = 15
room_height = 15
room_type = 1
process = 0
entrance = [0,0,0,0]
floor_list = []
door_open = 1
room_x_offset = 0
room_y_offset = 0
```

`CreateWall` loops the room perimeter using `room_x_offset`, `room_y_offset`,
`room_width`, and `room_height`. The outer pass places top/bottom walls; the inner
pass places left/right walls. It draws `RGRandom.Range(0,100)` twice for wall
variant selection and writes the room edge layer. Exact per-cell door-gap skip
conditions are not fully decoded; the 4 serialized `door`/`door_trigger` objects
and their collider sizes are the evidence for door openings.

## 4. Room scripts

`RGRoomX` fields (`dump.cs` TypeDefIndex 7426):

```text
process, room_width, room_height, room_type
wall_level, obstacle_level, random_room
e_maker
room_y, room_x
int[] entrance
int[,] map
List<Vector2> floor_list
Transform wall_group, floor_group
RGAisle aisle_n/e/w/s
RGDoor door_n/e/w/s
bool door_open
room_x_offset, room_y_offset
special_box_rate
create_with_close_state
need_cage
RoomExtraData room_extra_data
```

Relevant methods:

- `Awake` `0xDE97BC`
- `Start` `0xDE9B0C`
- `CreateRoomObstacle` `0xDE9DA8`
- `StartRoom(Vector2)` `0xDEA05C`
- `ClearRoom` (virtual)
- `OnSetRGRandomSeed` `0xDEE7B0`
- `SetRGRandomSeed` inherited from `RandomBehaviour`

`EnemyMaker` fields (`dump.cs` TypeDefIndex 6803):

```text
RGRandom rg_random; int room_size, max_room_size;
int level; int enemy_point; int this_count; int part;
RGRoomX the_room; Transform enemy_group;
int enemy_total_count; int badass_rate;
bool always_badass, all_badass;
```

`EnemyMaker` is the room-local enemy spawn owner. The actual spawn table and AI
selection are unresolved.

`RGDoor`, `RGDoorTrigger`, `RGAisle`, and `RGObjectSkin` are serialized scripts on
the room prefab; their full state machines were not decoded.

## 5. Map lifecycle

`MapManager` fields:

```text
RGRandom rg_random; // +0x58
string level;       // +0x60
AudioClip bgm_clip; // +0x68
Color camera_bg;    // +0x70
Vector2Int centerPoint; // +0x80
List<RGRoomX> all_room; // +0x88
bool enterNextMap;      // +0x90
static MapManager _Instance; // 0x0
```

`MapManager.Start` (`0xD26A10`):

1. resolves `RGGameInfo` singleton,
2. loads `RGGameInfo.map_random_seed` (`+0x1C`),
3. calls virtual `SetRGRandomSeed(seed)` through vtable `+0x1E8`,
4. continues map/scene setup.

`MapManagerLevel.SetRGRandomSeed` (`0x103B1D8`) additionally:

- calls base `MapManager.SetRGRandomSeed` (seeds `rg_random`),
- returns unless `RGGameProcess.game_type == Normal (0)`,
- requires `RGGameProcess.this_index >= 1`,
- requires `BattleData.IsNormalMode`,
- requires `BattleData.CompareFactor(10)` (`emBattleFactor.LongMap`),
- requires `RGGameProcess.this_index <= 15`,
- draws `map_random.Range(0,100)`; if `<= 49` (50% chance), increments
  `MapManagerLevel.map_long` (`+0xC0`).

`MapManagerLevel.CreateMap` (`0x1039898`) and `CreateRoom` are not fully decoded.
The room-generation algorithm remains **UNKNOWN**; only the prefab and seed path
are confirmed.

## 6. Scene/object lifecycle

Confirmed Unity mechanisms in use:

- `MonoBehaviour` lifecycle messages (`Awake`, `Start`, `Update`, `FixedUpdate`,
  `OnDestroy`, `OnDisable`).
- `RGRandomInterface` and `RandomBehaviour.SetRGRandomSeed` / `OnSetRGRandomSeed`
  for per-object seed propagation.
- `DontDestroyOnLoad`/scene flow is not yet traced.
- `RGController.Dead` disables colliders and weapons, sets dead flags, and triggers
  the Animator; see `analysis/re/damage.md`.
- `RGBullet.DestroySelf` starts the `Destroying` coroutine; `destroy_time` controls
  the delay.
- `PrefabPool`/`IPrefabPoolObject` are present; `PrefabPool.Take` is used by
  projectile/spawn code. Exact pooling policy is not decoded.

Native runtime must keep explicit lifetime ownership; do not leak Unity-style
implicit destruction semantics.

## 7. Asset identity classification

| Asset | Classification | Evidence |
|---|---|---|
| `r1_1` room prefab | **CONFIRMED** | manifest path + GameObject root + `RGRoomX`/`EnemyMaker` |
| `c00` player prefab | **CONFIRMED technical** | manifest path + `C01Controller` + `RoleAttributePlayer` |
| `c01` player prefab | **CONFIRMED technical** | manifest path + `C02Controller` + `RoleAttributePlayer` |
| `e_slime01` enemy prefab | **CONFIRMED technical** | manifest path + `EnemyAI04` + `RoleAttribute` |
| `enemy01` sprite family | **CANDIDATE** | sprite names + bundle; no controller identity |
| `weapon_000` | **CONFIRMED weapon prefab** | manifest path + `Gun001` + `WeaponItemConfig` |
| `bullet_0` | **CONFIRMED projectile prefab** | manifest path + `Bullet01` |
| Knight/Rogue/slime/pistol display names | **CANDIDATE/UNRESOLVED** | names/indexes only; no serialized display-name mapping |

## 8. Runtime wall generation, doors, and spawn (new evidence)

### 8.1 Runtime wall generation

- `RGRoomX.CreateWall` (`0xDEBE3C`) is the collision source. It is not called by a
  direct `bl` from `RGRoomX.Start`; it is reached through the room setup/virtual
  path. Its behavior is CONFIRMED from disassembly.
- It reads `MapManagerLevel.wall_list` (`+0x100`; 1×1 wall prefabs with
  non-trigger `BoxCollider2D`), instantiates them, parents to `wall_group`, and
  sets layer `RGRoomX.EdgeLayer` (17).
- It loops the room perimeter from `room_x_offset - 1` to
  `room_x_offset + room_width` and from `room_y_offset` to
  `room_y_offset + room_height`; the separate left/right pass covers the
  vertical edges.
- Wall prefabs are selected with `RGRandom.Range(0,100)` (<30 selects a variant);
  the exact per-cell skip/gap condition is **PARTIAL**.
- The 12 serialized colliders (doors/door triggers/bullet colliders) are **not**
  the complete playable collision geometry.

### 8.2 Doors and room state

- `CloseDoor` (`0xDEA4AC`) calls `RGDoor.CloseDoor` on all four door fields
  (`+0x98`, `+0xA0`, `+0xA8`, `+0xB0`) and sets `door_open = false`.
- `OpenDoor` (`0xDEA53C`) calls `RGDoor.OpenDoor` on all four doors and sets
  `door_open = true`.
- `ClearRoom` (`0xDEA5D0`) sets `process = 2`, calls a virtual method at
  `+0x228`, plays the room-clear SFX, and sends `SendRoomClearMessage` in
  multiplayer. It does not itself reload the scene.
- `IsClear` (`0xDEBDD8`) returns false if `EnemyMaker.this_count` is nonzero,
  otherwise true if the maker's virtual count is zero.
- `StartRoom` (`0xDEA1A0`) sets `MapManagerLevel.the_room`, `this_room_x/y`,
  `process`, boss info, and game-state delegates; the full door/aisle state
  machine remains PARTIAL.
- `RGDoor`/`RGDoorTrigger`/`RGAisle` are serialized components on `r1_1`; their
  transition animation/timing is not fully decoded.

### 8.3 Player spawn and `b_point`

- `RGGameSceneManager.CreatePlayer` (`0xDCD3C0`) reads
  `MapManager.centerPoint` (`+0x80`) and builds the initial player position from
  it. Player spawn is therefore **MapManager.centerPoint**, not `b_point`.
- `RGRoomX.LoadRoom` (`0xDEA7CC`) calls `CreateFloor`, then `Transform.Find("b_point")`,
  iterates `b_point`'s children, converts each child to a world position, subtracts
  `room_x_offset`/`room_y_offset`, and appends the result to `RGRoomX.floor_list`
  (`+0x60`). `b_point` is a **valid floor/spawn marker group**, not the player
  root spawn by itself.
- `GetRandomEmptyPos` (`0xDEC9B4`) scans room cells and returns a free position;
  its exact callers for enemy/object spawning remain PARTIAL.

### 8.4 Remaining room blockers

1. Exact per-cell wall-gap/door-opening placement algorithm is PARTIAL.
2. Full `StartRoom`/`ClearRoom`/door/aisle transition state machine is PARTIAL.
3. Procedural `MapManagerLevel.CreateMap` is UNKNOWN.
4. Enemy spawn table/AI selection is UNKNOWN.
5. Scene loading/additive flow is UNKNOWN.
6. `b_point`/`floor_list` consumer selection is only partly traced.

These are named blockers; a first slice may use the serialized door openings and a
rectangular perimeter wall set, explicitly labelled as an approximation.
