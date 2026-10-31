# Soul Knight 2.6.8 — Project State

**Last Updated:** 2026-10-31 (Gate B / Phase 3 evidence pass)
**Commit:** session head; run `git log -1 --format='%h %s'`
**Phase:** Phase 2 **PARTIAL** (Gate B PARTIAL); Phase 3 **PARTIAL**; Phase 4 **PLANNED only**
**Current status:** Engine/runtime evidence is substantially reconstructed; the
vertical slice is **not** cleared for gameplay implementation because four Gate B
blockers remain.

---

## Current Phase and Gates

| Phase | Status |
|---|---|
| Phase 0 — Environment | ✅ COMPLETE |
| Phase 1 — APK Forensics | ✅ COMPLETE (artifact inventory) |
| Phase 2 — Engine Reconstruction | ⚠️ PARTIAL — Gate B **PARTIAL** |
| Phase 3 — Behavioural Reconstruction | ⚠️ PARTIAL — vertical-slice contract largely known; full-game content UNKNOWN |
| Phase 4 — Native Runtime | ⬜ PLANNED — Gate C **NOT MET** |
| Phase 5 — First Playable | ⛔ BLOCKED by Gate B/C |
| Phase 6+ — Systems/Content/Fidelity/Release | ⛔ BLOCKED |

Canonical baseline:

- Soul Knight 2.6.8 (`com.ChillyRoom.DungeonShooter`, versionCode 20680)
- Unity **2018.4.12f1**
- IL2CPP metadata **v24**
- APK SHA-256 `8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea`
- CachyOS x86-64
- Wayland only
- Vulkan target
- 1920×1080, 60 FPS target
- native executable
- no Android runtime, Waydroid, emulator, Wine, X11, or XWayland

Canonical reports:

- Gate B: `docs/GATE_B.md` — **PARTIAL**
- Phase 3: `docs/PHASE_3_BEHAVIOURAL_RE.md` — **PARTIAL**
- Phase 4 plan: `docs/PHASE_4_NATIVE_RUNTIME.md` — **PLAN ONLY**
- Final audit: `analysis/GATE_B_PHASE_3_FINAL_AUDIT.md`
- Evidence standard: `docs/EVIDENCE_STANDARD.md`

---

## Confirmed in this pass

### Timing

Serialized `TimeManager` in `globalgamemanagers`:

```text
Fixed_Timestep = 0.019999999552965164   // 0.02 s / 50 Hz
Maximum_Allowed_Timestep = 0.33333334
m_TimeScale = 1.0
```

No direct xref to `Time.set_fixedDeltaTime`; the build-time 50 Hz step is the
reference. Render target remains 60 FPS. See `analysis/re/timing.md`.

### Damage bridge

- Player: `RGBulletTrigger.OnTriggerEnter2D` → `GetComponent<RGController>`
  (`0x30E8F80`) → `RGController.GetHurt` vtable `+0x228` (slot 16,
  `0x11795CC`).
- Enemy: same method → `GetComponent<RGEController>` (`0x309A4E8`) →
  `RGEController.GetHurt` vtable `+0x2C8` (slot 26, `0x1108E60`).
- Box: `GetComponent<RGBox>` (`0x31003B0`) → `RGBox.Hit` vtable `+0x168`
  (`0x11675F8`).

`CanDie` resolves `Singleton<RGGameProcess>.get_Inst()` and checks
`RGGameProcess.this_index` (`+0x1C`) against `-1`/`-3`; the earlier
`RGGameInfo.map_random_seed` interpretation is wrong. See `analysis/re/damage.md`.

### RNG

- Core Unity 2018.4 RNG algorithm, wrapper, map seed, and LongMap branch are
  reconstructed. `MapManagerLevel.SetRGRandomSeed` adds a 50% `map_long` increment
  for Normal mode, `this_index` 1..15, LongMap active.
- `seed_state` 1/2 remains unused/UNRESOLVED; single-player paths use 0/3.
- Reference vectors are **NOT AVAILABLE**. See `analysis/re/rgrandom.md`.

### Assets / room / entities

- `r1_1`: 178 transforms, 109 sprites, 12 box colliders, 4 doors, 4 door triggers,
  8 platform effectors, 4 animators; `room_width=15`, `room_height=15`.
  Runtime wall collision comes from `RGRoomX.CreateWall` instantiating
  `MapManagerLevel.wall_list` 1×1 collider prefabs on layer `Edge` (17), not from
  the 12 serialized colliders.
