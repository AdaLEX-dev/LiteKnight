# Reference Timing and Frame Model

**Status:** CONFIRMED for the reference configuration.
**Scope:** Phase 2/3 timing primitives. This is evidence, not a native implementation.

---

## 1. Direct evidence

`globalgamemanagers` (`analysis/apktool_out/assets/bin/Data/globalgamemanagers`)
contains the serialized Unity `TimeManager` object (path ID 8). UnityPy reads:

```text
TimeManager(
  Fixed_Timestep = 0.019999999552965164,
  Maximum_Allowed_Timestep = 0.3333333432674408,
  m_TimeScale = 1.0,
  Maximum_Particle_Timestep = 0.029999999329447746
)
```

Therefore:

- **fixed simulation step = 0.02 s (50 Hz)**
- maximum allowed timestep = 1/3 s
- time scale = 1.0
- particle timestep = 0.03 s

This confirms the old "50 Hz" statement from the artifact itself; it is no longer
an unsupported assumption.

## 2. Runtime override check

`UnityEngine.Time.set_fixedDeltaTime` is at `0x1E9977C`. A direct-call xref scan
returns **0** call sites in `libil2cpp.so`. No game code changes the fixed timestep
at runtime through the direct IL2CPP path. Treat 0.02 s as the build configuration.

`UnityEngine.Time.get_fixedDeltaTime` at `0x1E99724` is read by 20 sites, including:

- `LifeCounter.FixedUpdate` (`0xD11A9C`)
- `RaceController.SlowDown` (`0xD311B8`)
- `PhantomCreator.FixedUpdate` (`0xDED1F8`)
- `NetPlayerController.FixedUpdate` (`0x104EA10`)
- `Bullet05.FixedUpdate` (`0x14C48F4`)
- `Bullet06.FixedUpdate` (`0x14C4E18`, `0x14C4EE8`)
- `BulletParabola.FixedUpdate` (`0x14D8CC4`, `0x14D8D78`)
- `ProCamera2D.FixedUpdate` (`0x16237EC`)

## 3. Update/FixedUpdate separation

The metadata contains both `Update` and `FixedUpdate` methods across gameplay
classes. Confirmed examples relevant to the vertical slice:

- `C01Controller.Update` (`0x12AA1BC`) → `RGController.AttributeUpdate`, `SeachUpdate`
- `C01Controller.FixedUpdate` (`0x12AA200`) → virtual `SetVelocity` (slot 21)
- `RGEController.FixedUpdate` (`0x110B7A0`)
- `Bullet01.FixedUpdate` (`0x14C1230`)
- `RGJoyControl.Update` (`0xDD7E84`) reads input and calls controller buttons
- `RGEController` AI classes implement `FixedUpdate`

The reference therefore uses the normal Unity split:

```text
Update()      : input, UI, per-frame presentation/state refresh
FixedUpdate() : physics/controller movement, AI, projectile integration
coroutines/Invoke : timers, destruction, animation-adjacent delays
```

The native runtime must not collapse both into a single 60 FPS update. The target
model is:

```text
simulation tick: 50 Hz (0.02 s), fixed
render frame:    60 FPS target (16.6667 ms), variable/interpolated
```

## 4. Coroutines, Invoke, and timers

The binary uses `MonoBehaviour.StartCoroutine`, `CancelInvoke`, `Invoke`, and
`WaitForFixedUpdate` (see `WaitForFixedUpdate..ctor` at `0x1EA90A0`). Projectile
destruction is coroutine-based: `RGBullet.DestroySelf(float)` (`0x11689FC`) starts
the `Destroying` coroutine (`0x1169310`), using `destroy_time` (`+0x88`).

Native equivalent must preserve:

- fixed-step timers for gameplay,
- coroutine-like delayed destruction,
- explicit cancellation on death/disable/reset.

The exact scheduling order between Unity coroutines and `FixedUpdate` is a Unity
engine detail and is not independently observed; it is a **HYPOTHESIS** that the
first native slice can use a deterministic timer queue ordered after the fixed
simulation step.

## 5. Confidence

| Claim | Confidence | Evidence |
|---|---|---|
| Fixed timestep is 0.02 s / 50 Hz | CONFIRMED | serialized `TimeManager.Fixed_Timestep` |
| Game does not directly set `Time.fixedDeltaTime` | HIGH CONFIDENCE | 0 direct xrefs to `0x1E9977C` |
| Reference separates Update and FixedUpdate | CONFIRMED | method inventory + `C01Controller`/`Bullet01` disassembly |
| Coroutine/Invoke timing relative to FixedUpdate | HYPOTHESIS | engine behavior, no runtime trace |
| Render target 60 FPS | PROJECT TARGET | not a reference-engine measurement |

## 6. Native scaffold alignment

`src/platform/timing/FrameTimer.hpp` now uses `TARGET_DT = 1.0 / 50.0`
(0.02 s) and `MAX_ACCUMULATOR = 0.3333333333`, matching the serialized reference
`TimeManager`. A regression test (`tests/unit/timing_test.cpp`) checks the
confirmed constant and rejects the old 60 Hz value. The native scaffold still must
not be treated as a gameplay runtime.

## 7. Native implementation contract

- Simulate at 50 Hz.
- Render at up to 60 FPS with interpolation.
- Use an accumulator with a maximum catch-up clamp.
- Keep input sampling per render frame; consume a stable input snapshot per fixed tick.
- Use explicit timer/coroutine equivalents; never use wall-clock time for gameplay.
- Do not claim deterministic lockstep until RNG ordering and physics ordering are
  verified against a reference trace.
