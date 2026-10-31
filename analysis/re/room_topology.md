# `room/r1_1` room topology, doors and traversal

**Status:** recovered geometry CONFIRMED; native wall/door/aisle collision,
animated door lifecycle, obstacle replacement and door traversal are FUNCTIONAL;
runtime entrance assignment and the higher-level traversal listener remain
PARTIAL/UNKNOWN. This document describes the serialized
`room/r1_1` prefab and the `RGRoomX`/`RGAisle`/`RGDoor` code paths. `r1_1` is a
reverse-engineering test reconstruction, **not** a proven first level, starting
room, character selector, or canonical gameplay map.

**Evidence sources**

- Bundle `room/r1_1` (SHA-256
  `cef33f8731952a957b34ab602f940d3bb2a94c98243d6083861d58be5d50a142`).
- `MapManagerLevel` level `1-1`, bundle `level-objects`, path ID
  `-8892723654509893235`.
- `libil2cpp.so` RVA `0x00DEBE3C` (`RGRoomX.CreateWall`), `0x00DEB474`
  (`RGRoomX.CreateAisle`), `0x00DEE7B0` (`RGRoomX.OnSetRGRandomSeed`),
  `0x00DEA7CC` (`RGRoomX.LoadRoom`), `0x01103468` (`RGDoor`),
  `0x01103F44` (`RGDoorTrigger`), `0x00EEE260` (`RGAisle.CreateDoor`),
  `0x00EEEE48` (`RGAisle.CreateAisleWall`), `0x00EEF6FC`
  (`RGAisle.CreateWall`).
- Global `TagManager` and `Physics2DSettings`.
- Machine-readable map: `analysis/re/r1_1_wall_map.json` and
  `analysis/re/r1_1_wall_map.txt`.

---

## 1. Room dimensions and placement

`RGRoomX` serialized fields on the `r1_1` root:

```text
room_width  = 15
room_height = 15
room_type   = 1
entrance    = [0, 0, 0, 0]
door_open   = 1
room_x_offset = 0
room_y_offset = 0
create_with_close_state = 0
need_cage = 0
```

**CONFIRMED:** `MapManagerLevel.<CreateRoom>d__35.MoveNext` places each room at
`(vx * 41, vy * 41)`. `RGRoomX.OnSetRGRandomSeed` computes:

```text
room_x_offset = (41 - room_width)  / 2 = 13
room_y_offset = (41 - room_height) / 2 = 13
```

Therefore the test reconstruction occupies one 41×41 map cell and has:

```text
room wall lines:      x = 12 and x = 28; y = 12 and y = 28
interior (collision): [12.5, 13.0] .. [27.5, 28.0]
interior cell centers: x = 13..27, y = 13..27
```

The serialized `room_x_offset/room_y_offset = 0` are prefab-template defaults;
the runtime seed path overwrites them with 13/13 for this 15×15 room.

**CONFIRMED:** the serialized room's `entrance` array order is
`[east, south, west, north]`. `MapManagerLevel` overwrites each bit from
`room_relation` when the room is instantiated. The serialized `[0,0,0,0]` is
therefore **not** proof that the gameplay instance had no exits.

**UNKNOWN:** which entrance configuration any particular runtime `r1_1` instance
received. No generated `r1_1` instance was found outside the template bundle.
The native test scene opens all four sides and labels this a test configuration.

---

## 2. `RGRoomX.CreateWall` reconstruction

### 2.1 Normal-room branch

For a normal `RGRoomX` (the only branch relevant to `r1_1`):

```text
gap_min = 17
gap_max = 23

for x = room_x_offset - 1 .. room_x_offset + room_width:
    if x > 17 and x < 23:
        continue                       // five-cell door gap: x = 18..22
    instantiate wall at (x, room_y_offset - 1)
    instantiate wall at (x, room_y_offset + room_height)

for y = room_y_offset .. room_y_offset + room_height - 1:
    if y > 17 and y < 23:
        continue                       // five-cell door gap: y = 18..22
    instantiate wall at (room_x_offset - 1, y)
    instantiate wall at (room_x_offset + room_width, y)
```

