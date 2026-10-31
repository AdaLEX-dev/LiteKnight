# Phase 4 — Native Runtime Implementation Plan

**Status:** **PARTIAL — native foundation implemented and manually verified.**
The application bootstrap, Wayland-only SDL3 window, Vulkan 1.3 context/swapchain,
50 Hz fixed loop, batched textured-quad renderer, and PNG → RGBA8 → GPU texture
path are implemented. Input-to-simulation wiring, audio, gameplay, KTX2/atlas
packing, and the full asset registry are not.
**Reference:** Soul Knight 2.6.8 / Unity 2018.4.12f1 / metadata v24.
**Target:** CachyOS x86-64, Wayland only, Vulkan 1.3, 1920×1080, 60 FPS.
**Gate:** Gate B remains **PARTIAL**. Gate C is **PARTIAL** — launch, present, and
resize are manually verified; input/gameplay are not. The evidence record is
`analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` and `docs/ASSET_PIPELINE.md`.

---

## 0. Current implementation state (2026-10-31)

| Area | State | Evidence |
|---|---|---|
| Application lifecycle / shutdown | FUNCTIONAL | `src/app/Application.*` |
| SDL3 Wayland-only window | FUNCTIONAL / manual PASS | `src/platform/wayland/Window.*` |
| Event resize/minimize/focus/close | FUNCTIONAL; restore handler unit-tested | `WindowState`, smoke log |
| Vulkan instance/device/queues/swapchain | FUNCTIONAL / validation-clean | `src/engine/rendering/VulkanContext.*` |
| Clear + textured quad | FUNCTIONAL / visual PASS | `SpriteRenderer`, `shaders/` |
| 50 Hz fixed loop with 60 FPS render | FUNCTIONAL | `FrameTimer`, smoke log |
| PNG → RGBA8 → GPU texture | FUNCTIONAL | `PngDecoder`, `TextureData`, libpng, `tests/unit/png_test.cpp` |
| Asset registry/KTX2/atlas | NOT STARTED | manifest conversion not implemented |
| Input → fixed simulation | NOT STARTED | `InputManager` not wired |
| Audio/UI/gameplay | NOT STARTED | deliberate boundary |

Do not read this table as parity. The reference layer order and native rendering
mechanism are **PROJECT DECISION** unless explicitly marked otherwise.

## 1. Architecture

Dependency direction (arrows mean "depends on"):

```text
Application
  ├── Platform        (SDL3, Wayland, filesystem, timing)
  ├── Asset Runtime   (decoded asset registry, conversion output)
  ├── Renderer        (Vulkan device/swapchain/sprites)
  ├── Input           (action snapshots)
  ├── Audio           (miniaudio engine)
  ├── Simulation      (fixed clock, RNG, ECS world, systems)
  │     └── Gameplay  (controllers, projectiles, damage, rooms)
  └── UI              (HUD/menu, after first slice)
```

This is a dependency graph, not a strict call stack. The renderer reads simulation
state; simulation does not call the renderer. Gameplay does not own Vulkan or SDL
objects.

### Ownership

| Layer | Owns | Must not own |
|---|---|---|
| Application | lifecycle, config, top-level loop, shutdown ordering | gameplay rules |
| Platform | SDL window, event queue, Wayland enforcement, XDG paths, frame timer | Vulkan objects, gameplay state |
| Renderer | Vulkan instance/device/queues/swapchain/pipelines/descriptors/GPU resources | gameplay state, input semantics |
| Asset Runtime | asset IDs, decoded CPU data, manifest, conversion metadata | GPU handles, gameplay entities |
| Simulation | fixed clock, RNG streams, ECS world, system order, timers | window/GPU/audio devices |
| Gameplay | controllers, role attributes, weapons, projectiles, damage, rooms | platform APIs |
| UI | HUD/menu state and draw data | simulation mutation |
| Audio | miniaudio engine, voices, bus volumes | gameplay decisions |

### Reference behaviour vs native mechanism

