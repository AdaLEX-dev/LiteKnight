# Gate B — Engine Reconstruction Report

**Date:** 2026-10-31
**Reference:** Soul Knight 2.6.8, Unity 2018.4.12f1, IL2CPP metadata v24
**Status:** **PARTIAL**

> Gate B is **not** claimed complete. The reusable engine/runtime primitives needed
> by the first Phase 4 stages are understood. Wall/door placement, wall-prefab
> collision and closed-door traversal are now recovered; runtime entrance
> assignment for a generated `r1_1`, the event-to-map-manager listener, door
> transition timing, weapon cadence/spread-field identity, and full death/reborn
> slots remain PARTIAL. The first slice continues to use explicitly labelled
> approximations; full parity remains Phase 3 work. No unlabelled speculative
> implementation may start.

---

## 1. What Gate B requires

Gate B means the reusable engine/runtime primitives are understood well enough that
native implementation can begin without inventing behaviour. It is broader than
"the code compiles" and narrower than full content parity.

Canonical evidence documents:

- `analysis/re/rgrandom.md`
- `analysis/re/damage.md`
- `analysis/re/timing.md`
- `analysis/re/input_movement.md`
- `analysis/re/rooms_scene.md`
- `analysis/re/weapons_projectiles.md`
- `analysis/re/enemy_ai04.md`
- `analysis/re/combat_prototype.md`
- `analysis/assets/extraction_manifest.json`
- `docs/EVIDENCE_STANDARD.md`

## 2. Evidence matrix

| System | Requirement | Evidence | Status | Confidence | Unresolved work |
|---|---|---|---|---|---|
| RNG core | Unity 2018.4 Xorshift128, LCG seeding, 23-bit floats, int/float range branches | `libunity.so` `0x37AAA0..0x37BA70`; `analysis/re/rgrandom.md` | CONFIRMED | CONFIRMED | Runtime reference vectors unavailable |
| RNG wrapper | `seed_state`, `seed`, `this_state`, lazy seed, global save/restore | `RGRandom` `0xDD07E8`, `0xDE9370`, `0xDE93DC`, `0xDE94B0`, `0xDE95C8` | PARTIAL | HIGH CONFIDENCE | `seed_state` 1/2 unused; DateTime seed expression not exact |
| Map seed | MapManager → `map_random_seed` → virtual `SetRGRandomSeed` | `MapManager.Start` `0xD26A10`; vtable `+0x1E8` | CONFIRMED | CONFIRMED | none for normal mode |
| LongMap | Normal mode, index 1..15, `CompareFactor(10)`, 50% increment `map_long` | `MapManagerLevel.SetRGRandomSeed` `0x103B1D8` | CONFIRMED | CONFIRMED | none |
| Time | Fixed 50 Hz step, max timestep, time scale | serialized `TimeManager` in `globalgamemanagers` | CONFIRMED | CONFIRMED | coroutine order vs FixedUpdate unobserved |
| Math | Unity 2D vectors, quaternion composition, Y-up world, metres/units | Unity API metadata; room collider coordinates | CONFIRMED | HIGH CONFIDENCE | exact float math parity not runtime-verified |
| Lifecycle | Awake/Start/Update/FixedUpdate/OnDestroy; coroutine destruction; pooling | `RGBullet.DestroySelf`; `PrefabPool`; metadata | PARTIAL | HIGH CONFIDENCE | exact pool ownership and destruction order |
| Scene/room | `r1_1` prefab, root `RGRoomX`/`EnemyMaker`, groups/doors/aisles; `room_width=15`, `room_height=15`; 41-unit map cell; offsets 13/13 | `room/r1_1` bundle; `analysis/re/room_topology.md` | PARTIAL | HIGH CONFIDENCE | runtime entrance bits for a generated instance; event-to-map-manager listener; door transition timing |
| Collision | Physics2D gravity (0,-9.81), trigger queries, layers/tags; c00 physical CircleCollider2D r=0.4 offset (0,0.5) | `Physics2DSettings`, `TagManager`, serialized c00 | CONFIRMED | HIGH CONFIDENCE | native swept AABB approximates the circle and Unity solver; wall collision is runtime-generated from `CreateWall`/`CreateAisle` |
| Projectile | `Bullet01`/`RGBullet` fields, trigger chain, destroy_time, rotation; BulletInfo speed 36 overrides prefab 10 | `weapon_000`, `bullet_0`, `RGBullet.UpdateInfo` `0x1168000` | PARTIAL | HIGH CONFIDENCE | exact velocity/rebound/through and fire cadence |
| Damage | Projectile → player/enemy/box dispatch; armor/HP mutation | `analysis/re/damage.md`; `0x116078C`, `0x1160F78`, `0x1160ADC` | CONFIRMED (core) | CONFIRMED | effect-trigger overrides/crit/element details |
| Death | Player/enemy death flags, collider/weapon disable, Animator, game-over; Reborn/ForceReborn/DoReborn; TryAgain/NextScene | `0x1179ECC`, `0x110DE18`, `0x117E27C`, `0x111F0CC` | PARTIAL | HIGH CONFIDENCE | exact +0x328/+0x3A8/+0x3E8 slot owners and UI sequencing |
| Input | Legacy axes, WASD/joystick mapping, movement consumer | `InputManager` serialized axes; `RGJoyControl` `0xDD8618`/`0xDD8718`; `SetVelocity` `0x117A6E4` | CONFIRMED | HIGH CONFIDENCE | exact attack/skill/dodge button semantics |
| Object state | RoleAttribute/RoleAttributePlayer fields and HP/armor/energy | `dump.cs` fields; serialized `c00` stats | CONFIRMED | CONFIRMED | full buff/status state |
| Map/room seed | Per-object `RandomBehaviour` seed flow | `RandomBehaviour.SetUpRGRandom` `0xD31E18`, `SetRGRandomSeed` `0xD32224` | PARTIAL | HIGH CONFIDENCE | multiplayer seed distribution, DateTime conversion |
| Save/runtime boundary | Runtime state classes vs persistent save manager; PlayerPrefs string/int keys | `BattleData`, `PlayerSaveData`, `RGSaveManager`; `analysis/re/save.md` | PARTIAL | CONFIRMED PlayerPrefs usage; UNKNOWN payload | `AboGameData` serialization/keys unknown; not required for first slice |
| Callbacks/events | `Action`, delegate fields, virtual methods; death/reborn/room callbacks | metadata + disassembly of damage/attack/death | CONFIRMED (structure) | HIGH CONFIDENCE | exact callback order in complex flows |
| Unity API assumptions | Legacy Input, Physics2D, built-in renderer, Time | `PlayerSettings`, `GraphicsSettings`, `InputManager` | CONFIRMED | HIGH CONFIDENCE | custom shader/renderer path not decoded |

