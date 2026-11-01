# Living Room / Hub

## Default scene

The Living Room is one recovered `HeroRoom` scene (`level2`) using its default
active hierarchy. Inactive event tilemaps and event-token objects are excluded.
The current audit reports zero baked event instances.

## Removed in 0.5.4

- `room/objects/cellar` and the cellar trigger/lock/obstacle group: removes the
  staircase beside the recycling bin and the cellar interaction.
- `room/objects/first_arrow`: tutorial arrow.
- `room/objects/npc_trainer`: Drillmaster.
- `room/objects/npc_sell_limit`: the NPC opposite Drillmaster.
- `room/objects/promotion`: promotion pair above the recycling bin.
- `room/objects/magic_ball`: floating orb.
- `room/objects/magic_protal1/2`: lower portal visuals.
- `room/objects/npc_bossrush_mgr`: boss-rush sign/guardian.
- Every `*/tab` child using `biaoqing_16`: the floating `(!)` markers.
- `room_tilemap/floor_cellar`, `object_cellar` and portal light objects.

The physical floor under the removed cellar cells remains the default lobby
floor; no placeholder geometry was added.

## Collision

Wall tile cells provide room boundaries. Non-trigger Box/Circle colliders are
extracted per prop. The central recovered table collider is explicitly audited.
The hub topology report currently shows:

```text
reachable from spawn: 2,296 / 2,409 walkable cells (95.3%)
collision solids:     732
event instances:      0
requested removals:   0
central table:        collision present
```

Markers: character station, forge/workshop, garden, orb trader and dungeon door
are all reachable. The inactive `hireroom` area is not treated as main-floor
geometry.

## Interactions

- Character station: character selector.
- Forge/workshop station: weapon selector; full material/crafting system remains
  pending (see `MATERIALS_INVENTORY.md`).
- Garden station: plant plots.
- Orb trader: three actual weapon offers, priced by serialized `item_value`.
- Dungeon door: room transition.
- `O`: developer +100 orbs while in the hub (documented developer control).

## Remaining

- Full workshop material/recipe UI.
- NPC speech, pets, punching bag.
- Door to the workshop remains a direct physical opening; the reference door
  open/close state is not yet reconstructed.
- The central brown lower floor is default scene geometry, not a removed cellar
  replacement.
