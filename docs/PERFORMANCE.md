# Performance Plan

**Target:** 60 FPS / 16.6667 ms frame at 1920×1080; simulation fixed at 50 Hz.
**Status:** no renderer or playable build exists; no performance claim is valid yet.

---

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
