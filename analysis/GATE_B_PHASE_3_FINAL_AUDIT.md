# Gate B / Phase 3 Final Audit

**Date:** 2026-10-31
**Previous baseline:** `8ba02d5` (clean tree)
**Scope:** final evidence sprint for Gate B and Phase 3. No native runtime,
Vulkan renderer, gameplay, or vertical-slice implementation was added.

Run `git log -1 --format='%h %s'` for the commit containing this report.

---

## 1. Gate B result

**PARTIAL.**

Resolved in this sprint:

- runtime wall-collision source,
- player spawn source,
- door open/close/clear entry points,
- `Gun001.Attack → CreateBullet → BulletFactory.TakeBullet`,
- effective projectile speed 36 vs prefab default 10,
- `EnemyAI04` contact behavior,
- death/reborn/game-over/restart entry flow,
- save boundary (Unity PlayerPrefs).

Remaining exact-detail blockers:

1. exact per-cell wall/door-gap placement and the full door/aisle state machine,
2. exact `RGWeapon` attack cooldown and the controller/hand field used in the
   spread formula,
3. exact post-death virtual slot owners (`+0x328`, `+0x3A8`, `+0x3E8`) and full
   reborn UI sequencing,
4. full `EnemyAI04`/other AI state machines, procedural generation, save payload,
   and content identities,
5. no approved runtime observation oracle.

These are explicitly scoped for the first slice: rectangular 15×15 room with
serialized door openings, fixed documented attack interval, death + restart
(no reborn-card flow), and minimal `EnemyAI04` contact behavior. They are not
reference parity.

Full matrix: `docs/GATE_B.md`.

## 2. Phase 3 result

**PARTIAL.**

The vertical-slice behavioural contract is now concrete, including the room
collision source, spawn, weapon/projectile path, damage pipeline, death/restart
entry, and minimal enemy. Full-game behaviour remains incomplete.
Report: `docs/PHASE_3_BEHAVIOURAL_RE.md`.

## 3. Systems reconstructed

### Room / scene / collision

- `r1_1` root: `RGRoomX` + `EnemyMaker`; 178 transforms, 109 sprites, 12 serialized
  box colliders, 4 doors, 4 door triggers, 8 platform effectors, 4 animators.
- UnityPy `parse_as_dict` values: `room_width=15`, `room_height=15`, `room_type=1`,
  `door_open=1`, offsets 0.
- `RGRoomX.CreateWall` (`0xDEBE3C`) instantiates `MapManagerLevel.wall_list`
  (`+0x100`) wall prefabs, parents them to `wall_group`, and assigns
  `RGRoomX.EdgeLayer` (17). `wall101`/`wall201` have 1×1 non-trigger
  `BoxCollider2D`.
- `CloseDoor`/`OpenDoor` (`0xDEA4AC`/`0xDEA53C`) call `RGDoor.Close/Open` on all
  four doors and set `door_open`.
- `ClearRoom` (`0xDEA5D0`) sets process 2 and sends the room-clear message;
  `IsClear` (`0xDEBDD8`) checks `EnemyMaker`.
- `RGGameSceneManager.CreatePlayer` (`0xDCD3C0`) uses `MapManager.centerPoint`
  (`+0x80`) for the player spawn.
- `RGRoomX.LoadRoom` (`0xDEA7CC`) finds `b_point`, iterates its children, and adds
  their room-relative positions to `floor_list`.

### Weapon / projectile

- `Gun001.Attack` (`0x1241260`) → `RGWeapon.get_bullet_speed` (returns 36) →
  `Gun001.CreateBullet` → `BulletFactory.TakeBullet`.
- `Gun001.CreateBullet` (`0x124131C`) uses `deviation=5`, computes
  `spread = deviation * (field+1)`, calls `RGRandom.Range(-spread,+spread)`, builds
  `BulletInfo`/`DamageInfo`, and invokes `BulletFactory.TakeBullet`.
- `weapon_000` has `consume=0`; no ammo cost for this weapon.
- `bullet_0` prefab speed 10 is overwritten by `BulletInfo.speed=36` in
  `RGBullet.UpdateInfo`; effective initial speed is 36.
