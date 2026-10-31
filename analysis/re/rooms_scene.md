# Room, Scene, and Asset Lifecycle Reconstruction

**Scope:** `r1_1` vertical-slice room and the engine-level room lifecycle. This is
not a full procedural-generation reconstruction.

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
has 49 sprite variants but no BoxCollider2D components in the serialized room; wall
collision likely comes from the room pattern/edge or `Physics2D` layer mask, and is
**NOT YET RECONSTRUCTED**. Do not assume the sprite bounds are collision bounds.

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

## 8. Remaining room blockers

1. Wall/floor collision is not represented by the room's serialized BoxCollider2D
   list; the actual collision source (room edge mask, tilemap, pattern) is unknown.
2. `RGRoomX.StartRoom`/`ClearRoom`/door/aisle state machines are not decoded.
3. `MapManagerLevel.CreateMap`/procedural generation is unknown.
4. Player spawn point selection (`b_point` vs `RGRoomX.entrance`) is unresolved.
5. Enemy spawn tables/AI selection are unresolved.
6. Scene loading, additive scene flow, and `DontDestroyOnLoad` usage are unresolved.

These are documented blockers; they must not be replaced by guesses.
