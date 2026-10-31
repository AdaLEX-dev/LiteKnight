# Native Locomotion Slice — Evidence and Design

**Date:** 2026-10-31
**Scene:** `r1_1_visual` — a native reverse-engineering test reconstruction
assembled from the real Unity `room/r1_1` prefab. It is **not** proven to be the
game's canonical starting level or character-select scene.

---

## 1. Input

### Recovered evidence

- Unity legacy `InputManager` uses keyboard `w`/`s`/`a`/`d` and `Input.GetAxis`
  for horizontal/vertical movement.
- `RGJoyControl.GetKeyboardMovementInput` constructs a `MovementInput`:
  normalized `dir`, `weight = 1.0`.
- The native input layer reuses this project's existing SDL3 action mapping:
  `W/Up`, `S/Down`, `A/Left`, `D/Right`.
- Input is sampled once per render frame and consumed by every fixed simulation
  step in that frame. It is not sampled from render delta time.

### Native design

`platform::ReadDigitalMovement(InputSnapshot)` is a keyboard-only abstraction.
It produces a normalized `MovementInput { direction, weight }`; diagonal input is
normalized so it cannot move faster than cardinal input. Gamepad/joystick axes
remain scaffolded in `InputManager` but are not used for locomotion yet.

## 2. Movement

### Recovered evidence

`C01Controller.FixedUpdate` eventually calls `RGController.SetVelocity`
(`0x117A6E4`). The c00 `RoleAttributePlayer` serialized values are:

```text
speed      = 6.5
speed_rate = 0.5
```

The observed low-inertial branch computes:

```text
velocity = normalized_direction
           * (speed * (speed_rate + 1.0) + speed_bonus)
```

with the `processSpeedFactor`/bonus field defaulting to `0` in this reconstruction.
Therefore the native effective speed is:

```text
6.5 * (0.5 + 1.0) = 9.75 world units/s
```

Confidence: **HIGH CONFIDENCE** for the reference formula and c00 values;
`9.75` is a native arithmetic result, not a separately observed runtime value.

### Native design

`PlayerLocomotion` advances position only inside `FixedUpdate(0.02)`:

```text
input snapshot -> desired direction/weight
  -> requested delta = direction * effective_speed * weight * 0.02
  -> AABB collision resolution
  -> position/velocity
```

The player's ground anchor remains the world-space feet position. Sprite pivot
correction is handled by the existing `SpriteData` / `SpriteRenderer` path.

## 3. Collision

### Recovered evidence

- `r1_1` serializes `room_width = 15`, `room_height = 15`.
- `RGRoomX.CreateWall` generates 1×1 wall prefabs on the Edge layer (17). Exact
  per-cell gaps and door-open conditions remain PARTIAL/UNKNOWN.
- `b_point` floor markers occupy `x = 14..26`, `y = 14..26` (step 2).
- Serialized door/bullet colliders are at the room door areas; they are not the
  complete wall geometry.

### Native design

For this test chamber, collision uses a deterministic swept AABB with:

```text
playable room interior: [13.5, 13.5] .. [26.5, 26.5]
```

This is a **NATIVE APPROXIMATION**: 15×15 room dimensions with a 1-unit perimeter
wall ring. The player collision footprint is a 0.5×0.5 AABB at the feet, also a
native approximation; the serialized c00 `BoxCollider2D` (0.5×0.5, offset 0,0.7)
is a trigger/hurtbox and is not used as the solid wall collider.

Collision behavior:

- X and Y axes resolve separately, producing wall sliding;
- swept checks prevent crossing thin walls even for a large test delta;
- normal movement is 0.195 units per 50 Hz tick, well below wall thickness;
- no complex physics engine, gravity, or rigidbody simulation is introduced.

Doors, door gaps, and the full `CreateWall` gap algorithm are not reconstructed in
this milestone. The current boundary is closed and explicit as a test-chamber
approximation.

## 4. Idle/run state

### Recovered evidence

The c00 controller's Base Layer contains both selected states:

| State | State path ID | Clip | Clip path ID |
|---|---:|---|---:|
| `ide` | `580769968` | `char_1_skin_0_ide` | `-6496688600943111911` |
| `run` | `4142231225` | `char_1_skin_0_run` | `3681644519574295089` |

Both clips are 0.5 s at 8 Hz, loop, and bind
`SpriteRenderer.m_Sprite` at `img/body`. The two clips have four frames each:
idle `c01_4..c01_7`, run `c01_0..c01_3`. See `analysis/assets/animation.md`.

### Native design

`LocomotionAnimator` is the minimum state machine:

```text
movement weight == 0 -> ide
movement weight > 0  -> run
```

State changes reset the selected clip to time `0`. On return to idle, the player
world position is unchanged. The run clip is not a fallback for a missing clip;
if absent, the animator stays on idle. No combat, dead, or HitLayer states are
implemented.

## 5. Camera follow

### Native design

`Camera2D::follow_smoothing = 8.0`. Each fixed tick:

```text
camera.position += (player.position - camera.position)
                   * (1 - exp(-8.0 * 0.02))
```

The target is the player's feet position. There are no room-edge camera limits.
The smoothing value is arbitrary native design because the reference camera
follow constants have not been recovered. `follow_smoothing = 0` snaps exactly
for tests.

## 6. Classification summary

| Claim | Confidence |
|---|---|
| WASD movement mapping and keyboard normalization | CONFIRMED reference path + native implementation |
| c00 speed 6.5 / speed_rate 0.5 | CONFIRMED serialized values |
| `SetVelocity` low-inertial speed formula | HIGH CONFIDENCE; effective 9.75 native result |
| Fixed 50 Hz movement/collision | CONFIRMED timing reference + NATIVE DESIGN |
| Room width/height 15 | CONFIRMED serialized `RGRoomX` |
| `b_point` floor extent 14..26 | CONFIRMED serialized prefab |
| Native 13×13 interior AABB | NATIVE DESIGN / approximation |
| Exact wall gaps and door collision | UNKNOWN / PARTIAL |
| Idle/run clip IDs and four-frame mappings | CONFIRMED/HIGH CONFIDENCE |
| Idle/run selection rule | NATIVE DESIGN |
| Camera follow smoothing 8.0 | NATIVE DESIGN / reference UNKNOWN |

## 7. Verification

The native executable was driven with real SDL keyboard events through
`/dev/uinput` (`ydotool`) against the focused Wayland window. Debug logs showed:

```text
pos=(20.000,20.000) input=(0.00,0.00) state=ide
pos=(21.560,20.000) input=(1.00,0.00) state=run
pos=(26.250,20.000) input=(1.00,0.00) state=run blocked=(true,false)
pos=(26.250,26.250) input=(0.71,0.71) state=run blocked=(true,true)
pos=(26.250,26.250) input=(0.00,0.00) state=ide
```

The player remained at the boundary, changed from run to idle on key release, and
the camera followed to the boundary. Temporary screenshots are not committed.
