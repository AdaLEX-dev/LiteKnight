# Phase 4 — Native Runtime Foundation Audit

**Date:** 2026-10-31
**Baseline commit:** `94df258ac0807090cb94922a189b18a61093383e`
**Scope:** Phase 4A-4E foundation plus the controlled Unity-derived visual
slice (real `r1_1` room sprites, `c00` body/shadow, and real idle/run
AnimationClip). No gameplay, combat, AI, weapons, projectiles, damage, HUD, save,
procedural generation, or full content catalogue was implemented.

---

## 1. Status

**Phase 4 foundation: FUNCTIONAL / MANUALLY VERIFIED on this machine.**
**Gate C: PARTIAL.** Window, Vulkan device/swapchain, clear, textured-quad
presentation, resize, focus/minimize event handling, and clean shutdown are
verified. Input-to-simulation integration and gameplay are not implemented.

This is not a native-runtime-complete claim and not a rendering-parity claim.

| Stage | Status | Evidence |
|---|---|---|
| 4A application bootstrap | IMPLEMENTED / MANUAL PASS | `src/app/Application.*`, smoke log |
| 4B Wayland-only smoke | PASS | SDL driver `wayland`; non-Wayland startup exits 1 |
| 4C platform tests | PASS | `tests/unit/platform_test.cpp`, `render_test.cpp` |
| 4D Vulkan instance/device/swapchain | PASS | 10-second smoke run, validation-clean |
| 4E sprite renderer | PARTIAL / VISUAL PASS | decoded synthetic PNG, batched textured quad |
| PNG asset runtime | FUNCTIONAL | libpng → RGBA8 `TextureData` → existing staging/upload path |
| Asset-runtime PNG | FUNCTIONAL | libpng, exact-byte tests, synthetic PNGs |
| Controlled Unity Texture2D | FUNCTIONAL / MANUAL PASS | `level-objects`/`f903`; deterministic conversion + native round-trip + render |
| Unity Sprite metadata / camera | FUNCTIONAL | `SpriteData`, `Camera2D`, `analysis/assets/sprites.md`, `coordinate_system.md` |
| r1_1 + c00 visual composition | FUNCTIONAL / MANUAL PASS | 531 instances, 15 sprite assets, 3 textures, native SceneLoader, Wayland/Vulkan render |
| Unity idle/run animation | FUNCTIONAL / MANUAL PASS | `AnimationClip`/`AnimationPlayer`/`LocomotionAnimator`, real 4-frame clips, fixed-step loops |
| Keyboard locomotion / collision / camera | FUNCTIONAL / MANUAL PASS | `MovementInput`, `PlayerLocomotion`, recovered wall/door AABBs, one-way closed doors, `Camera2D` follow |
| Room topology / door traversal | FUNCTIONAL / MANUAL PASS | `analysis/re/room_topology.md`, `r1_1_wall_map.json`, SceneLoader, AabbCollision |
| Full Animator / registry / KTX2 | NOT STARTED | transitions and full registry remain |
| Input → fixed tick | FUNCTIONAL / MANUAL PASS | keyboard `InputManager` → normalized `MovementInput` → 50 Hz `PlayerLocomotion`; `--locomotion-debug` evidence |
| Audio/UI/gameplay | NOT STARTED | deliberate scope boundary |

---

## 2. Environment

- CachyOS, `Linux 7.3.0-4-cachyos`, x86-64.
- Wayland session, `XDG_SESSION_TYPE=wayland`.
- SDL **3.4.18**.
- Vulkan loader **1.4.363**; instance requests Vulkan **1.3**.
- GPU: Intel UHD Graphics 620 (WHL GT2), integrated, vendor `0x8086`,
  device `0x3ea0`, Mesa driver 26.2.5, API 1.4.354.
- Display: physical panel 1920x1080, KWin logical geometry 1536x864 at scale
  1.25. The application requests a 1920x1080 window; the compositor constrains
  the logical window to 1536x790. The swapchain correctly follows the compositor's
  current extent.

---

## 3. Implemented runtime

### 3.1 Application/platform

- `AppConfig` defaults: 1920x1080, Wayland-only, resizable, high-DPI,
  50 Hz simulation, 60 FPS render target.
- `Application` owns SDL, window, Vulkan renderer, render-world batch, frame
  timer, shutdown ordering, smoke-test duration, and measured frame statistics.
