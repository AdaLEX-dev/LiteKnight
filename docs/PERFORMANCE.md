# Performance Plan

**Target:** 60 FPS / 16.6667 ms frame at 1920×1080; simulation fixed at 50 Hz.
**Status:** the native Phase 4 renderer, fixed-step locomotion test scene, and
first-slice combat scene have been measured on one machine (Intel UHD 620,
KWin/Wayland, 1.25 scale). No cross-machine 60 FPS guarantee is claimed.

---

## 0. Measured smoke result (2026-10-31)

Command: `./build/soul-knight --smoke-seconds 10` on the target CachyOS/Wayland
machine.

- `FrameTimer` average: **16.665 ms** over 573 samples.
- Presented **603 frames** over ~10.1 s wall time (~59.6 presents/s).
- **506 fixed simulation ticks** in ~10.1 s (50 Hz contract).
- **0.834 simulation steps/render frame** (expected 50/60 ≈ 0.833).
- Debug validation: 0 messages.
- Measurement type: CPU frame pacing (`steady_clock` via `FrameTimer`), not GPU
  timestamp queries. FIFO present mode. Do not generalize to other GPUs/displays.

### Native r1_1 + c00 animated/locomotion scene

Command: `build/soul-knight --scene scenes/r1_1_visual/scene.json --locomotion-debug --smoke-seconds 10`
from `/tmp`, with the recovered wall/door/corridor geometry and wall603
obstacles, two e_slime01 fixtures, four recovered weapon definitions and the
equipped real weapon sprite. RelWithDebInfo, 482 static sprite instances plus
147 active obstacle sprites and four enemy sprites (594 drawn while all doors
are open), 14 scene textures plus the developer font (15 renderer textures),
182 sprite assets, 30 recovered character clips:

- `FrameTimer` average: **16.658 ms** over 571 samples.
- **601 frames** over ~10.14 s (~59.3 presents/s).
- **507 fixed simulation ticks** (0.832 sim steps/frame).
- Vulkan validation: 0 messages.
- A 30-second run with active movement measured ~**16.66 ms** and ~59.8
  presents/s in an earlier unloaded run; the recovered wall/door geometry and
  first-slice combat sprites did not change CPU pacing materially versus the
  earlier 111-instance composition (16.693 ms).
- Animation/geometry overhead is not separately GPU-timestamped; the measurement
  is CPU frame pacing on the test machine, not a GPU/60 FPS parity claim.

## 1. Budgets (provisional, main thread)

| Area | Budget |
|---|---:|
| Simulation (50 Hz, amortised) | 2.0 ms |
| Rendering CPU (culling/batching/submission) | 8.0 ms |
| Asset streaming/decode | 1.0 ms amortised |
| Audio main-thread | 1.0 ms |
| UI | 1.0 ms |
| Synchronization/other | 3.6 ms |
| **Total** | **16.6 ms** |

GPU frame time must fit the vsync interval; a CPU-only measurement is not enough.

## 2. Measurement

- CPU frame time, simulation tick time, render submission time.
- GPU timestamp queries per pass.
- Draw calls, vertices, descriptor binds, texture binds.
- Swapchain acquire/present waits, frame overruns.
- Memory: asset registry, GPU images/buffers, per-frame allocations.
- Tools: `perf`, RenderDoc, optional Tracy; Debug validation for correctness only,
  not performance.

## 3. Rules

- Correctness first; do not optimise before the smoke renderer and slice work.
- Batch sprites by pipeline/texture/layer.
- Avoid per-frame heap allocation in simulation/render hot paths.
- Upload assets via staging buffers off the critical path.
- Keep the fixed simulation independent of render rate; clamp catch-up.
- Profile on the target CachyOS/Wayland machine.
- Do not claim 60 FPS until measured with the real renderer and slice content.
- No `-march=native`; release binaries must stay reproducible.
