# Native Map / Minimap

**Status:** minimap and full map overlay implemented for the native room graph.

## Rendering

The HUD keeps a small map in the logical `minimap` layout element. `M` opens the
larger overlay in the `map_panel` element. Both use the same `MapRoomView` data:
room grid position, current flag, visited flag and devroom flag. Links come from
`RoomManager`'s physical connections. Visited rooms are blue; the current room
has the gold frame and a white position pixel.

The devroom cell displays a white `D` on its map cell. This is a dedicated
developer marker; it is not a claimed reference room icon.

## Graph

The native test graph currently contains:

```text
hero_room -- first -- second
                 \-- third
```

The seed maps actual room templates onto `first/second/third`; the graph is not a
reconstruction of `MapManagerLevel.CreateMap`. See
`docs/development/ROOM_TRANSITIONS.md`.

## Input and simulation

`M` toggles the map. The map is an overlay that freezes the 50 Hz simulation and
clears movement/attack input until closed. `ESC` also closes it. These bindings
are native desktop bindings; the reference mobile map button mapping is not a
keyboard contract.

## Evidence boundary

`CONFIRMED`: room IDs, actual scene connections, current/visited state, and the
requested white-D developer marker.

`HYPOTHESIS`: minimap cell layout, 26 px gap choices, default panel position and
the exact reference map open/close key. The reference `UIMap` visual atlas
mapping for the full room-icon family remains **UNKNOWN**.
