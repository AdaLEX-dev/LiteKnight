# Living Room and Dungeon Bug-Fix Campaign (0.5.5-dev)

This pass treats the playable result as the acceptance criterion. It does not
add a new architecture, tag, or release.

## Fixed in this pass

### Living Room boundary and geometry
- Sealed the former `room_tilemap/object_portal` gap immediately right of the
  mech spawner (`x=48..50, y=9..10`). The cells are physical solids and keep the
  recovered wall visuals.
- Removed the global rectangular `hub/floor_tile_fill` and `hub/floor_fill`
  quads. The garden now follows the recovered `floor_garden` tile silhouette and
  tree line; only enclosed holes inside a recovered footprint are filled.
- Generated thin boundary solids from the union of recovered floor cells, so
  the garden/hub perimeter follows the real tile topology instead of a box.
- Added the missing recovered wall body/cap behind the reforging furnace
  (`hub_spr_2098`/`hub_spr_2093`) and collision from every approach direction.
- Added collision for recovered static trigger volumes (planter beds, forge,
  mech, material chest, weapon station, seller containers, boards) while
  leaving NPC talk and door/portal transition volumes non-solid.
- Removed the three decorative notice-board signs (`hub_spr_2241`), the hire
  board and the task board signboards from the HeroRoom scene together with
  their orphan solids. The bounty officer NPC is retained; only signboard
  objects were removed.
- Removed the Living Room HUD stage caption; dungeon rooms keep the recovered
  stage caption.

### Material chest and trader
- The material chest is the recovered static `chest_material` prefab at
  `(48.91, 1.72)`. The runtime generic `chest03` overlay was removed.
- The Living Room seller keeps its recovered `room_seller` transform and is
  sealed behind the recovered bookshelf/stall collider. Its three display cases
  show the actual weapon sprites; a purchased offer leaves its case empty.
- The reconstructed `r_sell` room now renders the recovered seller NPC, shadow,
  counter and display-case sprites. The weapon offer uses its real weapon visual
  sprite instead of an empty container.

### NPCs, mech station, suggestions
- Living Room NPC bodies sample their recovered `<character>_idle` clips.
  Dialogue cycles through every recovered `CharacterN_talk*` term, faces the
  player within the interaction radius, and never auto-chatters.
- The Robotics Facility (`m_mech_0..5`) is a real station. Its names and
  `itemValue/defence/speedRate/unlock_key` fields come from the recovered mount
  prefabs. Unlocked mechs cost their serialized orb value; blueprint-gated
  mechs remain explicitly locked because the extracted APK does not contain the
  `MechInfos` Resources material table.
- Settings now contain persistent `Reduced Weapon FX` (default subtle) and
  `Living Room QA` toggles. Suggestion mode is hub-only, supports type
  selection, rectangle placement, list, delete, clear and export to
  `analysis/hub_suggestions.json`; it cannot be enabled from dungeon gameplay.

### Collision, depth, scale and camera
- `WorldDepth` now documents a foot-based rule for characters and ground props:
  `world Y / sprite foot -> deterministic render depth`. Tilemaps, lights and UI
  keep static sorting. Elevated props use their sprite foot, not the transform
  centre.
- Static props with recovered ground footprints are depth-sorted in the
  character band; the player body remains tied to its simulated feet.
- The player collider remains a 0.80 x 0.80 logical footprint offset from the
  feet; the sprite may extend above it.
- Camera follow now targets the character torso and clamps to recovered room /
  floor bounds. Rooms smaller than the viewport are centred, not scrolled into
  neighbouring space.
- Renderer clear colour is a clean black void. No biome/texture fallback is
  painted outside recovered geometry.

### Weapons
- Weapon walking has no independent weapon animation: `Weapon::VisualTime()`
  is zero unless an attack is active. A unit test locks this behavior.
- Reduced Weapon FX scales recoil and muzzle flash to 22 % and is the default.
  Full recovered effects remain available in Settings.

### Dungeon and rooms
- Fixed the level-surface cache bug that cleared generated floors after the
  first frame. Dungeon rooms now render their recovered floor and wall themes
  instead of a black interior; the regression is covered by gameplay smoke.
- New Game resets to recovered level `1-1`, clears campaign state and rebuilds
  the world graph at the configured seed.