Each instantiated wall is chosen independently from
`MapManagerLevel.wall_list`:

```text
RGRandom.Range(0, 100) < 30  -> wall_list[0]  (30%)
otherwise                    -> wall_list[1]  (70%)
```

The wall GameObject is parented to `RGRoomX.wall_group`; its local position is
set to the integer cell coordinate. Except during sandbox editing, each wall
GameObject is assigned `RGRoomX.EdgeLayer`, which `RGRoomX.cctor` initializes to
17 (`Edge`). The prefab asset itself is serialized on layer 13 (`Wall`); the
runtime assignment to 17 is explicit in `CreateWall`/`CreateAisleWall`.

For `r1_1` this produces:

```text
44 solid room-wall cells after all four openings are open
64 cells in the full unbroken 1-cell-thick perimeter ring
four five-cell gaps: south and north rows at x=18..22,
                     west and east columns at y=18..22
```

The wall cells are the integer centers `(x,y)`; the collider geometry is
described in section 3.

### 2.2 Subclass special case

The beginning of `CreateWall` tests a room subclass and changes the two gap
constants from `17/23` to `-3/3` for that subclass. The exact target class was
not resolved from the metadata pointer. **UNKNOWN** for the subclass branch;
**CONFIRMED** that normal `r1_1` takes the `17/23` branch.

### 2.3 What happens when all four sides are closed

`RGRoomX.CreateWall` always creates the four gaps. `RGRoomX.CreateAisle` then
decides whether each side is an open passage or a sealed wall:

- `entrance[side] == 1` → `RGAisle.CreateAisle()`; the door is active and the
  corridor is built.
- `entrance[side] == 0` → `RGAisle.CreateWall()`; the door GameObject is
  disabled and five wall prefabs are instantiated across the five-cell gap.

Therefore the serialized `r1_1` with `entrance=[0,0,0,0]` would be fully sealed
at runtime: four five-cell wall fillers replace the four door barriers and the
door objects are inactive. This is **CONFIRMED** from `RGAisle.CreateWall`
disassembly and the serialized template. The native test scene uses a labeled
all-open configuration instead, so the door/aisle geometry can be traversed.

---

## 3. Wall prefab and collider

`MapManagerLevel.wall_list` for the recovered level-1-1 manager has two
prefabs, `wall601` and `wall602`. Both have identical collision geometry:

```text
serialized prefab layer: 13 (Wall)
runtime instance layer:  17 (Edge; RGRoomX.EdgeLayer)
GameObject tag:          Wall (serialized tag value 20002)
Transform scale:         1
BoxCollider2D:
  size   = (1.0, 1.0)
  offset = (0.0, 0.5)
  isTrigger = false
  enabled   = true
  usedByEffector = false
```

A prefab transform at integer cell `(x,y)` therefore has a collider spanning:

```text
[x - 0.5, x + 0.5] × [y, y + 1]
```

`wall601` visual children (the 1×1 collider is only on the root):

| object | sprite | sorting layer | order |
|---|---|---|---|
| `wall601` root | `w601` | Wall (4) | 0 |
| `shadow` | `shadow0` | Shadow (3) | 0 |
| `w2` | `w602` | WallFont (6) | 2 |

`wall602` uses `w603`/`w604` with the same layers. The variant choice is random
per cell and its seed is not recovered; the native reconstruction renders one
recovered variant deterministically and labels that visual choice as native
design. Collision is variant-independent.

---

## 4. Aisle corridors

`RGAisle.CreateAisleWall` builds the corridor outside an open room side. For a
15×15 room in a 41×41 cell, each side contributes 24 corridor wall cells:

