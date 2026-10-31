# Gate B / Phase 3 Final Audit

**Date:** 2026-10-31
**Previous baseline:** `48758ca` (clean tree)
**Scope:** finish Gate B evidence, Phase 3 behavioural RE, and Phase 4 planning.
No native runtime, gameplay, renderer, audio, or input implementation was added.

Run `git log -1 --format='%h %s'` for the commit containing this report.

---

## 1. Gate B result

**PARTIAL.**

The reusable engine/runtime primitives needed by the vertical slice are largely
reconstructed, but four blockers prevent a PASS:

1. wall/floor collision placement from the room grid,
2. player spawn selection and door/aisle/room state transitions,
3. exact `Gun001` fire cadence/spread/ammo and `RGBullet` velocity/rebound/through,
4. full death/reborn/game-over virtual branches.

Full evidence matrix: `docs/GATE_B.md`.

## 2. Phase 3 result

**PARTIAL.**

The vertical-slice behavioural contract is reconstructed:

- fixed 50 Hz timing,
- damage bridge and state mutation,
- input/movement,
- room prefab/component inventory,
- player/enemy/weapon/projectile technical identities,
- one weapon/projectile pair.

Full-game behaviour remains unknown: AI state machines, procedural generation,
all content, save format, full reborn flow, status stacking, multiplayer.
Report: `docs/PHASE_3_BEHAVIOURAL_RE.md`.

## 3. Systems reconstructed

### Engine/runtime

- Unity 2018.4 Xorshift128 RNG, LCG seeding, 23-bit floats, int/float range,
  inside-unit-circle; native implementation and tests exist.
- `RGRandom` layout/wrapper, lazy seeding, map seed, `MapManagerLevel` LongMap
  branch; `seed_state` 1/2 unused in observed single-player paths.
- Fixed timestep 0.02 s (50 Hz) from serialized `TimeManager`; render target 60 FPS.
- Legacy input axes, WASD/joystick mapping, `MovementInput`, `SetVelocity`.
- Physics2D gravity/layers/tags/collision settings.
- Asset/room/weapon/projectile metadata and manifest identity.

### Damage/death

- `RGBulletTrigger.OnTriggerEnter2D` → box `RGBox.Hit` (`+0x168`), player
  `RGController.GetHurt` (`+0x228`), enemy `RGEController.GetHurt` (`+0x2C8`).
- Armor overflow, HP clamp, one-shot shield, `CanDie` via
  `RGGameProcess.this_index` `-1`/`-3`, enemy HP/death callback.
- Player/enemy death entry effects (collider/weapon disable, flags, Animator).

### Room/assets

- `r1_1` 178 transforms, 12 box colliders, 4 doors, 4 door triggers, 8 platform
  effectors, 4 animators; bounds approx. `x[-2.5,28.5]`, `y[-2,29]`.
- `c00`/`C01Controller`, `c01`/`C02Controller`, `e_slime01`/`EnemyAI04`,
  `weapon_000`/`Gun001`, `bullet_0`/`Bullet01`.

## 4. Evidence quality

| Class | Examples |
|---|---|
| CONFIRMED | TimeManager 0.02 s; prefab stats; damage bridges/vtable slots; `CanDie` singleton; map seed path; LongMap condition |
| HIGH CONFIDENCE | movement formula; room component counts/bounds; `Gun001.Attack`→`CreateBullet`→`BulletFactory.TakeBullet`; crit roll |
| HYPOTHESIS | display names Knight/Rogue/slime/pistol; sentinel meaning; wall-collider placement; projectile effective velocity |
| UNKNOWN | Enemy AI, procedural generation, save format, full reborn, status stacking, multiplayer |
| CONTRADICTED | Unity 2021.x, metadata v24.1, PCG32 parity, `RGGameInfo.map_random_seed` as `CanDie` field |

All claims are linked to `analysis/re/*.md` or the phase reports. No candidate name
is promoted to a confirmed display identity.

## 5. Unresolved questions

1. How does `RGRoomX`/`MapManagerLevel` place wall/floor colliders from the room
   grid or `wall_list`/`floor_list`?
2. Which transform is the player spawn (`b_point` vs `RGRoomX.entrance`)?
3. What are the door/aisle `StartRoom`/`ClearRoom` transitions?
4. What is the exact `RGBullet` velocity/rebound/through math?
5. What are the exact `Gun001` fire cadence, spread, ammo, and consume rules?
6. What are the post-death virtual slots (`RGEController +0x328`, `+0x3A8`) and
   reborn/game-over/restart branches?