- SDL initializes video + events only (no gamepad/audio in this stage).
- `SDL_HINT_VIDEO_DRIVER=wayland` is set before `SDL_Init`.
- Post-init contract verifies `SDL_GetCurrentVideoDriver() == "wayland"`; any
  other driver logs a fatal error and exits non-zero.
- `SDL_Vulkan_LoadLibrary` / `SDL_Vulkan_CreateSurface` provide the Wayland
  surface; no X11/XWayland surface path exists.
- `WindowState` isolates resize/minimize/restore/focus/close transitions for unit
  testing. SDL event handling is reconciled once per frame against
  `SDL_GetWindowFlags` for compositor/SDL variants that omit an event.
- Shutdown waits for the GPU, destroys renderer resources, Vulkan objects,
  surface, SDL window, Vulkan library, and SDL; repeated `Shutdown()` is safe.

### 3.2 Vulkan

- Instance API 1.3; extensions include `VK_KHR_surface`,
  `VK_KHR_wayland_surface`, and Debug `VK_EXT_debug_utils`.
- Debug builds request `VK_LAYER_KHRONOS_validation` when available.
- Physical-device enumeration logs name, type, vendor/device IDs, API/driver
  versions, queue-family flags, present support, and selected features.
- Selection prefers discrete, then integrated, then virtual/CPU; requires Vulkan
  1.3, graphics + present queues, `VK_KHR_swapchain`, and non-empty
  format/present-mode lists.
- Logical device supports separate graphics/present families; this machine uses
  family 0 for both.
- Swapchain: `B8G8R8A8_SRGB`, FIFO, 3 images, current compositor extent, OPAQUE
  composite alpha, color attachment only.
- Framebuffers, render pass with `PRESENT_SRC_KHR` final layout, 2 frames in
  flight, per-frame command pools/buffers, per-frame image-available semaphores
  and fences, per-swapchain-image render-finished semaphores.
- Resize/out-of-date/suboptimal triggers `vkDeviceWaitIdle` + swapchain and
  framebuffer recreation. Minimized/zero-extent windows skip rendering.
- Destroy order: idle → sync → command pools → framebuffers/image views/swapchain
  → render pass → device → surface → debug messenger → instance.

### 3.3 Rendering

- GLSL 450 sprite vertex/fragment shaders compiled to SPIR-V by `glslc` as a CMake
  target; no generated binaries are committed.
- Vertex format: position, UV, RGBA tint. Indexed quads, dynamic host-visible
  vertex buffers per frame in flight, shared static index buffer.
- One draw call per `SpriteBatch` texture batch; stable sort by the project-native
  layer enum `BackGround` → `UI`.
- 1920x1080 logical projection with aspect-fit viewport/scissor on resize.
- Alpha blending (`SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA`) and no depth buffer.
- Texture path: libpng-decoded synthetic PNG → canonical RGBA8 `TextureData` →
  existing staging buffer → `R8G8B8A8_SRGB` image → sampler/view → descriptor →
  sprite batch. The test texture is not a proprietary APK asset.
- Screenshot inspection confirmed a textured quad with correct checker/UV
  orientation and transparent border over the clear color.

---

## 4. Verification

### 4.1 Unit/CTest

- Debug tests (current room-topology pass): **55 cases / 1838 assertions, all
  passing**, including ASan/UBSan.
- CTest: `soul-knight-unit-tests` 1/1 passing.
- Earlier sections 4.5/4.7 record the measurements at the animation/locomotion
  stages; section 4.8 supersedes their room-collision and count claims.
- New tests:
  - config defaults + Wayland-only rejection,
  - bootstrap CLI parsing,
  - window-state transition coverage,
  - safe repeated shutdown before initialization,
  - sprite-batch layer sorting/stability and quad budget,
  - `FrameTimer` consumed-step statistics,
  - PNG exact RGBA bytes/alpha, RGB/gray/gray-alpha/palette normalization,
    repeated-decode determinism, malformed and missing-file failure,
  - SpriteData rect/UV/pivot/PPU/border validation,
  - sprite geometry pivot/crop/rotation/flips/determinism,
  - Camera2D world/screen/aspect/zoom/projection,
  - native SceneLoader synthetic scene + missing-scene failure,
  - native AnimationClip parsing, discrete track sampling, duplicate keys,
  - scene animation references and clear unknown-sprite failure,
  - AnimationPlayer loop/non-loop/reset behavior,
  - synthetic animated SceneLoader fixture,
  - real `c00` idle and run metadata integration against the committed scene metadata,
  - keyboard cardinal/diagonal/zero input and frame/event API,
  - fixed-step player movement, AABB boundary/wall/sliding/no-penetration,
  - idle/run state switching and deterministic camera follow.