## 3. Additional confirmed engine facts

- Reference fixed timestep is **0.02 s (50 Hz)**, not an assumption.
- Physics2D gravity is `(0, -9.81)`; auto simulation, auto sync transforms,
  queries hit triggers, and `m_ReuseCollisionCallbacks = False`.
- Layers: `Body`, `Body_P`, `Body_E`, `Bullet`, `Obstacle`, `Wall`, `Door`,
  `Body_pet`, `Penetrate`, `Edge`, `Bunker`, `Body_Enemy`, `HighLight`, `Fly`.
- Tags: `Body_P`, `Body_E`, `Wall`, `Chest`, `Explode`, `Weapon`, `Sword`,
  `Bullet`, `Body`, `Box`, `Pet`, `Body_P_stealth`, `Edge`, `WildPet`,
  `BulletMod`, `Undefined`, `Vacant`.
- `RGController.GetHurt` is vtable slot 16 (`+0x228`); `RGEController.GetHurt`
  is slot 26 (`+0x2C8`), with actual `Il2CppClass` vtable base `0x128` and
  16-byte `VirtualInvokeData`.
- `RoleAttributePlayer.CanDie` checks `RGGameProcess.this_index` (`+0x1C`) against
  `-1`/`-3`; it is not `RGGameInfo.map_random_seed`.
- `r1_1` prefab contains 178 transforms, 12 box colliders, 4 door triggers,
  4 doors, 4 aisles, 8 platform effectors, and 4 animators; serialized
  `room_width=15`, `room_height=15`. Wall collision is generated at runtime from
  `MapManagerLevel.wall_list` prefab `wall601`/`wall602`: 1×1 non-trigger
  `BoxCollider2D`, offset `(0,0.5)`, tag `Wall`. The prefab is serialized on
  layer 13 `Wall`; `RGRoomX.cctor` sets `EdgeLayer=17` and generated instances are
  assigned to layer 17 `Edge`.
- `RGRoomX.CreateWall` skips the five cells `18..22` on every side; runtime
  offsets are `(41-15)/2 = 13`; the interior is `[12.5,13]..[27.5,28]`.
