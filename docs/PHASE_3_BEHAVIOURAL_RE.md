# Phase 3 — Behavioural Reverse Engineering Report

**Date:** 2026-10-31
**Status:** **PARTIAL**
**Scope:** all reconstructed behaviour in `analysis/re/` plus the vertical-slice
subset. This is not a claim of full-game behavioural parity.

---

## 1. Status statement

Phase 3 has produced a usable behavioural contract for the first native vertical
slice, but it has **not** reconstructed the full game. The following are complete
for the slice:

- engine RNG algorithm and wrapper,
- fixed timestep and frame split,
- player/enemy damage state mutation and projectile target dispatch,
- input → movement contract,
- `r1_1` room asset/component inventory, runtime wall-collision source, door
  open/close/clear entry points,
- player/enemy/box damage bridge and player/enemy death entry effects,
- one confirmed weapon/projectile pair (`weapon_000`/`bullet_0`), including
  `Gun001.Attack → CreateBullet → BulletFactory.TakeBullet` and the effective
  speed-36 path,
- `EnemyAI04` contact behavior and the minimum `e_slime01` contract,
- save boundary: Unity `PlayerPrefs` string/int keys (payload UNKNOWN).

The following are **not** complete:

- exact per-cell wall/door gap placement and the full room/door state machine,
- exact weapon fire cooldown and spread-field identity,
- full enemy AI state machines,
- all content identities and all weapons/enemies/rooms,
- save payload serialization/versioning,
- full reborn/game-over UI sequencing,
- runtime-observed vectors and behaviours.

## 2. What is known (CONFIRMED)

### 2.1 Engine/runtime

- Unity 2018.4.12f1, IL2CPP metadata v24, Unity Xorshift128 RNG.
- Fixed timestep **0.02 s (50 Hz)**; render target 60 FPS.
- `Physics2D` gravity `(0,-9.81)`, auto simulation, auto sync transforms,
  queries hit triggers, collision callbacks not reused.
- Legacy `Input` axes and WASD/joystick mappings.
- `RoleAttribute`/`RoleAttributePlayer` field layout and HP/armor/energy mutations.
- `RGRandom` layout, `SetRandomSeed`, lazy seed from
  `RGGameInfo.sample_random_seed`, map seed path, `MapManagerLevel` LongMap branch.
- `RGController.AwakeController` seeds `rg_random` with
  `sample_random_seed + skillIndex`.

### 2.2 Damage and death

- Player target: `RGController.GetHurt` (vtable `+0x228`, slot 16).
- Enemy target: `RGEController.GetHurt` (vtable `+0x2C8`, slot 26).
- Box target: `RGBox.Hit` (vtable `+0x168`).
- `GetComponent` generic method identities from `script.json`:
  - `GetComponent<RGController>` → `0x32542B0`
  - `GetComponent<RGEController>` → `0x32542C8`
  - `GetComponent<RGBox>` → `0x3254270`
- `RoleAttributePlayer.CanDie` checks `RGGameProcess.this_index` against `-1`/`-3`.
- Player death disables collider/weapon, sets dead flags, triggers Animator,
  handles local game-over/save state.
- Enemy death sets dead flag, disables collider, removes delegates, invokes virtual
  death methods.

### 2.3 Room / assets

- `r1_1` root has `RGRoomX` + `EnemyMaker`; 178 transforms, 109 sprites,
  63 MonoBehaviours, 12 box colliders, 8 platform effectors, 4 animators.
- `c00` → `C01Controller` + `RoleAttributePlayer`; `c01` → `C02Controller`.
- `e_slime01` → `EnemyAI04` + `RoleAttribute`.
- `weapon_000` → `Gun001`; `bullet_0` → `Bullet01`.
- Asset manifest records source bundle, hashes, path IDs, types, names, output paths.

### 2.4 Input / movement

- WASD raw key codes 119/115/100/97; joystick axes with deadzone 0.19.
- `MovementInput { Vector2 dir; float weight; }`.
- `C01Controller.FixedUpdate` → base `SetVelocity` slot 21.
- `SetVelocity` writes `Rigidbody2D.velocity`; speed derives from
  `RoleAttributePlayer.speed`/`speed_rate` plus controller bonus.
- c00: speed 6.5, speed_rate 0.5, hp 6, armor 5, atk 4, critical 5.
- c01: speed 6.5, speed_rate 0.5, hp 5, armor 3, atk 5, critical 10.

### 2.5 Late evidence pass (2026-10-31)

