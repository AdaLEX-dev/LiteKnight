# Testing Strategy

**Status:** current baseline is 23 doctest cases / 1276 assertions, all passing.
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

### Platform

- Wayland driver enforcement; non-Wayland startup fails.
- SDL event handling/resize/minimize/focus.
- Vulkan instance/device/queue selection.
- Swapchain create/recreate/present.
- Validation-clean Debug frame.
- Clean shutdown/idempotence.

### Assets

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
