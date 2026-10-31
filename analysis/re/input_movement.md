# Input and Player Movement Reconstruction

**Scope:** minimum behavioural contract for the `r1_1` vertical slice. No native
implementation is included.

---

## 1. Serialized input axes

`globalgamemanagers` contains the Unity `InputManager` object. The relevant
`InputAxis` entries are:

| Name | Type | Axis | Positive | Negative | Dead | Sensitivity |
|---|---|---:|---|---|---:|---:|
| Horizontal | Key/Mouse button | 0 | `right` | `left` | 0.001 | 3.0 |
| Vertical | Key/Mouse button | 0 | `up` | `down` | 0.001 | 3.0 |
| BtnA | Key/Mouse button | 0 | `j`, alt `joystick button 0` | — | 0.001 | 1000 |
| BtnB | Key/Mouse button | 0 | `i`, alt `joystick button 1` | — | 0.001 | 1000 |
| BtnX | Key/Mouse button | 0 | `o`, alt `joystick button 2` | — | 0.001 | 1000 |
| BtnY | Key/Mouse button | 0 | `u`, alt `joystick button 3` | — | 0.001 | 1000 |
| BtnL | Key/Mouse button | 0 | `q`, alt `joystick button 4` | — | 0.001 | 1000 |
| BtnR | Key/Mouse button | 0 | `e`, alt `joystick button 5` | — | 0.001 | 1000 |
| BtnR2 | Key/Mouse button | 0 | `joystick button 7` | — | 0.001 | 1000 |
| Horizontal | Joystick axis | 0 | — | — | 0.19 | 1.0 |
| Vertical | Joystick axis | 1 | — | — | 0.19 | 1.0, inverted |
| HorizontalR | Joystick axis | 3 | — | — | 0.19 | 1.0 |
| VerticalR | Joystick axis | 4 | — | — | 0.19 | 1.0, inverted |
| Submit | Key/Mouse button | 0 | `enter`, alt `space` | — | — | — |
| Cancel | Key/Mouse button | 0 | `escape` | — | — | — |

The input abstraction is Unity's legacy `Input.GetAxis` / `Input.GetKey`, not the
new Input System (`PlayerSettings.enableNewInputSystem` is false/None and
`disableOldInputManagerSupport` is false).

## 2. `RGJoyControl` path

`RGJoyControl` (`dump.cs` TypeDefIndex 7658) has:

- `controller` at `+0x20`
- `net_controller` at `+0x28`
- `joy_R` at `+0x30`
- `joystickInputRight` of type `MovementInput` at `+0x4C`
- `Update` at `0xDD7E84`
- `GetKeyboardMovementInput` at `0xDD8618`
- `GetJoystickMovementInput` at `0xDD8718`
- `OnAtkBtnClick` at `0xDD7788`
- `OnSkillBtnClick` at `0xDD7918`
- `OnWeaponBtnClick` at `0xDD7C78`

`MovementInput` (`dump.cs` TypeDefIndex 7657):

```text
Vector2 dir;    // +0x0
float weight;   // +0x8
int facing;     // property
```

`GetKeyboardMovementInput` reads raw key codes:

- `119` = `w` → +Y
- `115` = `s` → -Y
- `100` = `d` → +X
- `97`  = `a` → -X

It constructs a `Vector2`, normalizes it, and sets `weight = 1.0`.

`GetJoystickMovementInput` reads `Input.GetAxis("Horizontal")` and
`Input.GetAxis("Vertical")`, subtracts a center offset, multiplies the magnitude
by 1.5, clamps weight to 1.0, and returns the axis vector.

## 3. Consumption: `RGController.SetVelocity`

`C01Controller.FixedUpdate` (`0x12AA200`) tail-calls the virtual method at vtable
`+0x278`. With the actual `Il2CppClass` vtable base of `0x128` and 16-byte
`VirtualInvokeData`, this is slot 21: `RGController.SetVelocity` (`0x117A6E4`).

Observed `SetVelocity` logic:

```text
if (!awake) { rigidbody.velocity = Vector2.zero; return; }

if (speed_field_0x48 > 1.0)
    rigidbody.velocity = normalize(move_dir_0x4C) * speed_field_0x48;
else
    rigidbody.velocity = (input_dir_0x110, input_weight_0xd8)
                          * (attr.speed * (attr.speed_rate + 1.0) + speed_bonus_0xCC);
```

The exact field labels are structural; the important contract is:

- movement is expressed through `Rigidbody2D.velocity`,
- direction is normalized for the high-speed branch,
- speed derives from `RoleAttributePlayer.speed` (`+0x1C`) and
  `RoleAttributePlayer.speed_rate` (`+0x20`), plus a controller bonus at `+0xCC`,
- `RoleAttributePlayer` is attached at `RGController+0x70`,
- rigidbody is at `RGController+0x28`,
- movement input direction is at `+0x110/+0x114`, weight at `+0xD8`,
- fixed-update, not Update, drives velocity.

`RoleAttribute` fields (`dump.cs` TypeDefIndex 6910):

```text
int camp;        // +0x18
float speed;     // +0x1C
float speed_rate;// +0x20
int max_hp;      // +0x24
int hp;          // +0x28
float deviation; // +0x2C
int critical;    // +0x30
```

