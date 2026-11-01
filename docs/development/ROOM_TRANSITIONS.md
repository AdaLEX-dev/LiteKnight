# Room Transitions and Native Map Graph

**Status:** three-room deterministic native dungeon graph implemented with the
recovered Living Room (`hero_room`) and recovered PatternRooms `r1_1`, `r1_2`,
`r1_3`.

## Loaded world

At startup the runtime locates the four actual scene JSON files, loads every
room and uploads the recovered textures once. The active room remains `scene_`;
inactive rooms are stored in `stored_rooms_` and swapped on transition, so
texture storage is stable and no room is a renamed copy of another.

The graph uses a fixed physical topology:

```text
          second
            |
  hub ---- first ---- third
```

Doors are the recovered five-cell room doors. Edges are directional in the
data model and therefore resolve both the source door and the matching
destination entrance:

```text
hub:hub_door          <-> first:south
first:north           <-> second:south
first:east            <-> third:west
```

`--map-seed <int>` deterministically assigns the actual room templates to the
first/second/third slots using the reconstructed Unity 2018.4 `RGRandomStream`.
The same seed produces the same graph; changing the seed can reorder which
actual recovered room occupies each slot. The compact HUD minimap draws these
cells and physical links; the current room is marked with a gold frame and the
current devroom cell carries a white `D` marker.

## Physical transition

Walking into an open door trigger invokes `RoomManager::TransitionThroughDoor`.
The runtime swaps the target stored scene into the active slot and places the
player at the opposite entrance door inset from its trigger. `T` is a developer
shortcut that selects the first connected door. The Living Room marker also
transitions on walk-through.

The transition preserves:

- character selection and animation clip set
- both weapon slots / active slot
- HP, armor, energy and dead state
- per-room enemy snapshots, including dead enemies
- per-room clear state and open doors on revisit
- current map seed and graph identity

## Evidence boundary

`CONFIRMED`: `r1_1`, `r1_2`, `r1_3` are actual PatternRoom prefabs with unique
serialized obstacle/topology data. The room catalogue is
`analysis/assets/room_catalogue.json` (108 discovered rooms, 3 currently
integrated).

`HYPOTHESIS`: the native three-room topology, room-template assignment and
entrance placement. These make the actual rooms physically traversable without
claiming the unrecovered reference map algorithm. The reference
`MapManagerLevel.CreateMap`/`CreateRoom`, room weights, special-room placement
and full 108-room runtime catalogue remain **UNKNOWN**. See
`docs/PROCEDURAL_GENERATION.md`.