7. What is the full `EnemyAI04` state machine?
8. What is the save serialization format (`RGSaveManager.Save/Load`, `AboGameData`)?
9. Which serialized mapping proves display names for c00/c01/e_slime01/weapon_000?
10. What is the exact DateTime-derived seed expression in `SetUpRGRandom`?
11. Do `seed_state` 1/2 have any runtime use outside the observed single-player
    paths?
12. Which effect-trigger subclasses consume `isCritic`/`damageType`, and do any
    multiply HP damage?

## 6. Known static-analysis limitations

- No approved runtime oracle: no Android runtime, Waydroid, emulator, Wine, X11,
  or XWayland.
- Static analysis cannot observe actual serialized runtime values not exposed by
  the typetree, Animator transitions/events, Physics2D contact ordering, coroutine
  scheduling, or live RNG sequences.
- The in-repo `analysis/il2cpp_dump/` is partial; full Il2CppDumper output is
  outside Git at `/home/jules/bin`.
- `script.json`/metadata identify method slots and names, but not all runtime
  generic instantiations; `ScriptMetadataMethod` was required to resolve several.
- No reference vectors are available; RNG tests are implementation regression
  tests, not reference-proof.

## 7. Vertical-slice prerequisites

**Not cleared.** Required before gameplay implementation:

- resolve wall-collider placement and spawn/door state,
- resolve weapon fire/projectile velocity or explicitly accept approximations,
- complete death/restart and at least a documented `EnemyAI04` minimal behavior,
- keep display identities candidate,
- implement Phase 4 renderer/platform stages first.

See `docs/VERTICAL_SLICE.md` and `docs/GATE_B.md`.

## 8. Phase 4 runtime architecture

Canonical plan: `docs/PHASE_4_NATIVE_RUNTIME.md`.

```text
Application
  ├── Platform (SDL3/Wayland, filesystem, timing)
  ├── Asset Runtime (decoded registry, conversion output)
  ├── Renderer (Vulkan instance/device/swapchain/sprite pipeline)
  ├── Input / Audio
  └── Simulation (50 Hz fixed clock, RNG, ECS world)
        └── Gameplay (controllers, projectiles, damage, rooms)
```

Key decisions:

- renderer owns Vulkan; simulation never calls it,
- platform owns SDL/Wayland; game code never touches X11,
- assets are keyed by stable bundle/path-ID, never filename alone,
- 50 Hz fixed simulation, 60 FPS render/interpolation,
- first implementation is single-threaded; miniaudio owns its thread.

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

Each stage has goal/modules/tests/acceptance/dependencies/risks in
`docs/PHASE_4_NATIVE_RUNTIME.md`.

## 10. Test strategy

- Current suite: 22 doctest cases / 1273 assertions.
- Engine: RNG/math/timing.
- Platform: Wayland-only startup, Vulkan device/swapchain/present, headless path.
- Assets: manifest/PNG/sprite/room JSON/missing asset.
- Simulation: movement/collision/projectile/damage/death/restart.
- Integration: complete `r1_1` loop.
- Regression: reference values where known; no invented values.
- Negative control: CTest must fail on an intentional break.
- Full plan: `docs/TESTING.md`.

## 11. Performance strategy

- Target 60 FPS / 16.6667 ms, simulation 50 Hz.
- Budgets: sim 2 ms, render CPU 8 ms, assets 1 ms, audio 1 ms, UI 1 ms, other
  3.6 ms.
- Profile on target hardware; measure CPU/GPU/draw calls/allocations.
- Do not claim 60 FPS before the renderer and slice exist.
- Full plan: `docs/PERFORMANCE.md`.

## 12. Verification

Clean builds after the documentation/evidence pass:

- Debug configure/build: PASS, **0 warnings**
- Debug tests: **22 cases / 1273 assertions, PASS**
- Debug CTest: 1/1 passed
- RelWithDebInfo configure/build: PASS, **0 warnings**
- RelWithDebInfo tests: **22 / 1273, PASS**
- RelWithDebInfo CTest: 1/1 passed
- `git diff --check`: clean
- `git status`: clean after commit
- No APK/proprietary assets/build output/secrets tracked

## NEXT SESSION

Begin Phase 4 implementation with:

**Create the application bootstrap and a Wayland-only SDL3 window at 1920×1080; force `SDL_HINT_VIDEO_DRIVER=wayland`, fail startup on any other driver, pump resize/minimize/focus events, and shut down cleanly.**

No gameplay feature implementation beyond that task. Do not add Vulkan, gameplay,
renderer, or vertical-slice code in the same session.