- `bullet_0` has `destroy_time=5`, `can_rebound=1`, `rotate_angle=0`, camp 1.
- Fire cooldown and the exact controller/hand field in the spread formula remain
  PARTIAL.

### Damage / death

- Player target `RGController.GetHurt` vtable `+0x228`; enemy
  `RGEController.GetHurt` `+0x2C8`; box `RGBox.Hit` `+0x168`.
- `CanDie` resolves `Singleton<RGGameProcess>` and checks `this_index` (`+0x1C`)
  against `-1`/`-3`.
- Critical roll uses `Random.Range(0,100)` vs `DamageTrigger.critical`; observed
  player HP damage is not multiplied by the crit bool.
- `RGController.Dead`, `RGEController.Dead`, `RGController.Reborn`,
  `ForceReborn`, `DoReborn`, `UIWindowReborn.BtnYesClick`, `RGGameProcess.GameFail`,
  `TryAgain`, `NextScene`, and `ReSetInfo` entry flows are reconstructed.
- Exact virtual slot owners and full UI/reborn sequencing remain PARTIAL.

### Enemy

- `e_slime01`: `EnemyAI04` + `RoleAttribute` (hp 12, speed 3.0, damage 3,
  enemyTag 66, levelScene 20).
- `EnemyAI04.OnTriggerEnter2D` (`0x10E54D0`) tail-calls
  `HitUnit(other, damage=3, 1.5f)`.
- Base `RGEController.Scout`/`FindNearestTarget`/`FollowDefault`/`HitUnit` provide
  the minimum movement/target/contact path.
- Full AI state machine/animations remain PARTIAL.

### Save / RNG

- `PlayerSaveData.SetString/SetInt` tail-call Unity `PlayerPrefs`; `RGSaveManager`
  uses these. `AboGameData` payload/keys remain UNKNOWN; save compatibility is not
  required for the first slice.
- RGRandom core/map/LongMap paths are reconstructed; `seed_state` 1/2 and exact
  DateTime seed expression remain UNRESOLVED; reference vectors unavailable.

## 4. Evidence quality

| Class | Examples |
|---|---|
| CONFIRMED | TimeManager 0.02 s; runtime wall source; spawn centerPoint; Gun001 attack path; speed 36 override; AI04 contact; death/reborn entry calls; PlayerPrefs boundary |
| HIGH CONFIDENCE | exact room component counts/bounds; `CreateWall` perimeter logic; spread formula structure; `RGWeapon`/hand attack flow; base AI target/follow path |
| HYPOTHESIS | display names Knight/Rogue/slime/pistol; sentinel meaning; per-cell gap behaviour; fixed native attack interval |
| UNKNOWN | procedural generation, `AboGameData` payload, full AI, full reborn UI, status stacking, multiplayer |
| CONTRADICTED | Unity 2021.x, metadata v24.1, PCG32 parity, `RGGameInfo.map_random_seed` as `CanDie` field, `bullet_0` speed 10 as effective speed |

No candidate display name is promoted to confirmed.

## 5. Unresolved questions

1. Exact `RGRoomX.CreateWall` per-cell skip/door-gap conditions.
2. Full `RGDoor`/`RGDoorTrigger`/`RGAisle`/`StartRoom`/`ClearRoom` state machine.
3. `RGWeapon` attack cooldown formula and the `+0x2c` spread field identity.
4. Exact `RGBullet` initial velocity composition, rebound, and through-count
   integration.
5. Exact `RGEController.Dead` virtual slot owners `+0x328`/`+0x3A8`, player
   `+0x3E8` reborn slot, and UI sequencing.
6. Full `EnemyAI04` Scout branch/target selection/animations.
7. `MapManagerLevel.CreateMap`/room graph/template generation.
8. `AboGameData` save payload/key names/versioning.
9. Serialized display-name mapping for c00/c01/e_slime01/weapon_000.
10. `RGRandom.seed_state` 1/2 and exact DateTime-derived seed expression.
11. Per-subclass `RGBulletEffectTrigger` `isCritic`/`damageType` consumers.
12. Reference vectors/runtime observations.

## 6. Known static-analysis limitations

