# Phase 4 — Native Runtime Foundation Audit

**Date:** 2026-10-31
**Baseline commit:** `9c506dc4288b6170f2e699743f4b591eea7e7ea0`
**Scope:** Phase 4A-4E foundation only. No gameplay, rooms, enemies, weapons,
projectiles, damage, HUD, save, procedural generation, or content catalogue was
implemented.

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
| Asset registry/KTX2/atlas | NOT STARTED | manifest conversion not implemented |
| Input → fixed tick | NOT STARTED | `InputManager` not wired into the loop |
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

- Debug tests: **35 cases / 1367 assertions, all passing**.
- CTest: `soul-knight-unit-tests` 1/1 passing.
- New tests:
  - config defaults + Wayland-only rejection,
  - bootstrap CLI parsing,
  - window-state transition coverage,
  - safe repeated shutdown before initialization,
  - sprite-batch layer sorting/stability and quad budget,
  - `FrameTimer` consumed-step statistics,
  - PNG exact RGBA bytes/alpha, RGB/gray/gray-alpha/palette normalization,
    repeated-decode determinism, malformed and missing-file failure.

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
`/tmp/opencode/phase4_png_quad.png` (temporary, not committed). It shows the
1920x1080-target window, dark clear color, and the decoded synthetic asymmetric
PNG with distinct quadrants, white/black orientation markers, and alpha border.
The app was launched from `/tmp` using the executable-relative asset search path.
Generated images/logs are intentionally not tracked.

---

## 5. Remaining blockers / gaps

1. **KTX2/atlas and asset registry** — not implemented. PNG is now decoded via
   libpng; KTX2 packing, sprite metadata/atlas, and the full registry remain.
2. **Shader/install path** — run-time shader and test-asset lookup now goes through
   `FileSystem`'s executable-relative/user/system search paths and CMake copies
   assets into `build/assets`; a signed/installed package still needs to install
   `share/soul-knight/assets/{test,shaders}`.
3. **Input → fixed tick** — `InputManager` exists but is not wired into the
   application loop; no gameplay input is consumed.
4. **Real 1920x1080 surface on this display** — the compositor's 1.25 scale and
   1536x864 logical desktop constrain the window. The code requests 1920x1080 and
   handles the actual surface extent correctly.
5. **Restore event observation** — handler is implemented and unit-tested; the
   programmatic xdg-shell restore path was not delivered by this compositor.
6. **Rendering parity** — no reference comparison; native layer numbers and
   rendering mechanism are project decisions.
7. **Audio/UI/gameplay** — deliberately out of scope for this session.
8. **Performance guarantee** — FIFO pacing measured on this machine only; no GPU
   timestamps, no multi-GPU/display matrix, no 60 FPS guarantee.

---

## 6. Exact next task

**Controlled Unity-extracted asset conversion:** choose one small, manifest-backed
source asset, produce a canonical native representation with source hashes, and add
deterministic conversion tests. Do not import the full catalogue, commit proprietary
bytes, or begin gameplay.
