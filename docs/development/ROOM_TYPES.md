# Room Types and Stage Metadata

**Status:** all 108 PatternRoom prefabs are catalogued; the three integrated
rooms carry room type and stage labels, and the minimap consumes the metadata.
Only normal battle rooms are present in the current native graph.

## PatternRoom catalogue

`analysis/assets/room_catalogue.json` records:

```text
PatternRooms discovered: 108
Rooms integrated:          3  (r1_1, r1_2, r1_3)
Room sizes:  15x15 (49), 15x21 (23), 21x15 (22), 21x21 (14)
RGRoomX room_type: 1 for all 108  (battle/normal integer; enum-name mapping HYPOTHESIS)
```

Each integrated room was re-extracted with its own floor/wall/door/obstacle
assets. The game does not use one r1_1 texture set for every room.

## Room metadata

`RGRoomX` serializes `room_type`; the integrated rooms are type `1`:

| Scene | RGRoomX type | Type name | Stage label | Clear label |
|---|---:|---|---|---|
| `r1_1_visual` | 1 | battle | `1-1` | `CLEARED` |
| `r1_2_visual` | 1 | battle | `1-2` | `CLEARED` |
| `r1_3_visual` | 1 | battle | `1-3` | `CLEARED` |
| `hero_room` | -1 (native hub marker) | hub | `LIVING ROOM` | — |

`SceneAsset::RoomMetadata` carries type, type name, stage, number, label and
clear label. The HUD consumes it; it does not hard-code `1-1` or `CLEARED` in
gameplay code.

The `1-N` labels are derived from the actual PatternRoom id (`r1_N`) and stage 1.
They are not claimed to be the canonical MapManagerLevel progression.

## UI behaviour

- On entering a room, the stage label is shown in the HUD and a fade/slide title
  appears for 3 s.
- When every enemy is defeated, the title switches to the configured clear label
  and the HUD appends it.
- The reference text clip/atlas for this banner has not been identified yet;
  the native fade/slide is `HYPOTHESIS`, while the text data is scene-driven.

## Minimap icons

`MapRoomView` consumes room type:

| Type | Icon |
|---|---|
| hub/home | `ui_125` |
| boss | `ui_14` |
| shop/special | `ui_114` |
| chest | `ui_113` |
| cage | `ui_193` |
| normal battle | plain cell |
| devroom | native white `D` |

The integrated r1 rooms are all normal battle, so they correctly show plain
cells. The Living Room hub shows home plus native hub subarea nodes for the
physically accessible garden/workshop/cellar.

Special start/exit/boss/shop rooms are referenced by `MapManagerLevel`
`room_start_list`/`room_end_list`, not by the 108 PatternRoom prefabs. The native
HUD labels dungeon rooms by BFS visitation order from the generated start room, using the
recovered level-1 prefix: `1-1`, `1-2`, `1-3`. This is documented as a HYPOTHESIS
for exact MapManagerLevel progression.

## Remaining work

- Classify the remaining 105 PatternRooms by type.
- Recover the canonical MapManagerLevel stage/floor mapping.
- Recover the reference room-title/clear animation clip and text asset.
- Integrate more actual room scenes with their own assets.
