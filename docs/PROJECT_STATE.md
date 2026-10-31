# Soul Knight 2.6.8 — Project State

**Last Updated:** 2026-10-31 (player render sync, animated door lifecycle, obstacle replacement)
**Commit:** session head; run `git log -1 --format='%h %s'`
**Phase:** Phase 2 **PARTIAL** (Gate B PARTIAL); Phase 3 **PARTIAL**; Phase 4 **PARTIAL (foundation FUNCTIONAL)**
**Current status:** Phase 4A-4F foundation is implemented and manually verified:
Wayland-only SDL3 window, Vulkan 1.3 device/swapchain, 50 Hz/60 FPS loop, a
batched textured sprite renderer, a native `r1_1` reconstruction test scene,
real `c00` idle/run animation, keyboard movement, recovered `CreateWall`
perimeter/aisle/door geometry, fixed-step collision, animated one-way doors with
their recovered clips, `RGObjectSkin` obstacle replacement, and camera follow.
Combat, AI, audio, and UI are not implemented. The vertical slice is **not**
cleared.

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
- Room topology and traversal: `analysis/re/room_topology.md`
- Machine-readable wall map: `analysis/re/r1_1_wall_map.json`
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
  `MapManagerLevel.wall_list` 1×1 collider prefabs on runtime layer `Edge` (17),
  not from the 12 serialized colliders.
- `MapManagerLevel` places rooms on a 41-unit grid; `RGRoomX.OnSetRGRandomSeed`
  gives 15×15 rooms offsets `(13,13)`. `CreateWall` places horizontal wall rows at
  `y=12/28` and vertical columns at `x=12/28`, skipping the five cells `18..22`
  on every side. The resulting interior is `[12.5,13]..[27.5,28]`; the 44 room
  wall cells and four five-cell openings are recovered.
- Wall prefabs `wall601`/`wall602` (level 1-1 manager, bundle `level-objects`)
  have identical 1×1 non-trigger `BoxCollider2D` at offset `(0,0.5)`, tag
  `Wall`. The prefab is serialized on layer 13 (`Wall`); `RGRoomX.cctor` sets
  `EdgeLayer=17` and generated wall instances are assigned to layer 17 (`Edge`).
  Variant selection is random per cell; collision is identical.
- Four serialized doors have barriers `5×1` (N/S) or `1×5` (E/W), trigger
  volumes `5×1.5` / `1.5×5`, and exact world rectangles recovered in
  `analysis/re/r1_1_wall_map.json`. Door Animator clips set open doors to
  `isTrigger=true`; closed doors restore the full solid collider with a
  one-way inward `PlatformEffector2D` (blocks exit, permits aisle entry).
  `RGDoorTrigger` events fire only while `room.door_open` is true and carry the
  room name and north/south/west/east direction.
- Runtime entrance bits are assigned by `MapManagerLevel.CreateRoom` from
  `room_relation`: `[east,south,west,north]`. The serialized `r1_1` entrance
  `[0,0,0,0]` is the template default; actual map instances differ. The native
  test scene deliberately opens all four sides as a labeled test configuration.
- Player spawn is `MapManager.centerPoint`; `b_point` children populate
  `RGRoomX.floor_list`. `CloseDoor`/`OpenDoor` iterate all four doors;
  `ClearRoom`/`IsClear` use `EnemyMaker`.
- `c00` physical collision is a child `CircleCollider2D` radius 0.4 at
  `(0,0.5)` on layer `Body`; the root `BoxCollider2D` is a trigger interaction
  box. Movement uses `Rigidbody2D.velocity` and Unity Physics2D.
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
- **Unity-derived sprite/scene gate PASSED:** the native `r1_1` reconstruction
  test scene renders 109 room sprites plus `c00` body/shadow from the real floor,
  character, and object-common atlases with serialized Sprite rect/pivot/PPU=16,
  Unity sorting layer/order, `Camera2D`, and the fixed multi-texture sprite batch.
  Scene metadata: `analysis/assets/scenes/r1_1_visual.json`. This scene is a
  controlled reverse-engineering reconstruction, **not** a proven canonical
  gameplay level.
- **Unity idle/run-animation gate PASSED:** `c00` Animator controller
  `char_0_skin_0` provides `Base Layer.ide` (`char_1_skin_0_ide`) and `run`
  (`char_1_skin_0_run`), both 0.5 s at 8 Hz, looping, with four real frame
  sprites each from the character atlas. Native `AnimationClip`/
  `AnimationPlayer`/`LocomotionAnimator` select idle/run at fixed 50 Hz.
  Metadata: `analysis/assets/scenes/animations/c00_idle.json` and
  `analysis/assets/scenes/animations/c00_run.json`.
- **Keyboard locomotion gate PASSED:** SDL3 keyboard actions feed normalized
  `MovementInput`; `PlayerLocomotion` applies the recovered
  `speed=6.5`/`speed_rate=0.5` equation (`9.75` effective units/s) only in the
  fixed 0.02 s tick; swept AABB collision and `Camera2D` follow are verified.
  Evidence/design: `analysis/assets/locomotion.md`.