- Player spawn is `MapManager.centerPoint`; `b_point` children populate
  `RGRoomX.floor_list`. `CloseDoor`/`OpenDoor` iterate all four doors;
  `ClearRoom`/`IsClear` use `EnemyMaker`.
- `c00` → `C01Controller` + `RoleAttributePlayer` (speed 6.5, hp 6, armor 5, atk 4,
  critical 5, c_index 0).
- `c01` → `C02Controller` (speed 6.5, hp 5, armor 3, atk 5, critical 10, c_index 1).
- `e_slime01` → `EnemyAI04` + `RoleAttribute` (hp 12, speed 3.0, damage 3,
  enemyTag 66, levelScene 20).
- `weapon_000` → `Gun001` (bullet `bullet_0`, speed 36, damage 3, critic 0,
  deviation 5, consume 0, weapon_type 14). Attack path:
  `Gun001.Attack` → `CreateBullet` → `BulletFactory.TakeBullet`.
- `bullet_0` → `Bullet01` (prefab speed 10, destroy_time 5, can_rebound 1,
  camp 1). `RGBullet.UpdateInfo` overwrites speed with the weapon's BulletInfo
  speed 36, so 36 is the effective initial speed; 10 is a prefab default.
- `e_slime01` contact damage: `EnemyAI04.OnTriggerEnter2D` →
  `HitUnit(other, 3, 1.5)`; see `analysis/re/enemy_ai04.md`.
- Save boundary: `PlayerSaveData` uses Unity `PlayerPrefs` string/int keys; exact
  `AboGameData` payload is UNKNOWN and not required for the slice. See
  `analysis/re/save.md`.

Display names (Knight/Rogue/slime/pistol) remain **CANDIDATE**, not proven. See
`analysis/re/rooms_scene.md` and `analysis/re/weapons_projectiles.md`.

### Input / movement

Legacy `Input` axes (WASD + joystick, deadzone 0.19), `MovementInput {dir, weight}`,
`C01Controller.FixedUpdate` → `RGController.SetVelocity` (vtable slot 21,
`0x117A6E4`), speed derived from `RoleAttributePlayer.speed`/`speed_rate`. See
`analysis/re/input_movement.md`.

---

## Gate B blockers (must close or explicitly accept)

1. Exact per-cell wall/door gap placement and the full door/aisle state machine
   remain PARTIAL. First-slice approximation: rectangular 15×15 perimeter with
   serialized door openings, explicitly labelled.
2. Exact `Gun001` fire cooldown and the controller/hand field used in the spread
   formula remain PARTIAL. First-slice approximation: fixed documented attack
   interval; do not claim reference fire rate.
3. Full death/reborn UI sequencing and virtual slot owners (`+0x328`, `+0x3A8`,
   `+0x3E8`) remain PARTIAL. First slice can implement death + restart; reborn-card
   flow is deferred.
4. Full `EnemyAI04`/other AI, all weapons, procedural generation, and content
   identities remain Phase 3 breadth. Minimal AI04 contact behavior is known.
5. Save payload/versioning remains UNKNOWN; not required for the slice.
6. No approved runtime oracle; static analysis only.

---

## Build and Test Verification

Recorded in `analysis/REPOSITORY_INTEGRITY_AUDIT.md` and the final audit. Current
baseline:

- Debug clean configure/build: PASS, 0 warnings
- Debug tests: 23 cases / 1276 assertions, PASS
- Negative control: CTest FAIL (exit 8), restored PASS
- RelWithDebInfo clean configure/build: PASS, 0 warnings
- RelWithDebInfo tests: 23 / 1276, PASS
- `git diff --check`: clean
- `git status`: clean after commit

The count changed from 22/1273 to 23/1276 because a `FrameTimer` regression test was added for the newly confirmed 50 Hz reference step. No existing test was removed or weakened.

---

## Next Action

Gate B remains **PARTIAL**, but the remaining approximations are explicitly scoped
for the first slice. The next implementation task is the Phase 4 application
bootstrap + Wayland-only SDL3 window; it does not depend on the unresolved exact
fire cadence, wall gaps, or reborn UI. Do not start gameplay/vertical-slice code
until the slice gates are accepted.
