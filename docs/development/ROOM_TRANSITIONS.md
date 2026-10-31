# Room Transitions

**Status:** two-room state-preserving transition implemented between the
recovered Living Room (`hero_room`) and the recovered `r1_1` gameplay room.

## Implementation

At startup the primary scene and one alternate scene are loaded. Textures from
both are uploaded once to the Vulkan renderer. `SwitchRoom` swaps the active
scene and reconfigures:

- player character set, animation clips and speed/stats
- weapon registry, two equipment slots and active weapon
- HP, armor, energy and dead state
- doors, obstacles, enemies, projectiles and collision world
- player render binding, camera and spawn position

This preserves character, equipment, HP, armor and energy across the transition.
The alternate scene is used as the return destination, so `T` or an interaction
with the hub door/open recovered door toggles rooms.

## Evidence boundary

`CONFIRMED`: both scenes and their room geometry exist in the local reference.
`HYPOTHESIS`: native doorway/entrance points and the exact open-door interaction
radius. The original `EnterDungeonRoomEvent`/`ExitDungeonRoomEvent` listener
chain and procedural dungeon room relations remain **UNKNOWN**. This is not
procedural generation and does not claim a full dungeon map.

No arbitrary teleport-to-coordinate is used: the transition switches actual
loaded scenes and preserves state.