| side | corridor wall cells | free corridor cells |
|---|---|---|
| south | `x = 17, 23; y = 0..11` | `x = 18..22; y = 0..11` |
| north | `x = 17, 23; y = 29..40` | `x = 18..22; y = 29..40` |
| west | `x = 0..11; y = 17, 23` | `x = 0..11; y = 18..22` |
| east | `x = 29..40; y = 17, 23` | `x = 29..40; y = 18..22` |

The free corridor is five units wide and extends to the edge of the 41-unit map
cell. The adjacent room contributes the mirrored half; the full connection is
therefore a 5-wide corridor. Corridor walls use the same `wall_list` prefabs and
therefore have the same 1×1 collider geometry.

The room's door opening and the corridor's free cells align:

```text
south door barrier:  [17.5,12.0] .. [22.5,13.0]
north door barrier:  [17.5,28.0] .. [22.5,29.0]
west door barrier:   [11.5,18.0] .. [12.5,23.0]
east door barrier:   [27.5,18.0] .. [28.5,23.0]
```

**CONFIRMED** from `RGAisle.CreateAisleWall` coordinates and the serialized door
colliders. Exact `CreateFloor` tile-pattern randomness was not reconstructed;
floor pixels are non-solid and do not affect collision.

---

## 5. The four serialized doors

All four door transforms have identity rotation and scale. The serialized leaf
positions correspond to the **closed** animation pose. Door root layer 14
(`Door`), tag `Wall` (20002); each root has an Animator, `RGDoor`, a
non-trigger `BoxCollider2D` used by a `PlatformEffector2D`, and a child
`bullet_collider`. The trigger is a child `door_trigger` with `RGDoorTrigger`.

| Door | GameObject path | path ID | position | collider size/offset | trigger size/offset | trigger path ID | aisle | RGAisle dir | event dir |
|---|---|---:|---|---|---|---|---|---|---|
| South | `/r1_1/aisle_s/door` | `6745575534016544133` | `(20,12)` | `(5,1)` / `(0,0.5)` | `(5,1.5)` / `(0,2.15)` | `-1687673530676222468` | `aisle_s` | 1 | 2 |
| North | `/r1_1/aisle_n/door` | `-7004038979866159705` | `(20,28)` | `(5,1)` / `(0,0.5)` | `(5,1.5)` / `(0,-1.15)` | `-6895341147321044750` | `aisle_n` | 3 | 1 |
| West | `/r1_1/aisle_w/door` | `-3118800413403885553` | `(12,20)` | `(1,5)` / `(0,0.5)` | `(1.5,5)` / `(1.65,0.5)` | `-6556733487589223018` | `aisle_w` | 2 | 3 |
| East | `/r1_1/aisle_e/door` | `-2170177141768453367` | `(28,20)` | `(1,5)` / `(0,0.5)` | `(1.5,5)` / `(-1.65,0.5)` | `-6975871405190372524` | `aisle_e` | 0 | 4 |

World-space barrier and trigger rectangles:

```text
south barrier: [17.5,12.0] .. [22.5,13.0]   trigger: [17.5,13.4] .. [22.5,14.9]
north barrier: [17.5,28.0] .. [22.5,29.0]   trigger: [17.5,26.1] .. [22.5,27.6]
west  barrier: [11.5,18.0] .. [12.5,23.0]   trigger: [12.9,18.0] .. [14.4,23.0]
east  barrier: [27.5,18.0] .. [28.5,23.0]   trigger: [25.6,18.0] .. [27.1,23.0]
```

| door | RGDoor component | trigger MonoBehaviour | Animator controller | clips |
|---|---:|---:|---|---|
| South | `-1641236803404413679` | `5396034433124824349` | `door_2` / `2736239371532244996` | `door_open`, `door_close2` |
| North | `-2419543365132411998` | `4492376270453062121` | `door_2` / `2736239371532244996` | `door_open`, `door_close2` |
| West | `7656297055103991887` | `2076029364540924421` | `door_1` / `143467717326297140` | `door_open`, `door_close` |
| East | `8114088698945216285` | `-2381660460226313568` | `door_1` / `143467717326297140` | `door_open`, `door_close` |

