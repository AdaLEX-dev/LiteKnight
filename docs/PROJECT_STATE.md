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
  8 platform effectors, 4 animators; serialized collider bounds approx.
  `x[-2.5,28.5]`, `y[-2,29]`.
- `c00` → `C01Controller` + `RoleAttributePlayer` (speed 6.5, hp 6, armor 5, atk 4,
  critical 5, c_index 0).
- `c01` → `C02Controller` (speed 6.5, hp 5, armor 3, atk 5, critical 10, c_index 1).
- `e_slime01` → `EnemyAI04` + `RoleAttribute` (hp 12, speed 3.0, damage 3,
  enemyTag 66, levelScene 20).
- `weapon_000` → `Gun001` (bullet `bullet_0`, speed 36, damage 3, critic 0,
  deviation 5, weapon_type 14).
- `bullet_0` → `Bullet01` (speed 10, destroy_time 5, can_rebound 1, camp 1).

Display names (Knight/Rogue/slime/pistol) remain **CANDIDATE**, not proven. See
`analysis/re/rooms_scene.md` and `analysis/re/weapons_projectiles.md`.

### Input / movement

Legacy `Input` axes (WASD + joystick, deadzone 0.19), `MovementInput {dir, weight}`,
`C01Controller.FixedUpdate` → `RGController.SetVelocity` (vtable slot 21,
`0x117A6E4`), speed derived from `RoleAttributePlayer.speed`/`speed_rate`. See
`analysis/re/input_movement.md`.

---

## Gate B blockers (must close or explicitly accept)

1. Wall/floor collision placement from the room grid; the room prefab's 12
   colliders are only doors/triggers/bullet colliders.
2. Player spawn point (`b_point`/`RGRoomX.entrance`) and door/aisle/room state
   transitions.
3. Exact `Gun001` fire cadence/spread/ammo and `RGBullet` velocity/rebound/through
   math.
4. Full death/reborn/game-over virtual branches.
5. Full AI, all weapons, procedural generation, save format, and content identities
   (Phase 3 breadth, not first-slice engine primitives).
6. No approved runtime oracle; static analysis only.

---

## Build and Test Verification

Recorded in `analysis/REPOSITORY_INTEGRITY_AUDIT.md` and the final audit. Current
baseline:

- Debug clean configure/build: PASS, 0 warnings
- Debug tests: 22 cases / 1273 assertions, PASS
- Negative control: CTest FAIL (exit 8), restored PASS
- RelWithDebInfo clean configure/build: PASS, 0 warnings
- RelWithDebInfo tests: 22 / 1273, PASS
- `git diff --check`: clean
- `git status`: clean after commit

The test count did not change. Any future change must be explained.

---

## Next Action

Do **not** implement Phase 4 until the Gate B blockers are resolved or explicitly
accepted for the slice. The next evidence task is to resolve wall-collider
placement and the `Gun001`/`RGBullet` fire/velocity contract. The Phase 4 plan
(`docs/PHASE_4_NATIVE_RUNTIME.md`) starts with the application bootstrap and
Wayland-only window when that gate is cleared.