- Room activation is deterministic:
  `UNENTERED -> ENTERED -> COMBAT -> CLEARED`.
  Enemies are dormant and doors are closed while `ENTERED`; combat starts after
  the entrance grace or inward movement; clear reopens doors and marks the
  room cleared.
- The `1-1` start label is verified during gameplay smoke before the label
  timer expires.
- Saves persist the campaign level and world seed. Loading a `1-2` save in a
  fresh process restores `1-2` and rebuilds the matching room graph instead of
  silently starting at `1-1`.
- Enemy contact/projectile/death-explosion damage is frozen to the recovered
  APK prefab values and compared by a 101-enemy regression test.

## QA added
- `soul-knight-geometry` area probe:
  `--area living-room|garden|trader|mech|reforging|workshop|planting`,
  dungeon `1-1/1-2`, 15x15/15x21/21x15/21x21, three boss rooms, six special
  rooms, and exact `x,y,w,h` rectangles. It walks a player AABB from 16
  directions, checks diagonal/corner approaches, verifies the final foot stays
  on the recovered floor/aisle union, and prints the required
  AREA/OBJECT/PLAYER POSITION/EXPECTED BOUNDARY/ACTUAL POSITION/COLLISION/
  RENDER-DEPTH report on failure.
- Gameplay smoke now covers hub systems, garden plant/water/harvest, NPC talk,
  trader purchase, mech build, dungeon `1-1`, combat entry/closed doors,
  dormant-to-combat activation, clear/reopen, chest, end room, `1-1 -> 1-2`,
  save/load, and return to the Living Room.
- Plant tests cover empty/locked/no-seed, planting, watering, growth, harvest,
  save/restore and seed inventory.
- Camera bounds, room activation, dungeon start and foot-depth rules have unit
  tests.
- Inspection launch aliases: `--area mech-ui`, `--area trader-world`,
  `--area reforging`, `--area planting`, `--area suggestions`,
  `--area calibration`, `--area boss-room-r1/r2/r3`,
  `--area special-sell/statue/mineral/mercenary/mount/chest`.

## Remaining evidence limits
- `MechInfos`/`PlantInfo` Resources data is not present in the extracted APK
  asset tree. Mech material costs are therefore not invented; blueprint-gated
  mechs stay locked. Plant growth uses a marked native 30 s timer and watering
  halves the remaining timer.
- The recovered character clips are idle/run/dead/hit; no dedicated Living Room
  walk or special interaction clip was serialized. NPCs remain stationary and
  use the recovered idle clips plus player-facing and dialogue.
- There is no recovered 11x11 PatternRoom in the 2.6.8 extraction (all 108 are
  15x15, 15x21, 21x15 or 21x21). The calibration view includes 11x11 as a
  developer scale reference, not as an invented playable room.
- The reforging furnace wall is reconstructed from the neighboring recovered
  wall body/cap because HeroRoom has no serialized south-wall row behind the
  furnace.
- The `r_sell` seller placement remains a native reconstruction; the recovered
  seller NPC/display-case sprites and stock values are real.
- The 108 extracted PatternRooms contain recovered door barriers but no
  serialized spike-trap prefab. Room closure therefore uses the recovered door
  barriers; no spike hazard was invented.


## Verification (this pass)

- Release unit suite: 229 test cases / 153,480 assertions, all passing.
- Release CTest: 33/33 passing (includes area probes and extended gameplay
  smoke).
- Debug CTest: 33/33 passing (927.57 s).
- Recovered room catalogue: 108 PatternRooms present; 1000 deterministic seeds
  covered by the room-graph unit test.
- Weapon catalogue audit: 302 weapons; runtime enemy catalogue: 101 definitions;
  recovered enemy damage reference: 101 entries.
- Texture gallery: 4,649 entries, 0 broken; `--validate-all` passes.
- Extended gameplay smoke covers:
  main menu boot -> Living Room -> garden plant/grow/harvest -> NPC talk ->
  trader purchase -> mech build -> dungeon `1-1` -> closed combat room ->
  dormant-to-combat -> clear/reopen -> chest -> end room -> `1-1 -> 1-2` ->
  save/load in a reset campaign -> return to Living Room.
