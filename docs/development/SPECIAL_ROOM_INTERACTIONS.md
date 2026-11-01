# Special Room Interactions

## Recovered systems

| Room | System | Recovered behavior | Status |
|---|---|---|---|
| `r_statue` | `TalkStatue` | 9 statue prefabs, base price 15, player/ExpertShop adjustment, first E confirms and second E buys, `buff_statue_<index>` spawn, `BattleData.SetStatus` / addition statues | **DATA/RUNTIME NOT WIRED** |
| `r_mineral` | `EnemyAI05` minerals | 40 HP, 10 pickups: gold drops 10×`coin_2` (value 1), crystal drops 10×`energy` (value 8) | **DATA/RUNTIME NOT WIRED** |
| `r_mineral` | water dispenser | `ItemWaterDispenser`: free heal 1–4, mount creature heal, no stock | **DATA/RUNTIME NOT WIRED** |
| `r_mineral` | bar/waiter | 8 drinks with weights/prices 20–30; two-stage trigger | **DATA/RUNTIME NOT WIRED** |
| `r_mercenary` | hiring/trainer | `npc_01..07` weights, 15-coin hire, MAX_LEVEL 5, trainer cost 10/20/40/80 | **DATA/RUNTIME NOT WIRED** |
| `r_mount` | mount seller | creature/mech pools, 15/30 sell price, update price 15 | **DATA/RUNTIME NOT WIRED** |

Exact fields, addresses and pools are recorded in the recovery report and in
`analysis/assets/chests/chest_catalogue.md`.

## Why they are not wired

The native special-room scenes currently contain only the recovered room
geometry, walls and doors. They do **not** contain the random `enemy_group`
prefab selection (`statue_01..09`, minerals/water/bar, NPCs, mount sellers), and
the builder does not emit their sprites or interaction markers. Adding a
generic interaction marker without those scene objects would be an invented
layout, not the reference room.

This is a concrete data-pipeline blocker, not an UNKNOWN behavior:

1. Extract the `r_statue`, `r_mineral`, `r_mercenary`, `r_mount` random
   `enemy_group` variants and their child SpriteRenderers/colliders.
2. Emit `InteractionMarker` records with the recovered class identity, price,
   stock and position.
3. Add the purchase/state transitions (statue status, mineral pickups, water
   heal, waiter drink, mercenary hire/train, mount sell/update) and persistence.
4. Add deterministic tests per system.

The exact 108 PatternRoom/level/chest pipeline remains unaffected.

## Tests

No tests claim these systems are implemented. The audit keeps them explicitly
unwired until the room objects and markers are extracted; the existing tests
cover the shared shop/chest and currency state only.