Each door has five `d1..d5` child clusters; each cluster contains a `door02`
leaf, a `shadow0` shadow, and a `d` child with `door01`. The serialized
transforms are the closed pose; the open animation retracts the leaves.

`bullet_collider` is a separate child collider on layer 13 used by projectiles;
it is not the player door barrier.

Serialized script fields: `RGDoor.the_room` is null in the prefab and is resolved
in `Awake` from the door's grandparent; `RGDoorTrigger.the_room` is null in the
prefab and resolved in `Start` from the trigger's third parent. `RGDoor` resolves
its `Animator` in `Awake`. The door trigger GameObject is layer 0/tag 0 with a
non-solid trigger collider; the door root is layer 14 (`Door`) / tag `Wall`
(20002).

---

## 6. Door triggers

`RGDoorTrigger.Start` resolves:

```text
the_room = transform.parent.parent.parent.GetComponent<RGRoomX>()
```

`RGDoorTrigger.OnTriggerEnter2D(other)`:

1. If `BattleData.data.IsSandboxEditing`, return.
2. If `other.gameObject` tag is not `Body_P` and not `Body_P_stealth`, return.
3. If `the_room.door_open == false`, return.
4. If `the_room.process == 0`, execute the multiplayer room-start message path.
5. `UICanvas.GetInstance().UpdateMap()`.
6. Resolve `RGAisle` from the trigger's parent chain.
7. If an aisle exists, call `OnEnterDoorEvent(aisle.name)`.
8. Create a `RoomStartEvent` with `player_pos = other.transform.position` and
   raise it through `SimpleEventManager`.

`OnEnterDoorEvent` maps the aisle GameObject name to `emRoomDoorDir` and raises:

```text
EnterDungeonRoomEvent {
  roomName = the_room.name,
  enter_dir = 1 (north) / 2 (south) / 3 (west) / 4 (east)
}
```

`RGDoor.OnTriggerExit2D(other)` uses the same `Body_P`/`Body_P_stealth` and
`door_open` gates. If active, it computes `player_pos` as the other collider's
world position plus collider offset and raises:

```text
ExitDungeonRoomEvent {
  roomName = the_room.name,
  exit_dir = 1 (north) / 2 (south) / 3 (west) / 4 (east)
}
```

The exit direction is chosen by comparing the player position to the door
position on the door's perpendicular axis. `RGDoor`/`RGDoorTrigger` do not
teleport the player and contain no destination-room reference.

**CONFIRMED** component types, serialized trigger geometry, tag gates,
`door_open` gate, event payloads, aisle-name string comparisons, and direction
mapping from disassembly. **UNKNOWN:** the exact listener(s) for
`EnterDungeonRoomEvent`/`ExitDungeonRoomEvent`; no direct static xref from those
events to `MapManagerLevel.IntoAisle` was found.

---

## 7. `door_open`: exact semantics

`door_open` is the room's logical door-open state, not merely an initial visual
flag.

- `RGRoomX.CloseDoor`:
  - calls `CloseDoor()` on all four `RGDoor` references (`door_n`, `door_e`,
    `door_w`, `door_s`);
  - sets `door_open = false`.
- `RGRoomX.OpenDoor`:
  - calls `OpenDoor()` on all four `RGDoor` references;
  - sets `door_open = true`.
- `RGRoomX.Start` calls the door-close path only when
  `create_with_close_state` is true. `r1_1` serializes it false.
- `RGRoomX.StartRoom` sets `process = 1` and invokes the door-close path when
  the room is a battle room. `RGRoomX.ClearRoom` sets `process = 2`, invokes the
  corresponding open path, and raises `ClearRoomEvent`. **HIGH CONFIDENCE** from
  the room lifecycle call structure; the exact virtual slot number is not
  asserted here.
- Door triggers are inert while `door_open == false`.
- Serialized `r1_1.door_open = 1`; the four serialized door Animators also
  default the `open` bool to true and default to the `door_open` state.

