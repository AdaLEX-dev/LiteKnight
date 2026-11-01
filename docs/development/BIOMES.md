# Biomes and Stage Progression

## Recovered levels

`MapManagerLevel` provides 18 level configs:

```text
1-1 .. 1-5     (5)
2-1 .. 2-5     (5)
3-1 .. 3-6     (6)
BR 3-5         (1)
(unnamed 0)    (1)
```

`chest_level` increases from 1 in 1-1 to 4 in 3-5/3-6. `map_long` is 3 for most
normal levels, 4 for 1-5/2-5/3-5, and 1 for boss-rush/special configs.

Enemy pools are recorded per level in `analysis/assets/map_catalogue.json`.
They are the authoritative source for biome enemy eligibility. `1-1` uses
`e_miner01/02/03` and `e_yeti01`; later levels add spiders, yetis, knights,
frankenstein, frogs, aliens and others.

## Native state

- Integrated biome: biome 1 only (`r1_*` room patterns).
- Runtime room code derives from BFS visitation order from the generated start room,
  using the recovered `1` prefix; `1-1`, `1-2`, `1-3` are shown in the HUD.
- Biome-specific floor/wall/obstacle lists are catalogued but not yet selected
  by a full native map generator.
- Enemy definitions and pools are catalogued independently; only `e_slime01` is
  currently combat-integrated.

## Confidence

- `CONFIRMED`: level strings, chest levels, map_long, enemy weights, room
  start/end lists, floor/wall/obstacle prefab names.
- `HYPOTHESIS`: room-code BFS mapping and the native 3-room shape.
- `UNKNOWN`: full `CreateRoom` adjacency/branch placement for every seed.