### 4.2 Wayland/Vulkan manual smoke

Command:

```bash
./build/soul-knight --smoke-seconds 10
```

Result: exit **0**.

- SDL video driver: `wayland`; Wayland-only mode enabled.
- Synthetic PNG decoded: `test/phase4_smoke.png`, 64x64, 16384 RGBA bytes.
- Window created at requested 1920x1080; compositor constrained it to
  1536x790 logical/pixels on this 1.25-scale display.
- Initial swapchain 1920x1080, then recreated to 1536x790 after the compositor
  geometry event.
- Validation: **0 validation messages/errors** in Debug.
- Rendered **603 frames**; advanced **506 fixed simulation ticks** in ~10.1 s.
- Measured average `FrameTimer` frame: **16.665 ms** over 573 samples;
  **0.834 simulation steps/frame**. This is a CPU frame-pacing measurement, not a
  GPU-timestamped 60 FPS guarantee.
- Clean shutdown logged; no LeakSanitizer report with compositor decorations
  (the earlier libdecor/GLib 90-byte external leak disappeared after selecting
  compositor decorations rather than the libdecor GTK/Cairo plugin).
- RelWithDebInfo smoke also exits 0 and presents normally; validation is
  intentionally disabled there (`Vulkan validation: false`) and Debug remains
  the validation gate.

Resize exercise:

```bash
./build/soul-knight --exercise-window --smoke-seconds 6
```

- Resize to 1600x900 observed; swapchain recreated (serial 3).
- Minimize observed; renderer skipped frames while minimized.
- Focus lost observed. Focus gained/lost and restore transitions are unit-tested.
- Programmatic `SDL_RestoreWindow` did not produce a compositor RESTORED event on
  this xdg-shell/KWin session; manual restoration was therefore not observed.
  This is a compositor/XDG limitation, not a hidden success claim.

Non-Wayland rejection:

```bash
DISPLAY=:0 WAYLAND_DISPLAY=invalid_wayland ./build/soul-knight --smoke-seconds 1
```

- Exit **1** with `SDL_Init failed: wayland not available`. No X11 fallback.

### 4.3 Screenshot

A desktop screenshot captured while the runtime was presenting is at
a temporary local screenshot (not committed). It shows the
1920x1080-target window, dark clear color, and the decoded synthetic asymmetric
PNG with distinct quadrants, white/black orientation markers, and alpha border.
The app was launched from `/tmp` using the executable-relative asset search path.
Generated images/logs are intentionally not tracked.

### 4.4 Real Unity Texture2D conversion

Selected source (CONFIRMED from UnityPy 1.25.4):

| Field | Value |
|---|---|
| APK SHA-256 | `8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea` |
| Bundle | `level-objects` |
| Bundle SHA-256 | `0ceed5578212398f4019943c759c3e67fb4338351732d09a2effa45dc5bf8829` |
| Texture2D name | `f903` |
| Path ID | `-8989119030688471403` |
| Container | `assets/rgprefab/mapmanager/level1/map_c3.prefab` |
| CAB | `CAB-a30724b9177a42016247cd793468ac48` |
| Format | `RGBA32`, 16x24, 1 mip |
| RGBA8 SHA-256 | `3b566df519684d6b4d7c9ca5daa6608ab72f9faf073fd2bb713fbae648662acd` |
| PNG SHA-256 | `a71dae026afe598a451723a466ccd1eec684405355a157fe9a306e91004f5505` |

Conversion ran twice with identical PNG, raw RGBA8, and manifest hashes.
`soul-knight-png-roundtrip` decoded the generated PNG through the production
`PngDecoder` and matched the converter's 1536 RGBA8 bytes exactly. The PNG was
then copied only into the gitignored build asset root and rendered through the
normal `--texture` path. A temporary local screenshot (not committed) confirmed the render. `analysis/assets/texture_conversion_manifest.json` is the machine-readable provenance record.

---

### 4.5 Native r1_1 reconstruction test scene + c00 composition

The controlled scene builder (`tools/asset/build_visual_scene.py`) uses the
serialized `r1_1` hierarchy and `common/c00` prefab:

- 109 room SpriteRenderers from `r1_1`, all sharing the 256×256 floor atlas
  (`5467841708608453938`).
