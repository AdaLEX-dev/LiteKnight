# Testing Strategy

**Status:** current baseline is **31 doctest cases / 1335 assertions**, all passing.
Future tests must be evidence-backed; do not invent expected values.

---

## 1. Current harness

```bash
./tools/configure.sh Debug
./tools/build.sh Debug
./tools/test.sh
ctest --test-dir build --output-on-failure
```

- doctest 2.4.11, CTest entry `soul-knight-unit-tests`.
- Debug: ASan/UBSan.
- RelWithDebInfo: optimized, no sanitizers.
- CTest fails if the filter selects zero cases (`FAIL_REGULAR_EXPRESSION`).
- Negative control: break an assertion, confirm CTest exits non-zero, restore.

## 2. Test layers

### Engine

- UnityRandom: seed expansion, Xorshift vectors, int/float range, min==max,
  min>max, global save/restore, lazy seed, independent streams, inside-circle.
- Math: Vec2/Vec3/Mat4/camera.
- Timing: 50 Hz fixed step, accumulator clamp, interpolation.

### Platform (implemented units)

- AppConfig defaults: 1920x1080, 50 Hz simulation, 60 FPS target.
- Wayland-only driver validation; non-Wayland rejected.
- Bootstrap CLI bounds and unknown-option tolerance.
- Window-state resize/minimize/restore/focus/close transitions.
- Safe repeated Application shutdown before initialization.
- FrameTimer consumed-step statistics.

### Renderer (implemented units)

- SpriteBatch layer sort and stable insertion order.
- SpriteBatch quad budget.
- Shader build is a CMake target (`glslc`); SPIR-V is generated in the build tree.
- Vulkan instance/device/swapchain/present is an **integration/manual** test,
  not a CTest case: it requires a real Wayland compositor and GPU.

### Integration / manual smoke

```bash
./tools/smoke_phase4.sh
# or: ./build/soul-knight --smoke-seconds 10
```

Requirements: real Wayland session, SDL3, Vulkan 1.3, working GPU. The script
checks driver, Wayland enforcement, validation status, swapchain/renderer markers,
clean smoke completion, and rejects validation/renderer errors. It is intentionally
not registered with CTest.

### Assets (planned; not implemented)

- Manifest path-ID/hash stability.
- PNG decode dimensions/format.
- Sprite rect/pivot/UV.
- Room JSON node/collider/spawn parse.
- Missing asset → clear error.
- No proprietary bytes tracked.

### Simulation

- Movement: keyboard/joystick vector, diagonal normalization, collision stop.
- Collision: wall/door trigger/bullet trigger/layer mask.
- Projectile: spawn, lifetime, hit, despawn.
- Damage: armor overflow, shield, enemy HP, box hit, death.
- Death/restart: flags, duplicate-callback prevention, world reset.

### Integration

- `r1_1`: spawn → move → shoot → hit enemy → enemy death → player damage → death →
  restart.
- Headless/offscreen mode where possible.

### Regression

- Reference values: fixed timestep 0.02, prefab stats, collider bounds, damage
  constants, RNG vectors labelled as reconstructed (not runtime-verified).
- No tests based on candidate display names or invented gameplay.
- Deterministic tests only where evidence supports determinism.

## 3. Rules

- Never weaken a test to make a gate green.
- Never add a test that asserts a hypothesis as fact; label it or omit it.
- Every new subsystem gets at least one failing-capable test.
- Keep the count visible; explain any change in the audit.
- Sanitizer failures are failures, not warnings.
