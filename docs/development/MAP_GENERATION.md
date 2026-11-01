# Map Generation and Corridors

## Reference data recovered

`MapManagerLevel` serialized configs were found in the reference bundles:

```text
levels discovered:                18
levels with enemy pools:          16
levels with start-room lists:     17
```

For each level the catalogue records:

- `level` string (`1-1`, `1-2`, ..., `3-6`, `BR 3-5`)
- `chest_level`
- `map_long` (1, 3 or 4 in recovered levels)
- `room_start_list`, `room_end_list`, `branch_room_list`
- `floor_list`, `wall_list`, `obstacle_list`
- weighted `enemy_infos` (`enemy`, `exEnemy`, `weight`)

Example `1-1` (CONFIRMED): `chest_level=1`, `map_long=3`,
`room_start_list=[r_start_no_event]`, `room_end_list=[r_transgete]`,
four weighted enemies `e_miner01/02/03` weight 1 and `e_yeti01` weight 2.
See `analysis/assets/map_catalogue.json`.

## Native generation

`DungeonPlan` uses the reconstructed Unity 2018.4 `RGRandomStream` to shuffle
the supplied recovered room templates. The current graph is a deterministic
L/chain layout; `map_long=3` matches the current three dungeon rooms.

Each graph edge names source and destination doors. The runtime:

1. configures the room’s four recovered physical doors,
2. seals any door with no graph edge,
3. only transitions when the door trigger is entered and the map edge exists,
4. runs a 4-neighbour reachability audit over the Living Room.

This guarantees that a map edge and a traversable physical door cannot disagree.

## Current limitation

`MapManagerLevel.CreateRoom` procedural placement is not fully reconstructed.
The native map does not yet generate arbitrary room counts, branch rooms,
boss/shop/chest room categories or per-room floor/wall/obstacle selections from
the level config. That is a Tier C reconstruction task, documented rather than
hidden behind a larger rectangle or teleport.