The native runtime recreates **behaviour**, not Unity's internal architecture. A
Unity `MonoBehaviour` may become an ECS component plus a system. A Unity Animator
may become a native animation state machine. A Unity coroutine may become an
explicit timer. The reference RNG stream/order is preserved; the native object
layout is not.

---

## 2. Initialization and shutdown order

### Startup

1. Logging and config (`config/default.toml`, CLI overrides).
2. Resolve XDG paths and ensure directories.
3. Create SDL3 window with `SDL_HINT_VIDEO_DRIVER=wayland`; fail if the active
   driver is not `wayland`.
4. Create Vulkan instance (validation in Debug; `VK_KHR_surface`,
   `VK_KHR_wayland_surface`).
5. Create surface; select physical device; create logical device/queues.
6. Create swapchain, image views, render pass/dynamic rendering, framebuffers.
7. Create frame resources (command pools/buffers, semaphores, fences).
8. Create sprite pipeline, descriptor layouts/sets, sampler.
9. Initialize audio engine.
10. Initialize input manager.
11. Load the first asset manifest and required room/player/enemy/weapon assets.
12. Create the ECS world and initial gameplay state.
13. Enter the main loop.

### Shutdown

Reverse order, with GPU idle before resource destruction:

```text
simulation/gameplay teardown
→ asset runtime release
→ input shutdown
→ audio shutdown
→ vkDeviceWaitIdle
→ descriptor/pipeline/swapchain/image views
→ surface/device/instance
→ SDL window/quit
→ logging shutdown
```

No static destructor may own Vulkan or SDL handles.

---

## 3. Thread and frame model

### First implementation: single-threaded

- Main thread: SDL events, input snapshot, fixed simulation, render submission,
  present.
- Audio: miniaudio internal thread; no gameplay calls from it.
- Asset conversion may use a worker pool later; GPU uploads stay on the render
  thread.

### Frame loop

```text
poll SDL events
sample input
accumulate real frame time (max catch-up 0.25 s)
while accumulator >= 0.02:
    consume one fixed tick
    run gameplay/AI/physics/projectile systems
    advance RNG streams in reference order
render with interpolation alpha
present
```

- Fixed step: **0.02 s (50 Hz)**, confirmed from `TimeManager`.
- Render target: 60 FPS; do not tie simulation to render rate.
- Input: one stable snapshot per fixed tick; UI reads per-frame state separately.
- Timers/coroutines: explicit queue advanced by fixed ticks; destruction timers
  use the same clock.
- Do not claim deterministic lockstep until RNG ordering and collision ordering are
  verified.

---

## 4. Vulkan renderer plan

### 4.1 Window and surface

- SDL3 `SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY`.
- Force `SDL_HINT_VIDEO_DRIVER=wayland`; reject X11/XWayland at startup.
- `SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)`.
- No X11 surface extensions, no XWayland fallback.

### 4.2 Instance

- Vulkan 1.3.
- Extensions: `VK_KHR_surface`, `VK_KHR_wayland_surface`; optional
  `VK_EXT_debug_utils` in Debug.
- Validation layers only in Debug; no validation dependency in Release.

### 4.3 Physical device

- Require graphics + present queue families; swapchain support; Vulkan 1.3.
- Prefer discrete GPU, fall back to integrated.
- Log device name, API version, driver version.
- No hardcoded ICD path; use the system loader.

### 4.4 Logical device and queues

- One graphics queue, one present queue (may be same family).
- Enable `VK_KHR_swapchain`.
- No async compute/transfer queue in the first milestone.

### 4.5 Swapchain

- 1920×1080 requested; clamp to surface caps.
- Prefer `VK_FORMAT_B8G8R8A8_SRGB`/`R8G8B8A8_SRGB`; fall back to UNORM.
- Color space sRGB non-linear.
- Present mode: FIFO for deterministic 60 FPS; mailbox optional later.
- 2–3 images; recreate on resize/out-of-date/suboptimal.
- Handle minimized window by pausing rendering.

### 4.6 Frame resources