`RGDoor` animation semantics (**CONFIRMED** by decoding the streamed clips):

| clip | duration / rate | recovered keys |
|---|---|---|
| `door_open` | 0.2 s, 30 Hz, non-loop | t=0: leaf `door02`, `d` local y=1, leaf/shadows/`d` enabled, `m_IsTrigger=false`; t=0.133333: `d` y=0.1, shadows disabled, `m_IsTrigger=true`; t=0.166667: `d` renderers disabled, leaf sprite `door03`; end 0.2 |
| `door_close` (E/W) | 0.166667 s, 30 Hz, non-loop | t=0: leaf `door03`, `d` y=0.1, shadows/`d` disabled, `m_Size.x=0.2`, `m_IsTrigger=false`; t=0.033333: shadows/`d` enabled, leaf `door02`; t=0.166667: `d` y=1, `m_Size.x=1` |
| `door_close2` (N/S) | 0.166667 s, 30 Hz, non-loop | same visibility/sprite/leaf keys; t=0 sets `m_Size.y=1`; t=0.166667 sets `d` y=1 |

All five `d1..d5` clusters share the same transform/visibility/sprite curves.
`m_IsTrigger` stays false for the entire close clip, so a closing door is solid
from the first frame. The ornament child `dN/d` is the only moving transform;
the door root, leaf roots, shadow position and collider centre stay fixed.

The closed door root also has a `PlatformEffector2D`:

```text
UseOneWay     = true
SurfaceArc    = 180°
RotationalOffset: south 0°, east 90°, north 180°, west 270°
```

The one-way top face points into the room. Therefore a **closed** door blocks
the player from leaving the room but permits entry from the aisle. This is
**HIGH CONFIDENCE** from the serialized effector rotations plus Unity
`PlatformEffector2D` one-way semantics. When the room is cleared, `OpenDoor`
makes the collider a trigger, so both directions are passable.

Native implementation: `game/room/Door` exposes distinct `OPEN`, `CLOSING`,
`CLOSED`, `OPENING` states; `RoomDoorController` feeds collision from the
`m_IsTrigger` curve. Continuous ornament motion is linearly interpolated between
the serialized keys; `m_Enabled`, sprite PPtr and trigger values use step/hold
sampling. The native collision uses the full serialized barrier for every
non-trigger interval instead of the transient E/W `m_Size.x=0.2` thinning; this
is an explicit **APPROXIMATION** because the thin collider still spans the
doorway line. The recovered timing itself is reproduced at the 50 Hz fixed tick.

### 7.1 `RGObjectSkin` obstacle replacement

`RGRoomX.OnSetRGRandomSeed` calls `RGRoomX.CreateRoomObstacle` after `LoadRoom`.
That method collects every active child `RGObjectSkin`, instantiates
`MapManagerLevel.obstacle_list[obj_index]` at the marker's world position with
identity rotation, parents it under the marker's parent, and destroys the
marker. For `r1_1` all 49 central `wall_group/skin_wall` markers serialize
`obj_index=0`; level 1-1 `obstacle_list[0]` is `wall603`, a 1×1 solid
`BoxCollider2D` at offset `(0,0.5)` with root/shadow/wall-font sprites. The
native scene therefore removes the placeholder `skin` sprites and emits 49
`wall603` visual instances plus 49 separate obstacle AABBs. The c00 prefab
origin `(20,20)` lies inside that recovered obstacle block, so the native test
spawn is explicitly moved to `(20,15)`, just inside the south door on free
floor. This spawn is a **NATIVE TEST CONFIGURATION**, not recovered gameplay
state.

---

## 8. Room traversal behavior

The reference map is a 41-unit grid of rooms. `MapManagerLevel.<CreateRoom>`
instantiates a room prefab and sets its entrance bits from `room_relation`:

```text
west  neighbor relation > 0 and |diff| <= 1 -> entrance[2] = 1
south neighbor relation > 0 and |diff| <= 1 -> entrance[1] = 1
east  neighbor relation > 0 and |diff| <= 1 -> entrance[0] = 1
north neighbor relation > 0 and |diff| <= 1 -> entrance[3] = 1
```

Then `RGRoomX.OnSetRGRandomSeed` runs `LoadRoom`:

```text
CreateAisle()  -> open corridor/door or sealed five-cell wall
CreateFloor()
CreateWall()   -> perimeter with four five-cell gaps
populate floor_list from b_point children
CreateRoomObstacle()
CreateRoomSpecialObject()
EnemyMaker.SetRGRandomSeed(...)
```

`MapManagerLevel.IntoAisle(direction)` updates the manager's current room
coordinates:

```text
0 east  -> this_room_x += 1
1 south -> this_room_y -= 1
2 west  -> this_room_x -= 1
3 north -> this_room_y += 1
```

No direct static call site to `IntoAisle` was found; the event-to-manager
listener wiring is **UNKNOWN**. What is confirmed is:

- traversal is physical; there is no teleport in `RGDoor`, `RGDoorTrigger`, or
  `RGRoomX`;
- the aisle corridor is 5 units wide and spans the 41-unit map cell edge;
- door triggers report the current room name and direction through events;
- `r1_1` itself contains no adjacent-room or destination data; the map manager
  owns that.

For the native test reconstruction only one 41×41 map cell is built. The outer
cell edge is a clearly labeled **NATIVE TEST BOUNDARY**; adjacent rooms and the
map-manager transition listener are outside this milestone.

---

## 9. Player collision path

`c00` serialized components:

```text
root:
  Rigidbody2D mass=1, gravityScale=0, constraints=4 (freeze rotation),
              collisionDetection=1 (continuous)
  BoxCollider2D size=(0.5,0.5), offset=(0,0.7), isTrigger=true
child "collider":
  CircleCollider2D radius=0.4, offset=(0,0.5), isTrigger=false,
                    layer=8 (Body)
```

`RGController.SetVelocity` writes `Rigidbody2D.velocity`; the reference player
therefore uses Unity Physics2D with the circle collider, not a custom
raycast/transform-collision path. TagManager layers:

```text
Body = 8, Obstacle = 12, Wall = 13, Door = 14, Edge = 17
Physics2D matrix: Body collides with Obstacle, Wall, Door and Edge.
```

The native runtime still uses a deterministic swept AABB, preserving the
feet-anchored transform and wall sliding. It now consumes recovered room,
aisle and one-way door geometry. The AABB footprint is the enclosing square of
the reference circle (`0.8×0.8`, offset `(0,0.5)`), which is an explicit
**APPROXIMATION** of the circle/physics solver; full Physics2D parity is not
claimed.

---

## 10. Native topology consumed by the runtime

`analysis/assets/scenes/r1_1_visual.json` now carries:

```text
collision.bounds          = [0, 0, 41, 41]       (native map-cell test boundary)
collision.interior_bounds = [12.5, 13, 15, 15]   (recovered room interior)
collision.solids          = 140 wall-cell AABBs  (44 room + 96 corridor)
collision.doors[4]        = recovered barrier/trigger AABBs,
                            directions, path IDs, one-way normals, open state,
                            open/close animation keyframes
obstacles.markers         = 49 RGObjectSkin replacement positions
obstacles.variants[12]    = recovered obstacle_list prefabs, visuals and AABBs
topology.wall_cells       = 44
topology.corridor_wall_cells = 96
topology.floor_cells      = 225  (15x15 interior cell centers)
topology.aisle_cells      = 240  (four 5x12 open corridor runs)
topology.room_center      = [20.0, 20.5]
topology.wall_prefabs     = wall601/wall602, 1x1 collider offset (0,0.5)
topology.obstacle_prefabs = level-1-1 obstacle_list[0..11]
topology.obstacle_markers = 49 RGObjectSkin replacements (all -> wall603)
topology.openings         = four serialized doors/aisles
instances                 = 482: 420 wall sprites + 60 door sprites +
                            2 player sprites; obstacle sprites are generated
                            from the active variant (147 for wall603)
```