- No approved runtime oracle: no Android runtime, Waydroid, emulator, Wine, X11,
  or XWayland.
- Static analysis cannot observe live Animator transitions, Physics2D contact
  ordering, coroutine scheduling, actual PlayerPrefs values, or live RNG sequences.
- The in-repo `analysis/il2cpp_dump/` is partial; full Il2CppDumper output remains
  outside Git at `/home/jules/bin`.
- UnityPy can read serialized MonoBehaviour fields when typetree generation is
  available, but not every prefab field is exposed.
- Direct-call xrefs do not capture virtual/interface dispatch.

## 7. Vertical-slice prerequisites

**Not cleared for gameplay.** First-slice approximations are explicitly accepted:

- rectangular 15×15 room perimeter with serialized door openings,
- fixed documented attack interval for `weapon_000`,
- death + restart, no full reborn-card flow,
- minimal `EnemyAI04` contact behavior.

Still required before calling the slice playable: Phase 4 renderer/platform stages,
actual converted assets, and implementation tests. See `docs/GATE_B.md`,
`docs/VERTICAL_SLICE.md`, and `docs/PHASE_4_NATIVE_RUNTIME.md`.

## 8. Phase 4 runtime architecture

```text
Application
  ├── Platform (SDL3/Wayland, filesystem, timing)
  ├── Asset Runtime (decoded registry, conversion output)
  ├── Renderer (Vulkan device/swapchain/sprite pipeline)
  ├── Input / Audio
  └── Simulation (50 Hz fixed clock, RNG, ECS world)
        └── Gameplay (controllers, projectiles, damage, rooms)
```

Plan: `docs/PHASE_4_NATIVE_RUNTIME.md`. Ownership, init/shutdown, frame model,
Vulkan smoke test, asset conversion, gameplay architecture, stages, tests, and
performance budgets are specified there.

## 9. Phase 4 implementation sequence

1. Application bootstrap
2. Wayland/SDL3 window
3. Vulkan smoke renderer
4. Asset runtime foundation
5. Sprite renderer
6. Simulation clock
7. Input abstraction
8. Room loading
9. Collision representation
10. Player entity
11. Player movement
12. Weapon
13. Projectile
14. Enemy
15. Damage pipeline
16. Death/restart
17. HUD
18. Audio
19. Vertical-slice integration
20. Reference comparison/performance

## 10. Tests

Added `tests/unit/timing_test.cpp`:

- checks `FrameTimer::TARGET_DT == 0.02` (50 Hz),
- rejects the old 60 Hz value,
- checks the accumulator clamp is at least one fixed step.

The test count changed from **22/1273** to **23/1276**. No existing test was
removed or weakened. The change is the confirmed 50 Hz reference timing contract.

## 11. Verification

Clean builds after the source change:

- Debug configure/build: PASS, **0 warnings**
- Debug tests: **23 cases / 1276 assertions, PASS**
- Debug CTest: 1/1 passed
- intentional timing assertion break: CTest **FAIL**, exit 8
- restored file byte-identical; CTest passed again
- RelWithDebInfo configure/build: PASS, **0 warnings**
- RelWithDebInfo tests: **23 / 1276, PASS**
- RelWithDebInfo CTest: 1/1 passed
- `git diff --check`: clean
- `git status`: clean after commit
- no APK/proprietary assets/build output/secrets tracked

## 12. Final state

- Gate B: **PARTIAL** (engine/runtime primitives and vertical-slice source known;
  exact wall gaps, fire cadence, and reborn slots remain).
- Phase 3: **PARTIAL** (vertical-slice contract concrete; full-game breadth not).
- Phase 4: **PLANNED only / Gate C NOT MET**.
- Phase 5+: **BLOCKED**.
- No Phase 4 code was started.

## NEXT SESSION

Begin Phase 4 implementation with:

**Create the application bootstrap and a Wayland-only SDL3 window at 1920×1080; force `SDL_HINT_VIDEO_DRIVER=wayland`, fail startup on any other driver, pump resize/minimize/focus events, and shut down cleanly.**

No gameplay, Vulkan, renderer, or vertical-slice feature implementation beyond that task. The remaining Gate B exact details are documented approximations and must not be silently promoted to parity.
