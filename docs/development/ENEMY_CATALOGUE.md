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

The generic extractor now emits **101 enemy definitions** (from the 102 prefabs;
4 projectile/non-enemy prefabs lack RoleAttribute and remain catalogue-only). Of those,
**71 have recovered ranged bullet data**. Each normal dungeon room spawns three
weighted enemies from the current biome pool; `e_miner01/02` carry recovered child `ESword01`
`atk`/bullet data rather than a guessed damage value. The runtime supports
generic chase, contact damage and ranged enemy projectiles; player projectiles
and enemy projectiles use the same recovered bullet definitions.

## Biome pools

The `1-1` pool is `e_miner01/02/03` (weight 1) and `e_yeti01` (weight 2). Later
levels add their own recovered pools. The native `DungeonPlan` does not yet
select enemies from these weights.

## Next implementation

1. Extract body/dead/shadow sprites and attack clips per enemy.
2. Implement ranged AI classes (projectile spawn, cooldown, target search).
3. Add weighted enemy selection from the current level’s `enemy_infos`.
4. Add boss classification and boss reward/weapon drops.
