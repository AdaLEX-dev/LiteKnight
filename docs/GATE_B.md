# Gate B — Engine Reconstruction Report

**Date:** 2026-10-31
**Reference:** Soul Knight 2.6.8, Unity 2018.4.12f1, IL2CPP metadata v24
**Status:** **PARTIAL**

> Gate B is **not** claimed complete. The reusable engine/runtime primitives needed
> by the vertical slice are now largely understood, but four concrete blockers
> remain: wall-collision placement, room spawn/door state, weapon fire/projectile
> velocity, and full death/reborn branches. No speculative implementation may start
> until those are resolved or explicitly scoped as first-slice approximations.

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
| Scene/room | `r1_1` prefab, root `RGRoomX`/`EnemyMaker`, groups/doors/aisles | `room/r1_1` bundle; `analysis/re/rooms_scene.md` | PARTIAL | HIGH CONFIDENCE | wall-collision placement, spawn point, door/aisle state machine |
| Collision | Physics2D gravity (0,-9.81), trigger queries, layers/tags, wall collider prefab | `Physics2DSettings`, `TagManager`, `wall101/wall201` 1×1 `BoxCollider2D` | PARTIAL | HIGH CONFIDENCE | how room grid instantiates wall colliders |
| Projectile | `Bullet01`/`RGBullet` fields, trigger chain, destroy_time, rotation | `weapon_000`, `bullet_0`, `RGBullet.UpdateInfo` `0x1168000` | PARTIAL | HIGH CONFIDENCE | exact velocity/rebound/through math |
| Damage | Projectile → player/enemy/box dispatch; armor/HP mutation | `analysis/re/damage.md`; `0x116078C`, `0x1160F78`, `0x1160ADC` | CONFIRMED (core) | CONFIRMED | effect-trigger overrides/crit/element details |
| Death | Player/enemy death flags, collider/weapon disable, Animator, game-over | `0x1179ECC`, `0x110DE18` | PARTIAL | HIGH CONFIDENCE | exact virtual slots after death, reborn/game-over branches |
| Input | Legacy axes, WASD/joystick mapping, movement consumer | `InputManager` serialized axes; `RGJoyControl` `0xDD8618`/`0xDD8718`; `SetVelocity` `0x117A6E4` | CONFIRMED | HIGH CONFIDENCE | exact attack/skill/dodge button semantics |
| Object state | RoleAttribute/RoleAttributePlayer fields and HP/armor/energy | `dump.cs` fields; serialized `c00` stats | CONFIRMED | CONFIRMED | full buff/status state |
| Map/room seed | Per-object `RandomBehaviour` seed flow | `RandomBehaviour.SetUpRGRandom` `0xD31E18`, `SetRGRandomSeed` `0xD32224` | PARTIAL | HIGH CONFIDENCE | multiplayer seed distribution, DateTime conversion |
| Save/runtime boundary | Runtime state classes vs persistent save manager | `BattleData`, `PlayerSaveData`, `RGSaveManager` metadata | PARTIAL | HYPOTHESIS for format | save serialization format/versioning unknown |
| Callbacks/events | `Action`, delegate fields, virtual methods | metadata + disassembly of damage/attack/death | CONFIRMED (structure) | HIGH CONFIDENCE | exact callback order in complex flows |
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
  4 doors, 4 aisles, 8 platform effectors, and 4 animators; bounds from serialized
  door/bullet colliders are approximately `x[-2.5,28.5]`, `y[-2,29]`.
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

1. **Wall/floor collision placement.** The room prefab's 12 serialized colliders
   are only doors/door triggers/bullet colliders. Wall collision prefabs
   (`wall101`, `wall201`) have 1×1 non-trigger `BoxCollider2D`, but the runtime
   placement algorithm from `RGRoomX.map`/`MapManagerLevel` is not decoded.
2. **Spawn and door/room state.** `b_point` and `RGRoomX.entrance` exist, but player
   spawn selection, door-open/close rules, and `RGRoomX.StartRoom`/`ClearRoom` state
   transitions are not reconstructed.
3. **Weapon fire and projectile velocity.** `Gun001.Attack` → `CreateBullet` →
   `BulletFactory.TakeBullet` is identified, but exact velocity, deviation, spread,
   cooldown, ammo, and `RGBullet.SetBulletVelocity` math are not fully decoded.
4. **Full death/reborn.** Death flags/collider/weapon/Animator effects are
   documented, but the post-death virtual slots (`RGEController +0x328`,
   `+0x3A8`), game-over/restart, and reborn-card paths are not complete.
5. **Full AI/weapon/content.** Enemy AI state machines, all weapons, all rooms,
   progression, save format, and content identities remain Phase 3 work.
6. **Runtime oracle.** No approved runtime observation path exists; static evidence
   cannot reveal actual serialized runtime values, Animator state transitions, or
   physics contact results.

## 5. Gate B conclusion

**PARTIAL.** The engine/runtime primitives are understood sufficiently to design
the Phase 4 runtime and to implement the first bootstrap/renderer/asset/simulation
layers, but **not** sufficiently to claim a complete vertical-slice gameplay loop
without resolving blockers 1–4 or explicitly accepting them as approximations.

No native gameplay may start until the vertical-slice evidence gate in
`docs/VERTICAL_SLICE.md` is updated with these findings and the remaining blockers
are either closed or explicitly accepted.
