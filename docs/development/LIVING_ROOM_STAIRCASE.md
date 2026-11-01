# Living Room Staircase Repair

The removed object was `room/objects/cellar` at world `(-10.5, -9.5)`, with
the stair strips on `room_tilemap/floor_cellar` and the old collision
`cellar_lock_obstacle`/`cellar_obstacle`. It remains removed.

## Recovered replacement

The surrounding default south-wall line in `room_tilemap/wall` uses
`wall_u` (`tiles` atlas, path ID 2093, rect `347,495,16,16`, pivot `.5,.5`,
PPU 15.8) as the top cap and `wall_m` (path ID 2098, rect `347,478,16,16`) as
the wall body, on Unity sorting layer `Wall` (4), order 20. The native hub
places those exact recovered tiles in the old 3×3 opening:

```text
cx -101..-99, cy -69  -> wall_u
cx -101..-99, cy -70  -> wall_m
cx -101..-99, cy -71  -> wall_m
```

This is the same construction/scale/layer as the neighboring Living Room wall
line. No staircase or cellar sprite is recreated. The matching collision solid
is `[-12,-12,-9,-9]`, so the player cannot enter the old shaft.

The generic floor-gap fill was also corrected to use the default `tiles_0`
(path 938) crop rather than the removed cellar/stair sprite or the whole atlas.