- **Room topology / door traversal gate PASSED (native reconstruction):**
  `RGRoomX.CreateWall` and `RGAisle.CreateAisleWall` are reconstructed; the
  native scene consumes 44 room wall cells, 96 corridor wall cells, four exact
  door barriers/triggers, and the closed-door one-way normals. Open doors are
  passable; closed doors block exit and permit aisle entry; door triggers emit
  direction/room payloads only while `door_open`. The 41×41 map-cell edge is an
  explicit native test boundary. Evidence: `analysis/re/room_topology.md`,
  `analysis/re/r1_1_wall_map.json`.
- **Player simulation/render synchronization gate PASSED:** the rendered c00
  body now follows `PlayerLocomotion` exactly through an explicit
  `PlayerRenderSync` binding; the shadow keeps its recovered `(0,-0.05)` offset.
  Animation and camera follow cannot change player world position. Runtime
  `--locomotion-debug` logs `sim`, `render`, `body`, `shadow` and `camera`; unit
  tests cover animation and camera/viewport changes.
- **Door animation gate PASSED (native reconstruction):** both recovered
  Animator controllers/clips (`door_open`, `door_close`, `door_close2`) are
  exported from the serialized StreamedClip/PPtr curves. `Door` exposes
  `OPEN`/`CLOSING`/`CLOSED`/`OPENING`; `RoomDoorController` drives rendering
  (leaf sprite `door02`/`door03`, moving `d` ornament, shadow visibility) and
  collision from the `m_IsTrigger` curve. **O** toggles OpenDoor/CloseDoor and
  **C** issues CloseDoor. Native collision uses the full barrier during the
  non-trigger interval; transient `m_Size` thinning is a documented
  approximation.
- **Obstacle replacement / wall visual correspondence PASSED:** r1_1's 49
  `RGObjectSkin` markers (all `obj_index=0`) are replaced by `wall603`, matching
  `RGRoomX.CreateRoomObstacle`; the placeholder gray `skin` grid is gone. The
  scene carries 140 wall AABBs with 420 wall sprites, and 49 obstacle AABBs with
  147 obstacle sprites. The test spawn moved to `(20,15)` because the c00
  prefab origin `(20,20)` is inside the recovered obstacle block.
- Sprite metadata/camera/animation evidence: `analysis/assets/sprites.md`,
  `analysis/assets/coordinate_system.md`, `analysis/assets/animation.md`.
- Manual smoke from `/tmp` (path-independent): PNG decoded, real scene rendered,
  0 validation messages, clean exit.
- Non-Wayland startup fails with exit 1 (`wayland not available`), no X11
  fallback.

Not implemented: full AnimatorController transitions/layers/blend trees, other
clips/states, the exact event-to-map-manager traversal listener, room
combat/enemy lifecycle, adjacent-room generation/coordinate updates, combat, AI,
general KTX2/atlas packing, the full asset registry/manifest loader, audio, UI,
content, or parity comparison. Native sorting numbers are mapped to recovered
TagManager layers; camera size, camera follow smoothing, the AABB-vs-circle
player collision approximation, linear replay of the recovered streamed keys,
transient door-collider size animation, and fixed-step rather than Normal-update
sampling remain **PROJECT DECISIONS/APPROXIMATIONS** because exact reference
behavior is not recovered.

Evidence and limitations: `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`.

---

## Gate B blockers (must close or explicitly accept)

1. Wall/door gap placement, door collider geometry, door open/closed collision
   states, the recovered door animation clips, and `RGObjectSkin` obstacle
   replacement are now CONFIRMED and implemented. Remaining PARTIAL items:
   the exact entrance bits of any generated `r1_1` instance and the event-to-
   `MapManagerLevel` traversal listener.
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
- Debug tests: 55 cases / 1838 assertions, PASS
- Negative control: CTest FAIL (exit 8), restored PASS
- RelWithDebInfo clean configure/build: PASS, 0 warnings
- RelWithDebInfo tests: 55 / 1838, PASS
- `git diff --check`: clean
- `git status`: clean after commit

The count changed from 52/1676 to 55/1838 in the room-topology pass because new
deterministic tests cover the recovered 44 room wall cells, 96 corridor wall
cells, four exact door/trigger rectangles, one-way closed doors, open-door
passage, wall sliding, and synthetic loader parsing. No existing test was removed
or weakened. The real scene conversion/smoke remains manual because it requires
the local reference bundles and a Wayland/Vulkan GPU.

---

## Next Action

Phase 4 foundation, PNG, controlled Unity Texture2D, SpriteData, camera, the
`r1_1` native reconstruction test scene, real idle/run animation, keyboard
locomotion, recovered room perimeter/aisle/door geometry, and native door
traversal are **FUNCTIONAL**. The next bounded investigation is the higher-level
room transition path: resolve the listener(s) for `EnterDungeonRoomEvent`/
`ExitDungeonRoomEvent`, prove how `MapManagerLevel.IntoAisle` and room-coordinate
updates are invoked, and decide whether one adjacent room can be reconstructed
without starting procedural generation. Do not start combat, AI, menus, or the
full catalogue.