## 4. Concrete player prefab evidence

`common` bundle path `Assets/RGPrefab/Player/c00.prefab` contains a GameObject
`c00` with:

- `Rigidbody2D`: body type 0 (dynamic), gravity scale 0, constraints 4 (freeze
  rotation), mass 1, collision detection 1
- `BoxCollider2D`: size 0.5×0.5, offset (0, 0.7), `isTrigger = true`
- `C01Controller`
- `RoleAttributePlayer`: speed 6.5, speed_rate 0.5, hp/max_hp 6, armor/max_armor 5,
  energy 180, atk 4, critical 5, camp 1, c_index 0
- `RGCharGray`

`Assets/RGPrefab/Player/c01.prefab` uses `C02Controller` with speed 6.5,
speed_rate 0.5, hp 5, armor 3, energy 180, atk 5, critical 10, c_index 1.

**Identity status:** the prefab/controller/stat data is CONFIRMED. The display
names "Knight" and "Rogue" are **CANDIDATE**, not proven by a serialized name in
the prefab. Use `c00/C01Controller` as the technical player for the first slice and
do not claim the display name.

## 5. Action → consumer contract

| Action | Reference source | Consumer | State change |
|---|---|---|---|
| Move | WASD / `Horizontal`,`Vertical` axes / joystick | `RGJoyControl` → `MovementInput` → `RGController.RoleMove` | desired direction/weight; `SetVelocity` sets `Rigidbody2D.velocity` in FixedUpdate |
| Attack press | `BtnA` (`j`, joystick 0) | `RGJoyControl.OnAtkBtnClick` → `RGController.BtnAtkClick` (`0x117C3DC`) | attack state; exact weapon path unresolved |
| Skill press | `BtnB`? (`i`, joystick 1) | `OnSkillBtnClick` → `RGController.BtnSkillDown/Up` | skill state |
| Weapon switch | `BtnL/R` (`q`/`e`, joystick 4/5) | `OnWeaponBtnClick` → `SwitchWeapon` | active hand/weapon |
| Interact | not in this axis table | scene/interaction UI | unresolved |
| Pause | UI/back input | `UIWindowPause` | pause state |

The exact button mapping between `BtnA/B/X/Y` and attack/skill/dodge is partly
inferred from the method names and must be verified against the UI prefab or a
runtime trace before calling it parity.

## 6. Confidence

| Claim | Confidence | Evidence |
|---|---|---|
| Legacy Unity input axes and WASD mapping | CONFIRMED | serialized `InputManager` |
| `MovementInput {dir, weight}` | CONFIRMED | metadata + `RGJoyControl` disassembly |
| Keyboard/joystick movement path | HIGH CONFIDENCE | `0xDD8618`, `0xDD8718`, `0xDD7E84` |
| Movement uses `Rigidbody2D.velocity` in FixedUpdate | CONFIRMED | `C01Controller.FixedUpdate` → slot 21 → `0x117A6E4` |
| c00/C01Controller stats | CONFIRMED | serialized prefab components |
| Display-name identity (Knight/Rogue) | CANDIDATE | prefab names/indices only |
| Exact attack/skill/dodge button semantics | PARTIAL | method names + axis table; no full call trace |

## 9. Native locomotion slice (2026-10-31)

The first native implementation is in:

- `src/platform/input/MovementInput.hpp` — keyboard-only normalized intent.
- `src/engine/locomotion/PlayerLocomotion.cpp` — fixed-step movement and
  collision resolution.
- `src/engine/collision/AabbCollision.cpp` — swept axis-separated AABB walls.
- `src/engine/animation/LocomotionAnimator.cpp` — idle/run selection.
- `src/engine/rendering/Camera2D.cpp` — native follow smoothing.

What is recovered and what is native:

| Item | Status |
|---|---|
| WASD key mapping, normalized `MovementInput` | CONFIRMED reference path; native keyboard implementation |
| c00 `speed = 6.5`, `speed_rate = 0.5` | CONFIRMED serialized values |
| `SetVelocity` low-inertial formula | HIGH CONFIDENCE; native effective speed `9.75` |
| Fixed 0.02 s simulation consumption | CONFIRMED reference timing; native loop |
| `CreateWall` 44 room wall cells, four five-cell gaps | CONFIRMED disassembly + serialized offsets |
| `CreateAisleWall` 96 corridor wall cells | CONFIRMED disassembly |
| Door barriers/triggers and one-way closed state | CONFIRMED serialized colliders + streamed clips/effector |
| Player physical collider CircleCollider2D r=0.4 offset (0,0.5) | CONFIRMED serialized c00 child collider |
| Native enclosing AABB (0.8×0.8, offset 0.5) | APPROXIMATION of the circle/Unity Physics2D solver |
| 50 Hz idle/run state switching | NATIVE DESIGN |
| Camera follow smoothing `8.0` | NATIVE DESIGN; reference values UNKNOWN |

The input snapshot is sampled once per render frame and reused for all fixed
steps in that frame. Gamepad axis input remains scaffolded but is not consumed by
the locomotion slice.