The native test configuration is:

```text
entrance  = all four sides open
door_open = true
spawn     = (20, 15)  // just inside the south door; (20,20) is inside wall603
```

This is a **NATIVE TEST CONFIGURATION**, not a recovered runtime instance state.
The serialized template has `entrance=[0,0,0,0]`. Press **O** in scene mode to
toggle all four doors closed/open for manual verification; press **C** to issue
`CloseDoor()` for all four. Closed doors use the recovered one-way inward
barrier, animated leaves, and solid collision. The doors animate over their
recovered clip durations rather than switching state instantly.

The dark strips below/around wall rows are the serialized `wall601/602`
`shadow0` children (sorting layer `Shadow`, color alpha 0.392), not missing wall
collision. Generated wall visuals are one deterministic variant (`wall601`) per
collision cell; the reference randomizes `wall601`/`wall602`, which share the
same 1×1 collider. The 49 central `wall603` obstacles are the recovered runtime
replacement for the `RGObjectSkin` markers, not additional room-wall cells.

### 10.1 Native reconstruction artifact: upper-right wall sprites (fixed)

The original native builder treated each wall prefab's **root** local position
as a sprite offset. The `wall601` prefab asset serializes a root local position
of `(20,20)` in its source bundle; `RGRoomX.CreateWall` overrides that root
position when instantiating at the wall-cell coordinate. The builder therefore
placed 140 root wall sprites at `cell + (20,20)`, producing the unexplained
upper-right geometry that extended beyond the room. The shadow and WallFont
children were already correct because their prefab-local positions are zero.

Fix: wall prefab collection ignores the root local position and keeps only
child-relative offsets; the wall-cell coordinate is authoritative. The
regression tests in `tests/unit/room_visual_test.cpp` assert every
`native_wall/...@x,y` instance position equals `(x,y)` and that no scene instance
lies outside the recovered 41×41 map cell. This is an artifact of the native
reconstruction, not part of the recovered Unity room.

### 10.2 Dev obstacle variant switching

`analysis/assets/scenes/r1_1_visual.json` carries the recovered
`MapManagerLevel.obstacle_list` variants and the 49 `RGObjectSkin` marker
positions. The native dev room starts with `obstacle_list[0]` (`wall603`) and
allows cycling the 8 solid recovered variants at runtime. Rendering derives every
obstacle sprite from the active variant's visual records; collision derives one
AABB per marker from the same variant, so rendered and physical obstacle cells
stay synchronized. The four decorative entries without a BoxCollider2D
(`sting01`, `speed_up`, `speed_down`, `signs01`) remain in metadata but are not
selectable. This switching mechanism is a **NATIVE TEST FEATURE**, not recovered
gameplay behavior.

---

## 11. Unresolved questions

1. Exact event listener and call path from `EnterDungeonRoomEvent` /
   `ExitDungeonRoomEvent` to `MapManagerLevel`/room-coordinate updates.
2. Entrance bits of any actual generated `r1_1` instance.
3. Target class and behavior of the `CreateWall` subclass branch (`-3/3` gap
   constants).
4. Exact `CreateFloor` tile pattern and floor-prefab random choices.
5. Full Unity Physics2D solver parity: circle-vs-box contacts, continuous
   detection details, platform-effector contact filtering, trigger ordering.
6. Whether `r1_1` is instantiated in a normal generated map or used only as a
   test/asset template. No external instance was found.
7. Native door collision uses the full serialized barrier during the non-trigger
   interval instead of replaying the transient E/W `m_Size.x` thinning.
8. Native animation replays the recovered keyframes linearly; Unity's streamed
   cubic coefficients are not evaluated, and sprite/m_Enabled/trigger keys use
   discrete hold sampling.
