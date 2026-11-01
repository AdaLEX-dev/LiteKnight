# 08 — Current State and Roadmap

**Snapshot:** 2026-11-01, beta cut `v0.6.0-beta.1`.
**Canonical executable:** `build/soul-knight`.
**Reference:** Soul Knight 2.6.8, Unity 2018.4.12f1, local-only assets.

This document is deliberately blunt. The project is a playable development beta,
not a finished reconstruction.

## 1. What is working now

### Platform / runtime

- Wayland-only SDL3 window, Vulkan 1.3 renderer, swapchain, resize/minimize.
- 50 Hz fixed simulation, 60 FPS render target.
- XDG paths for saves/config/cache/assets.
- Native audio engine and per-weapon audio metadata.
- Native save/load JSON schema v5.

### Hub / Living Room

- Recovered HeroRoom scene with floor, walls, garden, stations, NPCs, portal,
  grave, beanbag, mission board, trader, mech, forge, material chest.
- Real runtime weapon chest (`hub_weapon_chest`) with random loot and persisted
  open state.
- Dedicated material storage UI with actual icons/names/quantities.
- Weapon station/forge blueprint and material crafting; no free weapon.
- Trader stock seeded and persisted; exactly three offers; sold state persisted.
- Three vertical world pedestals for trader weapons.
- Enlarged mech platform UI.
- Garden planting/harvest and planter collision.
- Mission board with recovered task lines.
- Explorer NPC dialogue and magic NPC → grave relationship.
- Rogue beanbag NPC restored after fixing the sprite path-ID collision.
- Table characters aligned to chair seat planes.
- Stair-wall repair removed; dark outside-world gap remains.
- Wall cap/shortening and oxygen-tank footprint collision.

### Dungeon / combat

- 108 recovered PatternRooms + special rooms as scene metadata.
- Deterministic room graph under `--map-seed`.
- Room activation lifecycle and animated door blocking.
- Enemy spawn/clear/door lifecycle.
- 302-entry weapon catalogue; integrated weapons with real projectile sprites.
- Player health/armor/energy, auto-aim, dual wield, weapon energy, pickups.
- Rogue starter weapon `weapon_init_ranger` = hybrid melee+thrown with
  `bullet_107`/`weapons3_62`, `fx_sword1`, no pistol inheritance.
- Weapon presentation is anchored while walking; aim rotates in place; recoil is
  tiny; muzzle is per-weapon and attack-gated.

### UI

- Main menu, pause, settings, save/load, character/weapon/pet selection.
- HUD with health/armor/energy, coins/orbs, weapon slots, minimap, ability.
- Workshop/weapon station, material chest, trader, mech, inventory screens.
- Persistent logical HUD layout and UI design mode.
- Review modes for the coordinate pass and station UIs.

### Testing

At the beta cut:

```text
Unit:       238 test cases / 161,569 assertions / 0 failed
Coordinate: 41/41 checks passed
Trader:     4/4 checks passed
Gameplay smoke: passed
```

The coordinate suite covers 19 supplied bug coordinates plus station/garden/
autonomous-walk checks. See `11-COORDINATE_PASS_2026-11-01.md`.

## 2. What changed in the beta pass

- Added `tools/qa/living_room_coordinate_regression.py`.
- Added `tools/qa/trader_stock_regression.py`.
- Added CTest registrations for both.
- Added unit tests for weapon presentation, Rogue starter, weapon station
  blueprints, multi-seed chest loot, chest save/restore, and save fields.
- Removed three decorative signboard placeholders; kept/restored the functional
  task board.
- Restored `magic_protal1`, explorer NPC, and guardian/grave visual.
- Removed the stair repair wall and artificial 3x3 block.
- Shortened the protruding wall at `(6.6,-23.08)`.
- Added table/object/oxygen collision footprints.
- Seated c13/c04/c08 on their chairs.
- Fixed sprite path-ID collisions (Rogue lighting vs signboard).
- Restored the Rogue body in the beanbag.
- Added `hub_weapon_chest` random chest; removed the appointed `weapon_021`
  free weapon.
- Added material/blueprint persistence; fixed missing serializer reads.
- Added trader stock seed + sold offers persistence.
- Enlarged mech UI.
- Added vertical trader pedestal rendering.
- Added blueprint/material locks to the weapon station.
- Added dedicated material chest UI.
- Added review teleports for every supplied coordinate.

## 3. Known verification debt

These were not fully re-run after the last builder/runtime changes at the beta
cut:

- full Release and Debug CTest matrices;
- all 108 PatternRoom scene runs;
- all boss rooms and special rooms in one sweep;
- 1000 map-seed sweep;
- complete 302-weapon catalogue/gallery validation;
- full autonomous Living Room walk in the *actual runtime* (the current suite
  uses a grid AABB model and BFS, not the real player controller);
- exact `/build/soul knight` invocation. That path does not exist in this tree.
  The canonical path is `build/soul-knight`.

Run these before calling the beta “fully verified”. See
`06-TESTING_AND_QA.md` for commands.

## 4. Known functional / visual gaps

### Reference blockers

- No recovered `PlantInfo` or `MechInfos` Resource tables for exact plant/mech
  stats/text.
- No dedicated Rogue sitting animation clip. The current Rogue uses the
  recovered standing body positioned in the beanbag.
- Exact blueprint acquisition loop for the 17 `blueprint_weapon_*` IDs is not in
  the recovered runtime data. The station gate, cost, craft, and persistence are
  implemented; the source of each blueprint is not.
- No 11x11 PatternRoom in the recovered room set.
- Exact reference save format is unknown; LiteKnight uses its own schema.
- Full EnemyAI04 scouting/pathfinding is not reconstructed.
- Exact reference procedural dungeon algorithm is not reconstructed; the native
  graph is deterministic over recovered rooms.
- Full crit/element/status/attachment systems are partial.

### Visual/manual gaps

- Table characters are aligned to seat planes, but there is no dedicated sitting
  clip; inspect the pose visually.
- Trader world weapons are vertical and centered, but exact pedestal/sprite
  scale is a reconstruction.
- Material icons are recovered, but the review stock is QA-only; a fresh save
  starts empty unless materials are earned/loaded.
- Mech panel is enlarged/readable; inspect aspect-ratio cropping at other
  resolutions.
- Mission board orientation/collision should be visually checked against the
  wall.
- Portal NPC facing and exact grave relationship should be visually checked.
- Oxygen tank AABB covers the reported coordinate but may feel too wide; it is a
  footprint reconstruction, not a full polygon physics body.
- `build-release/soul-knight` is an older synthetic-scene binary. It is not the
  canonical executable. Do not distribute it or treat it as current.
- `/build/soul knight` (with a space) is not present in this tree. If a user
  reports that exact path, determine which machine/binary they mean before
  debugging the canonical build.

## 5. Next work, in priority order

1. **Run the full verification matrix** in `06-TESTING_AND_QA.md`:
   Release/Debug CTest, 108 rooms, boss/special rooms, 1000 seeds, 302 weapons,
   gallery.
2. **Visual acceptance** of the coordinate pass:
   - table seating pose;
   - Rogue beanbag pose;
   - trader pedestal scale/sitting;
   - material chest icons;
   - mech panel at 16:9/21:9/4:3;
   - mission board/portal/grave/NPC facing.
3. **Fix the actual runtime autonomous walk**: replace the grid BFS check with a
   real player-controller path test from multiple spawns, or wire an autopilot
   into a CTest mode.
4. **Dungeon executable identity**: remove or rename stale
   `build-release/soul-knight`; keep one canonical executable. If a `/build/soul
   knight` launcher is required, create a documented symlink/copy, not a second
   stale binary.
5. **Blueprint acquisition**: either recover the loop or keep the documented
   blocker and expose a test/QA unlock path.
6. **Weapon catalogue sweep**: ensure every integrated weapon has non-pistol
   audio/projectile/muzzle unless evidence says otherwise.
7. **Boss/special room behavior**: verify door/chest/seller/statue/mercenary/
   mount interactions across all special rooms.
8. **Save migration**: add explicit tests for old-version saves if the schema
   changes again.
9. **Reference evidence cleanup**: update stale `docs/PROJECT_STATE.md`,
   `docs/ROADMAP.md`, `docs/development/LIVING_ROOM*.md` to point at this
   handbook.

## 6. Release decision

This beta is suitable for:

- developers who have the local reference tree;
- testing the hub, coordinates, weapon behavior, trader, workshop, save/load;
- reproducing the coordinate suite and unit suite.

It is **not**:

- a content-complete Soul Knight replacement;
- a reference-asset redistribution;
- a stable 1.0;
- fully regression-verified across every room/seed/weapon.

## 7. How to leave the tree for the next person

Before stopping:

```bash
./scripts/prepare-assets.sh
cmake --build build -j"$(nproc)"
./build/soul-knight-test
python3 tools/qa/living_room_coordinate_regression.py
python3 tools/qa/trader_stock_regression.py
git status
```

Record any failing/visual item in this file or the release note. Do not leave a
“temporary” broken binary or a half-written test without a note.