- `RGRoomX.CreateWall` (`0xDEBE3C`) instantiates wall prefabs from
  `MapManagerLevel.wall_list` (`+0x100`), parents them to `wall_group`, and sets
  `RGRoomX.EdgeLayer` = 17 (`Edge`). Wall prefabs `wall601`/`wall602` are
  serialized on layer 13 (`Wall`) and have a 1×1 non-trigger `BoxCollider2D` at
  offset `(0,0.5)`. The 12 serialized
  colliders are not the complete playable geometry.
- `CreateWall` skips the five cells `18..22` on every side; runtime offsets are
  `(41-15)/2 = 13`; wall rows are `y=12/28`, columns `x=12/28`; interior is
  `[12.5,13]..[27.5,28]`. `CreateAisle` opens each side or fills the five-cell gap
  with wall prefabs from the `[east,south,west,north]` entrance array.
- Serialized `r1_1`: `room_width=15`, `room_height=15`, `room_type=1`,
  `door_open=1`, template offsets 0. Four serialized door barriers/triggers and
  Animator open/close clips are recovered; closed doors are one-way inward.
- `RGGameSceneManager.CreatePlayer` uses `MapManager.centerPoint` for the player
  spawn. `RGRoomX.LoadRoom` finds `b_point`, adds its child positions to
  `RGRoomX.floor_list`; `b_point` is a valid floor/spawn marker group.
- `StartRoom` sets the current room/index/process/boss state; `CloseDoor`/`OpenDoor`
  call `RGDoor.Close/Open` on all four doors; `ClearRoom` sets process 2, plays SFX,
  and sends the room-clear message; `IsClear` checks `EnemyMaker`.
- `Gun001.Attack` → `CreateBullet` → `BulletFactory.TakeBullet`; `weapon_000`
  `deviation=5`, `consume=0`, bullet speed 36; `bullet_0` prefab speed 10 is
  overwritten by `BulletInfo.speed` in `RGBullet.UpdateInfo`.
- `Gun001.CreateBullet` computes `spread = deviation * (field+1)` and calls
  `RGRandom.Range(-spread,+spread)`.
- `RGController.Reborn/ForceReborn/DoReborn` and
  `UIWindowReborn.BtnYesClick → Reborn` are confirmed; `RGGameProcess.GameFail` /
  `TryAgain` / `ReSetInfo` handle game-over/restart/reset.
- `EnemyAI04.OnTriggerEnter2D` calls `HitUnit(other, 3, 1.5)`; `e_slime01` stats
  are hp 12, speed 3.0, damage 3.
- `RGSaveManager`/`PlayerSaveData` use Unity `PlayerPrefs` string/int keys; the
  `AboGameData` payload is not decoded.

## 3. What is HIGH CONFIDENCE

- Critical chance is rolled with `UnityEngine.Random.Range(0,100)` compared to
  `DamageTrigger.critical` (`+0x1C`); in `RGBulletTrigger` the resulting bool is
  passed to effect triggers/knockback and does **not** multiply the base
  `GetFinalDamage()` value on the observed paths.
- `RGBulletTrigger.GetFinalDamage` multiplies base damage by effect
  `damageFactor` and truncates to int.
- `RGBulletTrigger.GetDamageFactor` returns 0.5 when `has_ice_buff` and
  `BattleData.HasBuff(23)` (`emBuff.ShieldIce`), else 1.0.
- `BattleData.CompareFactor(6)` is `emBattleFactor.EnemyDefence`; `(8)` is
  `HardShield`.
- `MapManagerLevel.SetRGRandomSeed` LongMap branch: Normal mode, this_index 1..15,
  LongMap active, 50% chance increment `map_long`.
- `RandomBehaviour.SetUpRGRandom` seeds new `RGRandom` from time/network; the
  single-player branch is time-based.
- `Gun001.Attack` calls `RGWeapon.bullet_speed`, then `Gun001.CreateBullet`; that
  method uses `RGRandom.Range` for deviation and calls
  `BulletFactory.TakeBullet(BulletInfo, DamageInfo, …)`.
- `RGRoomX` instantiates wall prefabs from `MapManagerLevel.wall_list`;
  `wall601`/`wall602` have 1×1 non-trigger colliders. Floor/obstacle prefab
  selection remains only partly traced.

## 4. What remains HYPOTHESIS

- Display identities: `c00` = "Knight", `c01` = "Rogue", `e_slime01` = "slime",
  `weapon_000` = "Bad Pistol"/Knight starting weapon. The prefab/controller/stat
  identities are confirmed; the display-name mapping is not.
