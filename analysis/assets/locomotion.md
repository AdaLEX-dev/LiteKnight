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

### Recovered reference path

`c00` uses Unity Physics2D:

```text
root:
  Rigidbody2D mass=1, gravityScale=0, constraints=4 (freeze rotation),
              collisionDetection=1 (continuous)
  BoxCollider2D size=(0.5,0.5), offset=(0,0.7), isTrigger=true
child "collider":
  CircleCollider2D radius=0.4, offset=(0,0.5), isTrigger=false,
                    layer=8 (Body)
```

`RGController.SetVelocity` writes `Rigidbody2D.velocity`; the reference player is
moved by Unity's physics solver, not by a custom transform/raycast mover. The
root box is the interaction trigger; the child circle is the physical collider.

`TagManager`/`Physics2DSettings` confirm:

```text
Body=8, Obstacle=12, Wall=13, Door=14, Edge=17
Body collides with Obstacle, Wall, Door and Edge.
```

The recovered `r1_1` room uses:

```text
room_x_offset = room_y_offset = (41 - 15) / 2 = 13
wall rows:    y = 12 and y = 28
wall columns: x = 12 and x = 28
door gaps:    five cells 18..22 on every side
wall prefab:  1x1 BoxCollider2D, offset (0,0.5), i.e. [x-0.5,x+0.5] x [y,y+1]
wall layer:   prefab serialized 13 (Wall); generated instances 17 (Edge)
interior:     [12.5,13.0] .. [27.5,28.0]
```

`RGRoomX.CreateWall` always creates four five-cell gaps. `RGRoomX.CreateAisle`
either builds an open door/corridor (`entrance=1`) or disables the door and fills
the gap with five wall prefabs (`entrance=0`). The serialized `r1_1` entrance is
`[0,0,0,0]`; `MapManagerLevel` overwrites it from `room_relation` for actual map
instances. The native test scene opens all four sides as an explicit test
configuration.

Serialized doors have 5x1 or 1x5 non-trigger barriers and 5x1.5 or 1.5x5 trigger
volumes. The Animator open clip sets the barrier `isTrigger=true`; the closed
clip restores the full solid barrier. Closed doors also have a
`PlatformEffector2D` one-way normal facing into the room, so they block exiting
but allow entry from the aisle.

### Native design

The native runtime consumes `analysis/re/r1_1_wall_map.json` through
`r1_1_visual.json`:

- 44 room wall cells and 96 aisle corridor wall cells become static AABBs;
- four recovered door barrier AABBs are added as **one-way** solids while a
  door is closed (south negative Y, north positive Y, west negative X, east
  positive X);
- open doors contribute no barrier and their leaf sprites are hidden;
- the 41x41 map-cell edge is a clearly labeled native test boundary.

The player collision footprint is the enclosing square of the reference
`CircleCollider2D`:

```text
half extents = (0.4, 0.4), offset = (0, 0.5) from the feet anchor
```

This is an explicit **APPROXIMATION** of the circle/Unity solver. Collision
behavior preserves:

- X/Y axis separation and wall sliding;
- swept checks against thin wall cells;
- feet-anchored player position;
- deterministic 50 Hz movement;
- idle/run animation and camera follow.

Door animation transition timing is not simulated natively; the debug toggle
switches final open/closed states instantaneously. That timing remains an
approximation. Full Physics2D parity is not claimed.

Recovered evidence and the machine-readable map:
`analysis/re/room_topology.md`, `analysis/re/r1_1_wall_map.json`.

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
| `CreateWall` wall rows/columns and 18..22 gaps | CONFIRMED disassembly + serialized offsets |
| Wall prefab 1×1 collider offset (0,0.5) | CONFIRMED serialized prefab |
| Door barrier/trigger rectangles and path IDs | CONFIRMED serialized data |
| Door open/closed collider states | CONFIRMED streamed animation clips |
| Closed-door one-way inward traversal | HIGH CONFIDENCE effector geometry + matrix |
| Native room/corridor collision solids | CONFIRMED geometry consumed by native code |
| Native player AABB vs circle collider | APPROXIMATION |
| Door transition timing in native code | APPROXIMATION (instant final-state switch) |
| Exact entrance bits of runtime `r1_1` | UNKNOWN; test scene uses all-open |
| Event-to-map-manager traversal listener | UNKNOWN |
| Idle/run clip IDs and four-frame mappings | CONFIRMED/HIGH CONFIDENCE |
| Idle/run selection rule | NATIVE DESIGN |
| Camera follow smoothing 8.0 | NATIVE DESIGN / reference UNKNOWN |

## 7. Verification

The native executable was driven with real SDL keyboard events through
`/dev/uinput` (`ydotool`) against the focused Wayland window. The recovered
topology run rendered 531 instances / 15 sprite assets / 3 textures and logged:

```text
Collision: bounds [0.00,0.00]..[41.00,41.00], recovered solids=140, doors=4, topology=true
Room interior: [12.50,13.00]..[27.50,28.00]
Render batch: 471 sprite instances   (all four doors initially open -> leaves hidden)
Door state: all recovered doors closed (solid, leaves visible); closed barriers=4
pos=(15.919,12.900) input=(0.00,-1.00) blocked=(false,true)   // solid perimeter/door barrier
Door trigger south: enter aisle=aisle_s open=false active=false
Door state: all recovered doors open (passable, leaves hidden); closed barriers=0
Door trigger south: exit aisle=aisle_s open=true active=true
pos=(18.844,8.415) input=(0.00,-1.00)                          // passed through open south opening
Door trigger east: enter aisle=aisle_e open=true active=true
Door trigger east: exit aisle=aisle_e open=true active=true
pos=(31.895,20.000) input=(1.00,0.00)                          // east corridor traversal
```

The player remained blocked at the closed barrier, passed through the same
opening after the door opened, generated door enter/exit events only while
`door_open` was true, and camera follow remained active. The injected key stream
was noisy (some keys remained held between runs), so these excerpts are evidence
of the state transitions rather than a clean speed measurement. Unit tests cover
all four doors in both directions deterministically.
