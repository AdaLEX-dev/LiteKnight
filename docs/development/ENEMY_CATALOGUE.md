# Enemy Catalogue

`tools/asset/audit_enemy_catalogue.py` scans every reference bundle for `e_*`
prefabs with `RoleAttribute` and/or an `EnemyAI*` controller.

## Counts

```text
Enemy GameObjects:       102
RoleAttribute present:   102
Controller present:      102
Biome-assigned:           77
```

Each entry records prefab id, source CAB, HP, speed, camp, critical, AI class,
enemy_id, reward data, shoot cooldown, animation clip names and the
`MapManagerLevel.enemy_infos` biome levels/weights. See
`analysis/assets/enemies/enemy_catalogue.json`.

## Runtime state

Only `e_slime01` is currently spawned by the native room fixtures. The catalogue
is not a substitute for AI integration: the runtime only has the recovered
contact-damage chase behaviour. `EnemyAI02/10/...` fields that do not serialize a
contact damage value are marked `UNKNOWN` rather than assigned a made-up value.

## Biome pools

The `1-1` pool is `e_miner01/02/03` (weight 1) and `e_yeti01` (weight 2). Later
levels add their own recovered pools. The native `DungeonPlan` does not yet
select enemies from these weights.

## Next implementation

1. Extract body/dead/shadow sprites and attack clips per enemy.
2. Implement ranged AI classes (projectile spawn, cooldown, target search).
3. Add weighted enemy selection from the current level’s `enemy_infos`.
4. Add boss classification and boss reward/weapon drops.