- The meaning of `RGGameProcess.this_index` sentinel values `-1`/`-3` (likely
  non-gameplay scenes/modes).
- Entrance bits of a specific generated `r1_1`; wall variant selection by seed.
- Higher-level door/aisle transition listener and player spawn selection in a
  generated map.
- Exact projectile velocity formula and whether the prefab speed 10 or weapon
  bullet speed 36 is the initial velocity.
- Whether critical hits ever multiply HP damage in any weapon/effect subclass
  outside the observed `RGBulletTrigger` path.
- Element/status damage factors beyond the confirmed ice-buff 0.5 factor.

## 5. What remains UNKNOWN

- Full `EnemyAI01..20` state machines, attack patterns, and coroutine timing.
- Full weapon catalogue and `RGWeapon`/`RGHand`/`RGBaseHand` state machines.
- Procedural map generation (`MapManagerLevel.CreateMap`, room graph, templates).
- Event-to-`MapManagerLevel` traversal listener and adjacent-room transition
  orchestration; door/aisle geometry and closed-door collision are recovered.
- Save file format/versioning (`RGSaveManager.Save/Load`, `AboGameData`).
- Full reborn/revive/game-over/restart branches.
- Animation event semantics and Animator controller state machines.
- Status stacking/refresh/duration rules.
- Multiplayer seed/hurt/dead synchronization (out of scope for v1).
- Shader/material pipeline and exact reference rendering.

## 6. What cannot be statically recovered

Static analysis cannot provide:

- runtime values of serialized fields that are not exposed by the typetree,
- actual Animator state transitions and animation events,
- actual Physics2D contact results and collision ordering,
- actual coroutine scheduling relative to FixedUpdate,
- actual RNG sequences from a live session,
- display names/IDs that exist only in remote/server data or localization tables
  not present in the extracted assets,
- platform/GPU-specific rendering behavior.

These require an approved native/observation path or a trusted offline reference
implementation; Android runtime, Waydroid, emulator, Wine, X11/XWayland remain
forbidden.

## 7. Vertical-slice prerequisites

| Slice item | Current evidence | Status |
|---|---|---|
| Room `r1_1` prefab/hierarchy | `analysis/re/room_topology.md` | CONFIRMED wall/door/aisle geometry and closed-door collision; runtime entrance/listener/timing PARTIAL |
| Player `c00`/`C01Controller` | serialized prefab + stats | CONFIRMED technical; display name CANDIDATE |
| Enemy `e_slime01`/`EnemyAI04` | serialized prefab + stats + contact path | CONFIRMED technical; full AI PARTIAL |
| Weapon `weapon_000`/`Gun001` | serialized fields + Attack/CreateBullet chain | CONFIRMED asset/path; cooldown PARTIAL |
| Projectile `bullet_0`/`Bullet01` | serialized fields + trigger chain + speed 36 override | CONFIRMED asset/speed; rebound/through PARTIAL |
| Input/movement | axes + `RGJoyControl` + `SetVelocity` | HIGH CONFIDENCE |
| Tick rate | `TimeManager` 0.02 s | CONFIRMED |
| Damage bridge | `RGController`/`RGEController`/`RGBox` | CONFIRMED |
| Damage formulas | player/enemy GetHurt/armor/HP | HIGH CONFIDENCE; crit/element PARTIAL |
| Death | player/enemy entry effects + reborn/restart entry points | PARTIAL |
| Renderer | none implemented | NOT STARTED (Phase 4) |

## 8. What is deliberately deferred

- Full-game content parity (all characters, weapons, enemies, bosses, rooms).
- Progression, shops, pets, mounts, multiplayer, achievements, ads/IAP/SDKs.
- Full save/load compatibility and migration.
- Pixel-perfect rendering/animation/shader parity.
- Deterministic lockstep claims.
- Android/emulator/Waydroid/Wine/X11/XWayland support.

## 9. Phase 3 conclusion

**PARTIAL.** The engine and vertical-slice behavioural contract is now concrete
enough to design Phase 4 and begin the non-gameplay bootstrap. Wall/door/aisle
geometry and closed-door traversal are now recovered; remaining labelled
approximations include all-open entrance state for the single-room test scene,
native AABB/circle and door-timing approximations, attack cooldown, full reborn
UI, and minimal `EnemyAI04` AI. Full Phase 3 parity remains incomplete; see
`docs/GATE_B.md`, `docs/VERTICAL_SLICE.md`, and `analysis/re/*.md`.
