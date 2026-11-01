# Native Map / Minimap

**Status:** compact minimap implemented from the recovered `UIMap` construction;
no enlarged map.

## Reconstruction

The reference `level2` Canvas contains `map_info` (200x200, top-right). Its
children are:

```text
map_info
  a/       connector quads (a0_5 ... a23_24)
  room/    room00 ... room44 (24x24 cells on a 35 px pitch)
  target   current-position marker (ui_4, 19x19)
  Text     level label
```

The room cells and connectors have Image components with **no sprite**; they are
plain tinted quads. `UIMap` serializes `color_0`, `color_1`, `color_2` and five
special icons:

| UIMap field | Recovered sprite |
|---|---|
| `boss_logo` | `ui_14` |
| `special_logo` | `ui_114` |
| `chest_logo` | `ui_113` |
| `home_logo` | `ui_125` (green house) |
| `cage_logo` | `ui_193` |
| target marker | `ui_4` |

`SetMiniIcon` selects the icon by room type; `UpdateMap` applies the three
recovered gray/white cell colors. The reference HUD screenshot confirms a small
dark panel, not a fullscreen map.

## Native implementation

- Panel: fixed compact `minimap` HUD element (240x196 by default).
- Cells: plain colored quads using the recovered values:
  current white, visited light gray, unexplored dark gray.
- Connectors: plain light-gray quads; the previous native blue lines are gone.
- Special icons: the actual `ui_14`, `ui_113`, `ui_114`, `ui_125` and `ui_193`
  sprites are used when the room type is known.
- Current room: `ui_4` marker plus a thin gold current-room border.
- Devroom: one normal-size cell with a centered white `D`; this is the only
  intentionally native icon.
- Integrated rooms: the Living Room is `home` (green house). The seeded battle
  rooms currently have no boss/special/chest/cage type metadata, so they remain
  plain cells; the icon code path is implemented for when that metadata exists.

There is no `M` fullscreen overlay. The previous oversized map was removed.

## Graph

The native test graph currently contains:

```text
hero_room -- first -- second
                 \-- third
```

The seed maps actual room templates onto `first/second/third`; the graph is not
a reconstruction of `MapManagerLevel.CreateMap`. See
`docs/development/ROOM_TRANSITIONS.md`.

## Evidence boundary

`CONFIRMED`: the `UIMap` hierarchy, 24x24/35 px cell construction, unsprite
cell/connector quads, `color_0/1/2`, the five special-icon sprite references,
`ui_4` target marker, and the compact reference HUD composition.

`HYPOTHESIS`: mapping the native room graph's IDs onto the reference 5x5 cell
grid and the exact desktop minimap anchor. These are native desktop decisions,
not recovered `MapManagerLevel` positions.

## 0.5.3 hub subarea nodes

The Living Room remains one recovered `HeroRoom` scene. For exploration, the
minimap additionally shows native conceptual nodes for the physically accessible
garden, workshop/research and cellar. They are marked visited and linked to the
home cell; they are not claimed to be separate `MapManagerLevel` rooms. The
white `D` cell remains reserved for the developer room.