- `c00/img/body` Sprite `c01_5` from the 2048×1024 character atlas
  (`6092784074628815488`).
- `c00/shadow` Sprite `shadow3` from the 32×32 object-common atlas
  (`3188784419113904678`).
- 111 instances, 13 sprite assets, 3 textures.
- 5 contiguous texture draw groups; 111 quads in one batched frame.
- Camera center `(20,20)`, orthographic half-height `10.5` (camera size is a
  project decision; reference value not recovered).
- Scene metadata SHA-256:
  `b2ad4f9ae68f676d7058c1c9d7318ace038ea52860bed8b322e4701f44a3ecc9`.

The run from `/tmp` with `--scene scenes/r1_1_visual/scene.json` rendered the
7×7 floor block, four door clusters, and the `c00` player/shadows successfully.
The final RelWithDebInfo animated-scene run measured **16.693 ms** average
`FrameTimer` frame, **603 frames / 508 fixed ticks over 10.16 s**, with 111
instances, 3 textures, 13 sprite assets, 2 animation clips, 8 keyframes, and 5 draw
groups (CPU pacing, not a parity/FPS guarantee). Debug validation: 0 errors.

Two real defects found during this milestone were fixed rather than worked
around:

1. `Mat4::operator*` treated column-major matrices as row-major, dropping camera
   translation. Fixed and covered by a projection-center test.
2. Indexed multi-texture groups applied both global index bases and a vertex
   offset, double-offsetting all groups after the first. Fixed by using
   `vertexOffset=0` with the global index buffer.

### 4.6 Real c00 idle/run animation

The selected animation path is `common/c00` Animator → controller
`char_0_skin_0` → default state `Base Layer.ide` → clip
`char_1_skin_0_ide` (path ID `-6496688600943111911`) and `run`
`char_1_skin_0_run` (path ID `3681644519574295089`). Both serialized clips are
0.5 s at 8 Hz, loop, and bind `SpriteRenderer.m_Sprite` at path `img/body`.
The source clip's high-level `m_PPtrCurves` list is empty, but its
`pptrCurveMapping` contains four real sprites and the streamed curve supplies the
mapping indices 0..3 at 0/.125/.25/.375 s. Details and classification:
`analysis/assets/animation.md`.

The native animation model keeps clip data separate from `SpriteData` and
`TextureData`:

- `AnimationClip` / `SpriteTrack` / `SpriteKeyframe` metadata,
- `AnimationPlayer` playback time/loop count,
- fixed-step `AnimationPlayer::Update` in `Application`,
- current key resolves to a `SpriteData` each rendered frame; size and pivot are
  recomputed from that sprite so the feet stay anchored,
- the room and shadow remain static.

Animation metadata SHA-256 (idle):
`23f34e0a6e3b5eb7d11f2e7dcbb11d89c25a9f2b8a17360ff70c280e846f7d44`;
run: `5a464af90c3e61c4b5e3ac6340583234bf6cae114d0a0f281980f2c80e5a3693`.
The actual runtime idle frame sequence (`c01_4`, `c01_5`, `c01_6`, `c01_7`, loop)
was observed in the native window; temporary screenshots show the changed
scarf/eye pixels and the room composition. The run clip uses `c01_0`..`c01_3` and
is selected when movement input is nonzero. The serialized Animator update mode is
Normal; fixed-step evaluation is a deterministic native decision, not proven
parity.

### 4.7 Native locomotion slice

The first playable movement slice uses the real `r1_1` reconstruction scene and
the recovered c00 movement values:

- keyboard WASD/arrows -> normalized `MovementInput`;
- `PlayerLocomotion` runs only in the 50 Hz fixed tick;
- reference formula `speed * (speed_rate + 1)` gives effective `9.75` units/s
  from c00 `speed=6.5`, `speed_rate=0.5`;
- swept AABB collision uses a native 13x13 interior approximation
  (`[13.5,13.5]..[26.5,26.5]`) derived from the recovered 15x15 room dimensions
  and a 1-unit wall ring;
- idle/run selection is based on movement presence;
- `Camera2D` follows the player with native smoothing `8.0`.

The exact `RGRoomX.CreateWall` gap placement, door/aisle traversal, and reference
camera-follow constants remain open. The native collision footprint is a 0.5x0.5
feet AABB; the serialized c00 collider is a trigger/hurtbox. The `r1_1_visual`
scene remains a reverse-engineering test reconstruction, not a proven canonical
gameplay level. Details: `analysis/assets/locomotion.md`.

