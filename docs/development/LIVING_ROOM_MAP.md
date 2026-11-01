# Living Room Map Recovery

## Evidence

- `UIMap.Start` (`0xCC4C4C`) calls `ShowMap()` only when
  `BattleData.data.levelIndex != 0`.
- Entering HeroRoom resets `BattleData` and leaves `levelIndex = 0`.
- The Living Room's runtime map manager is the empty `map_A0` prefab:
  `level=""`, `map_long=0`, and empty start/end/chest/special/floor/wall/
  obstacle/enemy lists.
- Garden, workshop, trader, cellar and the other hub areas are ordinary
  HeroRoom scene objects/triggers, not `RGRoomX` map nodes.
- `UIMap` is serialized in HeroRoom but its Animator defaults to the hidden
  state (`show=false`; children inactive; panel parked at x=200).
- `UIMap.GetMinimapSprite` maps room type 0 to `home_logo` (`ui_125`), type 3
  to chest/special icons, and other room types to no icon.
- The Player's own hub duplicate is not a map node and is excluded from hub
  character markers by the existing occupancy logic.

## Native implementation

1. No minimap is drawn while `scene.is_hub`.
2. The hub is excluded from the dungeon map's room cells and connectors.
3. Dungeon maps use only the generated `room_relation` cells from the plan.
4. Start room type 0 uses the recovered `ui_125` home icon.
5. Transfer-gate/end room type 2 has no invented boss icon.
6. Chest/special rooms keep the recovered `ui_113`/`ui_114` icons.
7. No conceptual hub subarea nodes and no `D` marker are added.

This follows the reference: the Living Room has no multi-area minimap, and the
hub is not part of the dungeon map.
