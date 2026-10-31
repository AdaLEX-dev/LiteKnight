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
  door is blocking (south negative Y, north positive Y, west negative X, east
  positive X);
- open doors contribute no barrier; their animated leaf sprites remain in the
  recovered open pose (`door03`), while the open clip hides leaf shadows and the
  `d` ornaments;
- the 49 `RGObjectSkin` markers are replaced by `wall603` obstacle visuals and
  49 static AABBs, matching `RGRoomX.CreateRoomObstacle`;
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

Door animation is now simulated natively: `Door`/`RoomDoorController` replay
the recovered `door_open` (0.2 s) and `door_close`/`door_close2` (0.1667 s)
clips. The leaf sprite switches, ornament local y, shadow/ornament visibility,
and `m_IsTrigger` collision timing come from the serialized streams. Native
collision keeps the full barrier during the non-trigger interval instead of
replaying the transient `m_Size` thinning; that is the remaining explicit
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
| Door transition timing in native code | RECOVERED CLIPS; linear/step replay |
| Transient door `m_Size` animation | APPROXIMATION (full barrier) |
| Exact entrance bits of runtime `r1_1` | UNKNOWN; test scene uses all-open |
| Event-to-map-manager traversal listener | UNKNOWN |
| Idle/run clip IDs and four-frame mappings | CONFIRMED/HIGH CONFIDENCE |
| Idle/run selection rule | NATIVE DESIGN |
| Camera follow smoothing 8.0 | NATIVE DESIGN / reference UNKNOWN |

## 7. Verification

The native executable was driven with real SDL keyboard events through
`/dev/uinput` (`ydotool`) against the focused Wayland window. The current
topology run renders 482 static instances (plus 147 active obstacle sprites) / 156 sprite assets / 12 textures and logs:

```text
Collision: bounds [0.00,0.00]..[41.00,41.00], recovered solids=140, obstacle cells=49, doors=4, topology=true
Room interior: [12.50,13.00]..[27.50,28.00]
Render batch: 589 sprite instances   (open clip hides 40 door shadows/ornaments)

locomotion: sim=(20.000,15.000) render=(20.000,15.000)
            body=(20.000,15.000) shadow=(20.000,14.950) camera=(20.000,15.000)

// C: CloseDoor() on all four doors
Door lifecycle: CloseDoor() for 4 doors
  door north: state=CLOSING blocking=true t=0.000/0.167

// O: OpenDoor() on all four doors
Door lifecycle: OpenDoor() (animation started, 4 doors)
Door north: state=OPENING blocking=false t=0.140   // recovered 0.1333 s key
```

The current verification uses the closed-door one-way semantics: open doors are
passable in both directions; once closed, the player can enter from the aisle but
cannot leave the room through another closed door. The native test spawn is
`(20,15)` because `(20,20)` is inside the recovered wall603 obstacle block.
Reference door events are suppressed while `door_open` is false (debug logs show
`active=false`), matching the `RGDoorTrigger` gate. Unit tests cover all four
doors in both directions deterministically, the animation/collision timing, and
the player render-sync invariant; the real-input runs confirm the key path and
the observable collision/trigger behavior.

## 8. Eight-way movement, facing and dev-room controls (2026-10-31)

### Movement

`ReadDigitalMovement` produces the normalized eight-way vectors:

```text
W       ( 0, +1)        S       ( 0, -1)
A       (-1,  0)        D       (+1,  0)
W+A     (-1, +1)/√2     W+D     (+1, +1)/√2
S+A     (-1, -1)/√2     S+D     (+1, -1)/√2
```

W+S cancels Y; A+D cancels X. `PlayerLocomotion` applies the normalized vector
at the recovered effective speed 9.75 in the fixed 0.02 s tick, with no added
acceleration. Cardinal and diagonal displacement magnitudes are therefore equal
for the same number of ticks. Unit tests cover all eight directions, both
cancellation cases, velocity magnitude, displacement magnitude, and the player
render invariant.

### Facing and directional animation

The recovered data does **not** contain up/down/left/right/diagonal run clips.
`char_0_skin_0` contains `ide`, `run`, `dead` and `char_hit`; `RGController`
represents facing as an integer ±1 and applies `Quaternion.Euler(0,180,0)` to
the `img` root for left facing (identity for right). In 2D this is a horizontal
mirror; the shadow is a sibling and is not mirrored. See
`analysis/assets/animation.md` §7 for the disassembly evidence.

Native implementation:

```text
movement direction  -> PlayerLocomotion (independent)
horizontal X sign   -> FacingDirection::Left/Right
pure vertical input -> retains previous horizontal facing
body render         -> flip_x when facing left
shadow render       -> never flipped
idle/run selection  -> movement presence only
```

Confidence: **HIGH CONFIDENCE** for horizontal-only facing; **UNKNOWN** for any
additional directional animation states outside the inspected controllers.

### Dev-room controls

```text
1..9, 0        select recovered character c00..c09
[ / ]          cycle all 15 recovered character prefabs c00..c14
P              cycle the central obstacle among the 8 recoverable solid
               MapManagerLevel.obstacle_list variants (12 entries in metadata)
O              toggle OpenDoor()/CloseDoor()
C              CloseDoor() on all four doors
```

Character switching changes only the active body sprite/animation clips; the
player world position, collision footprint, and camera follow state are
untouched. Obstacle switching regenerates the same 49 marker cells from the
selected recovered prefab and rebuilds collision from that variant. Display
names remain CANDIDATE; the mechanism uses recovered asset IDs.

### Clean dev room

The previous upper-right stray geometry was caused by the builder applying the
`wall601` prefab **asset** root local position `(20,20)` as a sprite offset.
`RGRoomX.CreateWall` overrides that root position with the wall-cell coordinate
when instantiating. The builder now ignores the root asset position; child
offsets remain. The regression test asserts all 420 wall instances sit at their
`@x,y` cell and that no scene instance lies outside the 41×41 map cell.