Manual verification used real SDL keyboard events via `/dev/uinput` against the
focused Wayland window. Logs showed idle -> run on key press, movement to the
native boundary, blocked collision, wall sliding, release -> idle, and camera
follow. `--locomotion-debug` exposes position/input/velocity/state/frame/collision.

### 4.8 Recovered room topology and door traversal (2026-10-31, later pass)

The room collision approximation in 4.7 has been replaced as far as the evidence
allows:

- `RGRoomX.CreateWall` gap placement fully decoded: rows `y=12/28`, columns
  `x=12/28`, skip five cells `18..22`; runtime offsets `(13,13)` from the 41-unit
  map cell.
- Wall prefab `wall601`/`wall602`: 1×1 non-trigger `BoxCollider2D`, offset
  `(0,0.5)`, tag `Wall`. The prefab is serialized on layer 13 `Wall`, but
  `RGRoomX.cctor` sets `EdgeLayer=17` and generated instances are assigned to
  layer 17 `Edge`.
- 44 room wall cells and 96 aisle corridor wall cells represented explicitly.
- Four serialized door barriers/triggers and directions recovered; open doors
  set `isTrigger=true`; closed doors are full solid barriers with a one-way
  inward `PlatformEffector2D`.
- `RGDoorTrigger`/`RGDoor` events carry room name + direction, gated by
  `door_open`; exact higher-level listener remains UNKNOWN.
- Native `AabbCollisionWorld` gained one-way solids; tests cover all four doors in
  both directions and wall sliding.
- `r1_1_visual.json` now has 531 instances / 15 sprite assets; the initial
  all-open state draws 471. Scene SHA-256:
  `e7721abf2d90121cb0165c6f0cd1363b8f723c7574d3b09ca9a5780033b1e1d0`;
  wall-map SHA-256:
  `935a03bebd7d41c286cd978ce787f03f388cd65efdadc30474dd4e4937f1cd26`.
- Tests: **55 cases / 1838 assertions**.
- Runtime smoke: `recovered solids=140 doors=4 topology=true`, room interior
  `[12.5,13]..[27.5,28]`, door state toggle via **O**, closed-door blocking and
  open-door/east/south trigger traversal observed manually.

Evidence: `analysis/re/room_topology.md`,
`analysis/re/r1_1_wall_map.json`, `analysis/assets/locomotion.md`.

## 5. Remaining blockers / gaps

1. **Full Animator / room transition / KTX2 / asset registry** — idle/run and
   recovered `CreateWall`/door/aisle collision are implemented; transitions,
   other clips, door animation timing, the event-to-map-manager traversal
   listener, general atlas packing, and the full registry remain. The source
   `m_PPtrCurves` encoding is documented as a HIGH CONFIDENCE mapping +
   streamed-curve reconstruction, not full Unity Animator compatibility.
2. **Shader/install path** — run-time shader and test-asset lookup now goes through
   `FileSystem`'s executable-relative/user/system search paths and CMake copies
   assets into `build/assets`; a signed/installed package still needs to install
   `share/soul-knight/assets/{test,shaders}`.
3. **Input scope** — keyboard locomotion is wired through the fixed tick;
   gamepad/joystick axes, attack/skill/dodge, and UI input are not consumed yet.
4. **Real 1920x1080 surface on this display** — the compositor's 1.25 scale and
   1536x864 logical desktop constrain the window. The code requests 1920x1080 and
   handles the actual surface extent correctly.
5. **Restore event observation** — handler is implemented and unit-tested; the
   programmatic xdg-shell restore path was not delivered by this compositor.
6. **Rendering/camera parity** — no reference comparison; native layer mapping is
   recovered from TagManager, but camera size, follow smoothing, and collision
   approximations remain native decisions.
7. **Tight polygon meshes/borders** — rectangular tight-crop quads are used; full
   Sprite `m_VertexData` meshes and nine-slice borders are not implemented.
8. **Audio/UI/gameplay** — deliberately out of scope for this session.
9. **Performance guarantee** — FIFO pacing measured on this machine only; no GPU
   timestamps, no multi-GPU/display matrix, no 60 FPS guarantee.

---

## 6. Exact next task

**Complete room geometry and traversal:** reconstruct the actual
`RGRoomX.CreateWall`/door/aisle collision geometry and make the player traverse it
with the existing fixed-step movement, idle/run animation, and camera follow. Do
not import the full atlas/catalogue, commit proprietary bytes, or begin combat/AI.