- `RGRoomX.CreateAisle` opens or wall-fills each side from the
  `[east,south,west,north]` entrance array. Serialized `r1_1` entrance is
  `[0,0,0,0]`; actual instances receive bits from `room_relation`.
- Door barriers are `5×1`/`1×5`; triggers are `5×1.5`/`1.5×5`; open clips set
  `isTrigger=true`; closed clips restore full solid colliders with a one-way
  inward `PlatformEffector2D`.
- Player spawn comes from `MapManager.centerPoint`; `b_point` children are added
  to `RGRoomX.floor_list` as valid floor/spawn positions.
- `Gun001.Attack` → `CreateBullet` → `BulletFactory.TakeBullet`; weapon bullet
  speed 36 overrides `bullet_0` prefab speed 10; `weapon_000` has `consume=0`.
- `EnemyAI04.OnTriggerEnter2D` calls `HitUnit(other, 3, 1.5)`; `e_slime01` has
  hp 12, speed 3.0, damage 3.
- `PlayerSaveData` is backed by Unity `PlayerPrefs` string/int keys; payload
  serialization remains UNKNOWN and is not required for the first slice.
- `c00` uses `C01Controller` and `RoleAttributePlayer` (speed 6.5, hp 6, armor 5,
  atk 4, critical 5, c_index 0); `c01` uses `C02Controller` (hp 5, armor 3,
  atk 5, critical 10, c_index 1).
- `e_slime01` uses `EnemyAI04` and `RoleAttribute` (hp 12, speed 3.0, damage 3,
  enemyTag 66, levelScene 20).
- `weapon_000` is a `Gun001` weapon prefab with bullet proto `bullet_0`,
  speed 36, damage 3, critic 0, deviation 5, weapon_type 14.
- `bullet_0` is a `Bullet01` prefab with speed 10, destroy_time 5, can_rebound 1,
  camp 1.

## 4. Remaining blockers

1. **Room traversal beyond the prefab.** Wall/door placement, wall-prefab
   colliders, four five-cell openings, door barrier/trigger geometry, and
   closed-door one-way collision are now CONFIRMED and implemented. Remaining
   PARTIAL: the exact entrance bits of any generated `r1_1`, the
   event-to-`MapManagerLevel` listener (`IntoAisle`), door animation transition
   timing, and adjacent-room generation.
2. **Weapon cadence/spread field.** `Gun001.Attack → CreateBullet →
   BulletFactory.TakeBullet` is confirmed; speed 36, deviation 5, and a
   `Random.Range(-spread,+spread)` angle are reconstructed. Exact cooldown and the
   controller/hand field at `+0x2c` remain PARTIAL. First-slice approximation: fixed
   documented attack interval, no claim of reference fire rate.
3. **Full death/reborn slots.** Entry flows and `UIWindowReborn.BtnYesClick →
   Reborn → DoReborn` are confirmed; exact virtual slot owners (`+0x328`, `+0x3A8`,
   `+0x3E8`), UI sequencing, and every network/reborn branch remain PARTIAL.
   First slice can implement death + restart; reborn can be deferred.
4. **Full AI/weapon/content.** `EnemyAI04` minimal contact behavior is known, but
   the complete Scout/AI state machine, all weapons, all rooms, progression, and
   content identities remain Phase 3 work.
5. **Procedural generation/save.** `MapManagerLevel.CreateMap` is UNKNOWN; the save
   payload is UNKNOWN. Neither is required for the fixed `r1_1` first slice.
6. **Runtime oracle.** No approved runtime observation path exists; static evidence
   cannot reveal live Animator transitions, contact ordering, or RNG values.

## 5. Gate B conclusion

**PARTIAL.** The engine/runtime primitives are understood sufficiently to design
the Phase 4 runtime and to implement the first bootstrap/renderer/asset/simulation
layers. The first slice may use the following **explicitly accepted approximations**:

- all-open entrance state for the single-room `r1_1` test reconstruction,
- enclosing-AABB approximation of the c00 circle collider and door transition
  timing,
- fixed documented attack interval for `weapon_000`,
- death + restart without a full reborn-card flow,
- minimal `EnemyAI04` contact behavior.

The approximations must be labelled in code and in `docs/PARITY.md`; they are not
reference parity. Full Gate B/Phase 3 parity remains PARTIAL, and no vertical-slice
gameplay may be called complete until the `docs/VERTICAL_SLICE.md` gates pass.
