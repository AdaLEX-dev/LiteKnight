# Living Room / HeroRoom

**Status:** playable native hub recovered from the local reference `level2`
(HeroRoom) serialized scene.

## Source and extraction

`tools/asset/build_hub_scene.py`:

1. reconstructs local `sharedassets*.assets` from the APK's `.splitN` chunks;
2. loads `assets/bin/Data/level2` together with shared assets;
3. walks the recovered transform hierarchy;
4. exports active SpriteRenderers, 15 tilemaps (about 3,896 placed tiles),
   sprite textures/metadata and 91 solid Box/Circle/Polygon collider bounds;
5. adds a native floor fill for unrecoverable tile gaps;
6. writes `analysis/assets/scenes/hero_room.json`,
   `analysis/assets/scenes/hero_room_manifest.json` and local-only pixels under
   `assets/generated/visual/hero_room/`.

All recovered pixels remain `LOCAL_ONLY`, `REFERENCE_DERIVED`,
`NOT_FOR_REDISTRIBUTION`.

## Playable features

- Recovered floor/wall/garden tiles and props.
- Solid furniture/wall/machine collision.
- Player movement, camera, character animation and HUD.
- Character station: `E` near the station, or `H`; opens the 15-character
  selector with stats and real body sprites.
- Weapon station: `E` near forge, or `B`; opens the recovered weapon selector.
- Garden plots: four deterministic development plots with
  `Empty -> Growing -> Ready -> harvest`; growth values/economy are
  `UNRECOVERED` placeholders.
- Hub door: `E` at the door or `T` to switch to recovered `r1_1`; the same
  transition returns to the hub.

## Status vocabulary

`CONFIRMED`: HeroRoom scene hierarchy, tilemaps, SpriteRenderers and collider
components exist in the local reference.
`HYPOTHESIS`: spawn point, interaction marker positions, garden plot coordinates
and development growth timing are native choices for this slice.
`UNKNOWN`: production character names, dialogue, forge recipes, plant economy,
NPC schedules and all hidden progression rules.
