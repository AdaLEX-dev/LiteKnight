# Soul Knight 2.6.8 — Project State

**Last Updated:** 2026-10-31 (Phase 4 PNG asset runtime)
**Commit:** session head; run `git log -1 --format='%h %s'`
**Phase:** Phase 2 **PARTIAL** (Gate B PARTIAL); Phase 3 **PARTIAL**; Phase 4 **PARTIAL (foundation FUNCTIONAL)**
**Current status:** Phase 4A-4E foundation is implemented and manually verified:
Wayland-only SDL3 window, Vulkan 1.3 device/swapchain, 50 Hz/60 FPS loop, and a
batched textured-quad renderer. Input/gameplay, asset decoding, audio, and UI are
not implemented. The vertical slice is **not** cleared.

---

## Current Phase and Gates

| Phase | Status |
|---|---|
| Phase 0 — Environment | ✅ COMPLETE |
| Phase 1 — APK Forensics | ✅ COMPLETE (artifact inventory) |
| Phase 2 — Engine Reconstruction | ⚠️ PARTIAL — Gate B **PARTIAL** |
| Phase 3 — Behavioural Reconstruction | ⚠️ PARTIAL — vertical-slice contract largely known; full-game content UNKNOWN |
| Phase 4 — Native Runtime | ⚠️ PARTIAL — foundation FUNCTIONAL; Gate C **PARTIAL** |
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
- Phase 4 plan/state: `docs/PHASE_4_NATIVE_RUNTIME.md` — **PARTIAL**
- Phase 4 runtime audit: `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`
- Asset pipeline: `docs/ASSET_PIPELINE.md` — **PNG path FUNCTIONAL; conversion/registry PARTIAL**
- Gate B/Phase 3 final audit: `analysis/GATE_B_PHASE_3_FINAL_AUDIT.md`
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

## Phase 4 runtime foundation (new)

Implemented and manually verified on 2026-10-31:

- `Application` lifecycle with explicit ownership and idempotent shutdown.
- SDL3 video/events, `SDL_HINT_VIDEO_DRIVER=wayland`; post-init driver check
  rejects every non-Wayland backend.
- Window state for resize, pixel-size changes, minimize, restore, focus, and
  close; event handling reconciled against SDL window flags.
- Vulkan instance API 1.3, `VK_KHR_surface` + `VK_KHR_wayland_surface`,
  Debug validation layers, physical-device selection, graphics/present queues,
  `B8G8R8A8_SRGB` FIFO 3-image swapchain, render pass, 2 frames in flight.
- GLSL 450 sprite shaders compiled to SPIR-V by a CMake `glslc` target.
- Batched textured quads with UVs, tint, alpha blending, stable layer sorting,
  synthetic PNG fixtures, staging upload, aspect-fit resize.
- 50 Hz fixed simulation consumed from the existing `FrameTimer`; rendering is
  decoupled and targets 60 FPS.
- PNG asset runtime: libpng 1.6 system dependency, canonical RGBA8
  `TextureData`, exact-byte/palette/gray/RGB tests, malformed-input rejection,
  executable-relative synthetic fixtures, decoded PNG rendered through the
  existing staging/`Texture2D`/`SpriteBatch` path.
- **Controlled Unity Texture2D gate PASSED:** one real `level-objects`/`f903`
  Texture2D (`RGBA32`, 16x24, path ID `-8989119030688471403`) converted through
  UnityPy 1.25.4 + Pillow 13.0.0 to canonical RGBA8 and a deterministic PNG;
  provenance recorded in `analysis/assets/texture_conversion_manifest.json`.
  Conversion was deterministic across two runs, native `PngDecoder` round-trip
  matched all 1536 bytes, and the real PNG rendered through the runtime.
- Manual smoke from `/tmp` (path-independent): PNG decoded, 0 validation
  messages, clean exit.
- Non-Wayland startup fails with exit 1 (`wayland not available`), no X11
  fallback.

Not implemented: KTX2/atlas packing, sprite metadata/pivot/PPU, the full asset
registry/manifest loader, input-to-simulation bridge, audio, UI, rooms, gameplay,
content, parity comparison. Native sorting numbers are a **PROJECT DECISION**,
not recovered Unity values.

Evidence and limitations: `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`.

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
- Debug tests: 35 cases / 1369 assertions, PASS
- Negative control: CTest FAIL (exit 8), restored PASS
- RelWithDebInfo clean configure/build: PASS, 0 warnings
- RelWithDebInfo tests: 35 / 1369, PASS
- `git diff --check`: clean
- `git status`: clean after commit

The count changed from 31/1335 to 35/1369 because the PNG asset-runtime stage
added exact RGBA normalization, palette/gray, malformed-input, and missing-file
tests. No existing test was removed or weakened. The Phase 4 integration smoke
test is manual because it requires a real Wayland compositor and GPU.

---

## Next Action

Phase 4 foundation, the PNG path, and the controlled Unity Texture2D gate are
**FUNCTIONAL**. Evidence now points to **Unity Sprite metadata** as the next
dependency: `TextureData` intentionally contains no rect/pivot/PPU semantics.
The next task is to extract one manifest-backed `Sprite` referencing a known
texture and add a separate `SpriteData` model (rect, pivot, pixels-per-unit,
filter/wrap, texture reference) with deterministic tests. Do not start gameplay
or the full catalogue.