- 2 frames in flight.
- Per frame: command pool/buffer, image-available semaphore, render-finished
  semaphore, in-flight fence.
- `vkWaitForFences` / `vkResetFences` / `vkAcquireNextImageKHR` /
  `vkQueueSubmit` / `vkQueuePresentKHR`.
- Destroy/recreate on swapchain change.

### 4.7 Pipelines and shaders

- Sprite pipeline: position (vec2), UV (vec2), color (vec4), orthographic
  projection, alpha blending, no depth test.
- Shader source under `shaders/`; compile GLSL → SPIR-V offline with `glslc` or
  `shaderc`; commit source and build rule, not generated binaries unless the build
  requires them.
- Descriptor: combined image sampler per texture; grow to bindless/array later.
- Push constants for view-projection and tint.

### 4.8 Textures and sprites

- **Implemented:** libpng PNG decode to canonical RGBA8 `TextureData`; upload
  through the existing staging buffer; transition to
  `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
- **Planned:** KTX2/atlas decode and packed sprite metadata.
- Sprite metadata from the asset runtime: atlas rect, pivot, pixels-per-unit.
- Dynamic vertex buffer for batched quads; index buffer with fixed quad pattern.
- Sort by layer/sorting order; preserve reference sorting layer names
  (`BackGround`, `Floor`, `Shadow`, `Wall`, `Character`, `WallFont`, `Effect`, `UI`).

### 4.9 Frame pacing and resize

- FIFO present for 60 FPS; monitor frame time and log overruns.
- Recreate swapchain on `VK_ERROR_OUT_OF_DATE_KHR`/`VK_SUBOPTIMAL_KHR`.
- Do not resize the simulation; only the render extent changes.

### 4.10 Smoke-test milestone

Acceptance:

- Wayland-only window opens at 1920×1080.
- Vulkan instance/device/swapchain created and logged.
- Clear color presents for 5 seconds. **PASS 2026-10-31** (textured quad added).
- Resize and minimize/restore work. **PARTIAL** — resize/minimize/focus observed;
  programmatic restore event was not delivered by this xdg-shell compositor.
- Clean exit with no validation errors in Debug. **PASS** — 0 validation messages.
- `vulkaninfo` not required at runtime; no hardcoded ICD. **PASS**.

Manual command: `./build/soul-knight --smoke-seconds 10` (or
`./tools/smoke_phase4.sh`). See `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`.

---

## 5. Asset runtime plan

### 5.1 Pipeline

```text
APK/AssetBundle (local reference only)
    ↓ UnityPy extraction (tools/asset/extract_unity.py)
manifest + PNG/WAV/JSON (gitignored)
    ↓ conversion tool (new, Phase 4)
canonical native assets (KTX2/OGG/JSON)
    ↓ runtime asset registry
CPU/GPU resources
```

### 5.2 Data classes

- **Texture**: image bytes, dimensions, format, mip count, hash.
- **Sprite**: texture ID, rect, pivot, pixels-per-unit, border.
- **Prefab/room**: hierarchical nodes, transform, sprite/script IDs, collider
  descriptors, spawn points.
- **AudioClip**: decoded/streamed path, sample rate, channels, duration.
- **AnimationClip**: keyframes, events, loop mode (later).
- **Metadata**: bundle, path ID, name, source hash, identity confidence.

### 5.3 Runtime loading

- Search order: `$XDG_DATA_HOME/soul-knight/assets`, executable-relative
  `assets/`, `/usr/share/soul-knight/assets`.
- Manifest keyed by stable asset ID (`bundle:path_id`), never by filename alone.
- No proprietary bytes committed; conversion output is gitignored.
- Missing asset is a hard error with the asset ID and expected path.

### 5.4 First-slice asset set

- `room/r1_1` prefab/hierarchy and its sprite dependencies.
- `c00` player prefab + `RoleAttributePlayer` data.
- `e_slime01` enemy prefab + `EnemyAI04` data.
- `weapon_000` + `bullet_0`.
- Selected `res/floor`, `res/charactor_common`, UI and audio samples from the
  existing manifest.

Identity remains **candidate** until a serialized display-name mapping is found.

---

## 6. Native gameplay architecture

### 6.1 Components and systems

Suggested native components:

```text
Transform2D, SpriteRef, Collider2D, Rigidbody2D,
RoleAttribute, PlayerController, EnemyController,
Weapon, Projectile, DamageCarrier, Health/DeathState,
RoomMember, Door, SpawnPoint, Timer
```

Systems, in fixed-step order:

1. input snapshot application
2. controller/AI intent
3. movement/velocity
4. physics/collision
5. projectile update
6. trigger/damage dispatch
7. death/state transitions
8. timers/spawning/despawn
9. room/door state
10. audio/UI event publication

Do not copy Unity's execution order blindly; preserve observable ordering only
where evidence exists (damage → death → collider/weapon disable).

### 6.2 RNG streams

- `UnityRandom` shared scratch state for reference-compatible wrapper semantics.
- Per-object `RGRandomStream` for map/controller/weapon/enemy streams.
- Seed order: run/sample seed → map seed → room/object streams → weapon/controller
  streams where evidence supports it.
- No PCG32 on parity-sensitive paths.

### 6.3 Collision

- First slice: AABB/circle against room wall prefab colliders and door/aisle
  triggers; layer mask from the reference layer table.
- `Rigidbody2D` equivalent: velocity + kinematic/dynamic flag, no full Box2D.
- Preserve trigger vs solid distinction; bullets are triggers, walls are solid,
  door triggers are triggers.
- Wall placement blocker must be resolved before claiming room collision parity.

### 6.4 Damage and death

- Implement the reconstructed ordered pipeline from `analysis/re/damage.md`.
- Keep `DamageInfo`/`BulletInfo` as value objects; do not conflate with health.
- Player: defence/mount/one-shot shield/armor overflow/HP/CanDie.
- Enemy: defence factor, HP subtraction, callback, death threshold.
- Death: disable collider/weapon, set flags, publish death event, start timer/
  animation; game-over/reborn after the slice.
- Do not implement crit/element formulas that are still unresolved; use an explicit
  `HYPOTHESIS` flag only if the slice needs a placeholder.

### 6.5 Rooms

- Load `r1_1` hierarchy from converted room JSON.
- Instantiate visual sprites and collider descriptors.
- Use the door/aisle components as data-driven objects; first slice may keep doors
  open and skip transitions if clearly labelled.
- Player spawn: use `b_point` only after confirming its transform semantics; until
  then use an explicit native spawn marker in the converted room data.

---

## 7. Implementation sequence

Every stage requires: goal, modules, tests, acceptance, dependencies, risks.

### Stage 1 — Application bootstrap — **FUNCTIONAL (2026-10-31)**

- **Goal:** deterministic startup/shutdown, logging, config, XDG paths.
- **Modules:** `src/app/Application.*`, `src/engine/core/Log.*`,
  `src/platform/filesystem/FileSystem.*`.
- **Tests:** startup/shutdown smoke; config parse; XDG path resolution.
- **Acceptance:** clean exit, no leaked SDL/Vulkan handles.
- **Risks:** early GPU init complicates headless tests; keep platform init lazy.

### Stage 2 — Wayland/SDL3 window — **FUNCTIONAL (2026-10-31)**

- **Goal:** Wayland-only window, event loop, resize/minimize/focus.
- **Modules:** `src/platform/wayland/Window.*`, `src/platform/input/*`.
- **Tests:** driver-name assertion; synthetic resize; focus events.
- **Acceptance:** refuses non-Wayland; 1920×1080.
- **Risks:** compositor differences; libdecor availability.

### Stage 3 — Vulkan smoke renderer — **FUNCTIONAL (2026-10-31)**

- **Goal:** instance/device/swapchain/clear/present/resize.
- **Modules:** `src/renderer/vulkan/*`.
- **Tests:** device creation, swapchain recreation, validation-clean frame.
- **Acceptance:** 5-second clear-screen run, clean exit.
- **Risks:** driver-specific format/present support; handle fallbacks.

### Stage 4 — Asset runtime foundation — **PARTIAL (2026-10-31)**

- **Goal:** manifest → CPU asset registry; PNG/room/audio decode.
- **Implemented:** libpng-backed PNG decode to canonical RGBA8 `TextureData`,
  synthetic fixtures, deterministic/exact-byte tests, malformed-input rejection,
  executable-relative asset lookup, PNG decoded into the existing `Texture2D`
  staging path and rendered as the smoke quad.
- **Not yet:** KTX2/atlas, manifest/registry, room/audio conversion, UnityPy
  output import.
- **Modules:** `src/assets/*`, `tools/asset/convert_*`.
- **Tests:** texture decode, sprite rect, room JSON parse, missing-asset error.
- **Acceptance:** load `r1_1`, `c00`, `e_slime01`, `weapon_000`, `bullet_0`
  metadata without proprietary bytes committed.
- **Risks:** identity ambiguity; keep candidate labels.

### Stage 5 — Sprite renderer — **PARTIAL (2026-10-31)**

- **Goal:** textured quads, atlas UVs, camera, sorting layers.
- **Implemented:** GLSL sprite pipeline, generated RGBA8 texture, alpha blending,
  dynamic batched quads, stable layer sort, aspect-fit resize.
- **Not yet:** PNG/atlas assets, camera follow, real room sprite scale/pivots.
- **Modules:** `src/renderer/vulkan/SpriteRenderer.*`, `src/engine/rendering/Camera.*`.
- **Tests:** one sprite; atlas sprite; camera transform; resize.
- **Acceptance:** room floor/wall sprites render at correct scale.
- **Risks:** pixels-per-unit/pivot mismatches; validate against extracted sprite metadata.

### Stage 6 — Simulation clock — **PARTIAL (2026-10-31)**

- **Goal:** 50 Hz fixed step, accumulator, input snapshots, interpolation.
- **Implemented:** confirmed 50 Hz accumulator, consumed-step statistics, render
  decoupling in `Application`.
- **Not yet:** gameplay systems, input snapshot bridge.
- **Modules:** `src/sim/Clock.*`, `src/platform/timing/FrameTimer.*`.
- **Tests:** tick count over simulated time; render-rate independence; catch-up cap.
- **Acceptance:** 50 ticks per simulated second; no spiral of death.
- **Risks:** coroutine ordering; keep timers explicit.

### Stage 7 — Input abstraction

- **Goal:** WASD/joystick movement, attack/skill/pause actions.
- **Modules:** `src/platform/input/*`, `src/game/player/PlayerInput.*`.
- **Tests:** key/axis injection; deadzone; action press/held/release.
- **Acceptance:** movement vector matches reference keyboard/joystick conventions.
- **Risks:** exact attack button mapping; verify against UI prefab/trace.

### Stage 8 — Room loading

- **Goal:** instantiate `r1_1` nodes, sprites, colliders, markers.
- **Modules:** `src/game/rooms/*`, `src/assets/room_loader.*`.
- **Tests:** node count, bounds, door triggers, spawn markers.
- **Acceptance:** 31×31 world bounds; 4 doors/aisles; no invented geometry.
- **Risks:** wall collider placement unresolved; may need an explicit first-slice
  approximation with a `HYPOTHESIS` marker.

### Stage 9 — Collision representation

- **Goal:** AABB/circle vs walls/doors, trigger events.
- **Modules:** `src/engine/collision/*`, `src/sim/Physics.*`.
- **Tests:** wall stop, door trigger, bullet trigger, layer mask.
- **Acceptance:** player cannot cross wall bounds; bullets hit targets.
- **Risks:** Box2D vs AABB differences; document approximation.

### Stage 10 — Player entity

- **Goal:** c00/C01Controller equivalent, stats, collider, Rigidbody2D.
- **Modules:** `src/game/player/*`, `src/game/combat/RoleAttribute.*`.
- **Tests:** spawn, stats, move, damage, death.
- **Acceptance:** speed 6.5/speed_rate 0.5 contract; HP/armor values.
- **Risks:** display identity candidate; do not name Knight.

### Stage 11 — Player movement

- **Goal:** `SetVelocity` equivalent: direction/weight → rigidbody velocity.
- **Modules:** `src/game/player/MovementSystem.*`.
- **Tests:** normalized diagonal, acceleration/deceleration, collision stop.
- **Acceptance:** fixed-step movement matches reconstructed formula.
- **Risks:** exact speed bonus fields; validate with in-game values only if trace.

### Stage 12 — Weapon

- **Goal:** `weapon_000`/`Gun001` attack → bullet creation.
- **Modules:** `src/game/weapons/Gun001.*`, `src/game/weapons/Weapon.*`.
- **Tests:** attack creates one bullet; damage/speed/crit values; cooldown if known.
- **Acceptance:** bullet proto `bullet_0`, speed 36 candidate.
- **Risks:** fire cadence/ammo unresolved; mark approximation.

### Stage 13 — Projectile

- **Goal:** bullet spawn, velocity, lifetime, trigger, destruction.
- **Modules:** `src/game/projectiles/Bullet01.*`, `src/sim/ProjectileSystem.*`.
- **Tests:** travel, lifetime 5 s, collision trigger, despawn.
- **Acceptance:** stable straight projectile; no invented homing.
- **Risks:** velocity formula unresolved; validate against weapon field.

### Stage 14 — Enemy

- **Goal:** `e_slime01`/`EnemyAI04` minimal behavior.
- **Modules:** `src/game/enemies/*`.
- **Tests:** spawn, HP 12, damage 3, death.
- **Acceptance:** enemy can be hit/die; AI may be a documented minimal state.
- **Risks:** full AI unknown; do not claim parity.

### Stage 15 — Damage pipeline

- **Goal:** projectile → player/enemy/box → GetHurt/armor/HP/death.
- **Modules:** `src/game/combat/*`.
- **Tests:** armor overflow, shield, enemy HP, box hit, death.
- **Acceptance:** reconstructed ordering; unit tests for each branch.
- **Risks:** crit/element unresolved; keep out or flag.

### Stage 16 — Death/restart

- **Goal:** death flags, collider/weapon disable, restart.
- **Modules:** `src/game/state/*`.
- **Tests:** death once, no duplicate callbacks, restart resets world.
- **Acceptance:** restart produces no stale entities.
- **Risks:** reborn/game-over full path unresolved.

### Stage 17 — HUD

- **Goal:** HP/armor/energy from native state; real UI sprites.
- **Modules:** `src/game/ui/*`, `src/renderer/ui/*`.
- **Tests:** value binding, no fabricated max values.
- **Acceptance:** values match role attributes.
- **Risks:** UI asset identity; use extracted sprites.

### Stage 18 — Audio

- **Goal:** one BGM + few SFX through miniaudio.
- **Modules:** `src/platform/audio/*`, `src/game/audio/*`.
- **Tests:** load/play/stop, missing file error, bus volume.
- **Acceptance:** no crash, no audio thread gameplay calls.
- **Risks:** codec/format support.

### Stage 19 — Vertical-slice integration

- **Goal:** complete `r1_1` loop: spawn, move, shoot, hit, die, restart.
- **Modules:** integration only.
- **Tests:** full scenario; headless/offscreen where possible.
- **Acceptance:** all slice tests pass; no invented assets.
- **Risks:** accumulated approximation debt; document each.

### Stage 20 — Reference comparison and performance

- **Goal:** compare against approved reference evidence; profile.
- **Modules:** tools/profiling.
- **Tests:** reference-value regressions where known.
- **Acceptance:** no unresolved parity claim; performance within budget.
- **Risks:** no runtime oracle; comparison limited to static values/asset geometry.

---

## 8. Test plan

### Engine

- RNG state/vector/range/float/inside-circle tests (existing).
- Vector/matrix/camera math.
- Fixed clock: 50 ticks/s, catch-up cap, interpolation.

### Platform

- Wayland-only startup; X11 fallback rejection.
- Vulkan instance/device/queue selection.
- Swapchain create/recreate/present.
- Validation-clean Debug run.
- Headless mode does not require a display.

### Assets

- **Implemented:** PNG decode dimensions/format/exact bytes, malformed failure.
- **Planned:** Sprite rect/pivot/atlas UV.
- Room JSON node/collider/spawn parse.
- Missing asset error.
- Manifest hash/path-ID stability.

### Simulation

- Movement: keyboard/joystick vector, normalized diagonal, collision stop.
- Collision: wall stop, door trigger, bullet trigger, layer mask.
- Projectile: spawn, velocity/lifetime, hit, despawn.
- Damage: armor overflow, shield, enemy HP, box hit, death.
- Death/restart: flags, no duplicate calls, world reset.

### Integration

- `r1_1` scenario: spawn → move → shoot → hit enemy → enemy death → player damage
  → player death → restart.
- No proprietary assets in Git; assets loaded from ignored/generated paths.

### Regression

- Deterministic tests only where evidence supports determinism.
- Reference-value tests for `TimeManager`, prefab stats, collider geometry, damage
  constants.
- No tests based on invented values or candidate display names.

---

## 9. Performance plan

Target: **60 FPS = 16.6667 ms/frame**; simulation 50 Hz.

Provisional per-frame budget (main thread, 1080p):

| Area | Budget |
|---|---:|
| Simulation (50 Hz amortised) | 2.0 ms |
| Rendering CPU (culling/batching/draw submission) | 8.0 ms |
| GPU frame | within vsync budget |
| Asset streaming/decode | 1.0 ms amortised |
| Audio | 1.0 ms main-thread max |
| UI | 1.0 ms |
| Synchronization/other | 3.6 ms |
| **Total** | **16.6 ms** |

Rules:

- Do not optimise before the smoke renderer and slice are correct.
- Measure CPU frame time, GPU time (timestamp queries), draw calls, vertices,
  texture binds, and swapchain waits.
- Use `perf`, Tracy (optional), RenderDoc for GPU captures.
- Avoid per-frame allocation in simulation/render hot paths.
- Batch sprites; sort by pipeline/texture/layer.
- Keep asset uploads off the critical path; use staging + async where safe.
- Log frame overruns and swapchain recreation.
- Do not claim 60 FPS until measured on the target CachyOS/Wayland machine.

---

## 10. Error handling, logging, configuration, save locations

- **Errors:** `std::expected`/status codes where practical; no exceptions
  (`-fno-exceptions`). Fatal startup errors log and exit non-zero.
- **Logging:** existing `Log` levels; subsystem tags; no per-frame spam.
- **Config:** `config/default.toml` + XDG config override; no hardcoded paths.
- **Saves:** `$XDG_DATA_HOME/soul-knight/saves`; format not yet reconstructed, so
  do not claim reference save compatibility.
- **Assets:** `$XDG_DATA_HOME/soul-knight/assets`, executable-relative `assets/`,
  `/usr/share/soul-knight/assets`.
- **Caches/logs:** `$XDG_CACHE_HOME/soul-knight`.

---

## 11. Next implementation task

The PNG asset path is verified. The next task is **controlled Unity-extracted
asset conversion → canonical native asset representation**:

- select a small, already-manifested non-proprietary test subset only after
  establishing a deterministic conversion manifest;
- convert one sprite/metadata pair into the canonical RGBA8/`TextureData` or an
  atlas descriptor with documented source hashes;
- add conversion-output tests and a visible smoke asset;
- keep extracted proprietary bytes out of Git and keep the APK/bundles out of the
  repository.

Do not import the full asset catalogue and do not begin gameplay/content work.
Acceptance: reproducible conversion report, exact decoded bytes for the chosen
synthetic/approved source, Debug/RelWithDebInfo tests green, and no proprietary
asset bytes tracked.
