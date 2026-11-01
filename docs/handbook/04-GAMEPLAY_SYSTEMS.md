# 04 — Gameplay Systems

This is a map of the game systems, how they are wired, and what is actually
implemented. For exact reference evidence, follow the source paths and the
documents under `analysis/re/`.

## 1. Player and characters

Files:

```text
src/game/player/PlayerCharacterSet.*
src/game/player/Character.hpp
src/engine/locomotion/PlayerLocomotion.*
src/engine/locomotion/FacingDirection.*
src/game/combat/Damage.hpp
src/game/player/Energy.*
src/game/player/Aim.*
src/game/player/Ability.*
```

Behavior:

- 15 recovered character IDs `c00..c14` with body/dead sprites and idle/run clips.
- Character selection changes body visuals, stats, starting weapon, and ability.
- Movement is 8-directional, normalized, fixed-step, with axis-separated AABB
  collision.
- Facing follows movement while moving; aim chooses idle facing.
- Health/armor/energy are separate values. Damage applies to armor first where
  the reference rule applies.
- Aim is 360° mouse aim plus auto-aim. Auto-aim default is on for a fresh game.
- `Q` activates the active character ability. Knight c00 Dual Wield is the fully
  implemented first ability; other abilities have data-driven hooks.
- Death/reset is native development behavior; production reborn flow is not a
  reference-complete implementation.

Key rule: player render position is synchronized to simulation position. The
renderer must never move the player.

## 2. Weapons

Files:

```text
src/game/weapons/WeaponRegistry.*
src/game/weapons/WeaponInventory.*
src/game/weapons/WeaponCatalogue.*
src/game/weapons/WeaponPickup.*
src/game/weapons/WeaponRenderer.*
src/game/combat/Weapon.*
src/game/combat/ProjectileSystem.*
src/engine/assets/SceneAsset.hpp  (WeaponDefinition)
src/engine/assets/SceneLoader.cpp
```

Data:

- `analysis/assets/weapon_catalogue.json` contains 302 entries.
- The runtime scene integrates a subset of weapon definitions per room.
- `WeaponDefinition` carries display name, class, audio clip, visual sprite,
  curves, projectile data, damage/crit/deviation, energy cost, melee flag,
  special behavior, and owner deviation.
- `weapon_inventory_` has two slots. `1`/`2` select; `Tab` cycles.
- `weapon_` and `secondary_weapon_` are independent combat states.
- Pickups are world entities; picking one up equips the active slot and drops
  the old weapon as a pickup.

Weapon presentation is a hard project rule:

- `WeaponRenderer::ComputePresentation` is the single authoritative transform.
- Hand anchor + weapon visual offset are fixed.
- Walking does not enter the presentation function.
- Aiming rotates around the anchor.
- Firing may add tiny transient recoil from recovered `visual_position_x`/
  `visual_rotation_z` curves, scaled by `effect_scale` (normally 0.22).
- Muzzle flash is drawn only during an attack window and only if the weapon has
  a recovered muzzle sprite.
- If no muzzle sprite exists, no muzzle is drawn.
- The recoil rotation is converted from degrees to radians.
- `tests/unit/weapon_presentation_test.cpp` is the regression gate.

Rogue starter weapon:

- `weapon_init_ranger` = “Jack and Mary”.
- It is a hybrid melee + thrown weapon, not a pistol.
- Recovered `audio_clip = fx_sword1`, projectile `bullet_107` using the
  `weapons3_62` projectile sprite, `bullet_speed = 36`, attack cycle `0.3125 s`.
- `special.kind = rogue_throw`; the weapon still keeps its melee strike.
- `tests/unit/weapon_init_ranger_test.cpp` asserts it is not the Bad Pistol and
  that it spawns a projectile.

## 3. Projectiles and damage

Files:

```text
src/game/combat/ProjectileSystem.*
src/game/combat/Damage.hpp
src/game/combat/Damage.cpp
```

- Projectiles have position, velocity, lifetime, damage, camp, collider size,
  sprite, sorting data.
- Native simulation is swept linear movement with AABB collision and wall
  blocking. It approximates the reference Rigidbody2D/bullet behavior.
- Projectile hit applies damage to player/enemy/box based on camp.
- Rebound behavior is data-flagged but not a full Rigidbody simulation.
- Crit/repel/through fields exist; full reference crit/element/status effects are
  incomplete.

## 4. Enemies

Files:

```text
src/game/enemies/EnemyDefinitions.*
src/game/enemies/EnemySystem.*
src/game/enemy/EnemySystem.*
```

- Enemy definitions come from recovered metadata and per-room scene data.
- `EnemySystem` owns instances, HP, death, contact damage, and the first-slice
  AI.
- `PopulateBiomeEnemies` spawns weighted biome pools when a room needs enemies.
- Room activation gates enemy behavior: dormant until combat, then doors close.
- Clear detection is based on zero alive enemies.
- Full EnemyAI04 scouting/pathfinding remains partial; do not assume every
  recovered enemy controller is fully reconstructed.

## 5. Abilities, summons, pets

Files:

```text
src/game/player/Ability.*
src/game/summon/SummonSystem.*
src/game/pet/PetSystem.*
```

- Ability state is empty/ready/active/cooldown; UI exposes icon and cooldown.
- Knight Dual Wield spawns an off-hand weapon attack using `DrawAt`.
- Summons support skeleton/phantom/funnel-sword style special events.
- Pets follow, attack, are selectable in the pet menu, and persist by ID.
- Pet visuals use recovered body sprites and shadow sprites.

## 6. Rooms and doors

Files:

```text
src/game/room/RoomManager.*
src/game/room/RoomDoorController.*
src/game/room/RoomActivation.*
src/game/room/ObstacleField.*
src/game/room/DungeonPlan.*
src/game/room/BiomeSystem.*
src/game/room/LevelSurface.*
```

Room lifecycle:

```text
Inactive -> Entered -> Combat -> Cleared
```

- Entering a combat room closes doors and starts enemies.
- Killing all enemies opens doors and marks the room cleared.
- Door state is animated; collision is rebuilt when blocking changes.
- Room state is captured/restored across visits: enemies, pickups, chests,
  coins, clear state.
- Dungeon graph generation is deterministic under `--map-seed`; it uses
  recovered room templates and a 5x5 relation grid.
- All 108 recovered PatternRooms are integrated as scene data; the runtime graph
  picks a subset per seed.
- Boss rooms and special rooms have their own activation/door behavior.

## 7. Hub / Living Room

Files:

```text
tools/asset/build_hub_scene.py
analysis/assets/scenes/hero_room.json
src/app/Application.cpp
```

The hub is a full scene with explicit interaction markers:

- character station;
- forge/workshop;
- weapon station;
- material chest;
- garden;
- trader;
- mech station;
- dungeon portal;
- task/mission board;
- weapon chest;
- hero NPCs;
- explorer NPC near the portal;
- magic NPC near the grave;
- Rogue beanbag NPC.

Hub-specific systems:

- `ConfigureHubGarden` creates plots from recovered garden metadata.
- Runtime hub chest uses `hub_weapon_chest` at `(-2.89, 6.31)` with a marker at
  the reported chest coordinate.
- Trader offers are generated from the recovered drop pool and a persisted stock
  seed.
- Mech station UI is an enlarged world-space panel; building a mech costs orbs
  and sets `active_mount_id_`.
- Workshop opens for both forge and weapon station; the weapon station label and
  blueprint/material behavior are distinct.
- Material chest opens a dedicated `MATERIALS` screen, not the forge.
- Mission board cycles recovered `task/*` localization lines.
- NPC dialogue comes from `Character<N>_talk*`, `adv_toturial/*`, or plot terms.

## 8. Items: chests, coins, materials

Files:

```text
src/game/items/ItemAssets.*
src/game/items/ItemWorld.*
src/game/items/SellerSystem.*
src/game/items/BoxSystem.*
analysis/assets/items/item_assets.json
```

Chest rules:

- `ChestDefinition` supports appointed weapons, random drop level, boss chests,
  big chests, object lists, coins, opening animation time.
- `ItemWorld` stores per-room chest/coin state, seeds loot deterministically from
  room id + chest index + run loot seed, and prevents duplicate loot.
- `hub_weapon_chest` is the Living Room starter chest: chest00 visuals/collider
  with `weapon_drop_level = -2` (random level from current chest level). The
  original `chest00` appointed `weapon_021` would always give the same weapon.
- The runtime chest is interactable via its marker; opened state survives room
  transitions and save/load.
- Coins magnet/collect into the HUD wallet.
- Seller system handles special-room merchant stock; sold items persist.
- Box system handles breakable RGBox objects and drops.

Material inventory:

- `Workshop` loads recovered recipes (`weapon_materials.json`).
- Materials are stored by ID/count and persisted.
- The materials chest UI displays actual material icons, names, and counts.
- Forge/weapon station can dismantle the active weapon for materials.
- There is no free weapon grant from the weapon station.

## 9. Workshop and blueprints

Files:

```text
src/game/hub/Workshop.*
src/game/ui/GameUI.cpp (DrawWorkshop)
src/app/Application.cpp (CraftWeapon / Workshop UI)
tests/unit/weapon_station_test.cpp
```

Blueprint rules:

- 17 recovered IDs are blueprint-gated:
  `weapon_184, 188, 189, 195, 211, 212, 213, 217, 223, 224, 237, 238, 240,
  242, 245, 252, 294`.
- A locked recipe shows `BLUEPRINT` and
  `NEED TO COLLECT WEAPON BLUEPRINT FIRST`.
- Crafting requires the blueprint (if applicable) and all recovered materials.
- `ConsumeForCraft` deducts materials and returns false if anything is missing.
- A successful craft equips the weapon, drops the old one, plays pickup audio,
  and saves.
- Blueprint unlock set and materials persist in save v5.
- Exact acquisition loop for the 17 blueprints is not recovered; this is a
  documented reference gap. The station behavior and persistence are tested.

## 10. Garden / plants

Files:

```text
src/game/hub/PlantSystem.*
src/game/hub/PlantPlot? (see PlantSystem.hpp)
tools/asset/build_plant_assets.py
```

- Garden plots are created from recovered hub garden data.
- Seeds are stocked/persisted; planting advances growth over time.
- Interaction supports plant/water/harvest based on plot state.
- Harvest yields the recovered item and count.
- Review mode can seed a carrot for visual inspection.
- Exact plant economy/PlantInfo table is not recovered.

## 11. Trader

Files:

```text
src/app/Application.cpp (ConfigureTraderOffers, BuyTraderItem)
src/game/ui/GameUI.cpp (DrawTrader)
tools/qa/trader_stock_regression.py
```

- Exactly three offers/pedestals.
- Stock is generated from item drop tables and the weapon catalogue.
- `trader_stock_seed_` is derived from `--map-seed` when supplied, otherwise
  randomized at new game and persisted.
- Same seed reproduces the same stock; different seeds differ.
- Each offer has a price from recovered `item_value`.
- Sold state persists per offer index; buying deducts orbs.
- World weapons are drawn vertically on the three pedestals using actual sprite
  dimensions/pivots.
- The trader UI shows three cards with name, damage, energy, price/sold state.
- Q/A/B/A regression:
  ```bash
  python3 tools/qa/trader_stock_regression.py
  ```

## 12. Mech station

Files:

```text
src/app/Application.cpp (OpenMechStation, CycleMechSelection, BuildMech)
```

- World-space station UI, larger than the old HUD panel.
- Lists mech name, orb cost, defence, blueprint/lock state.
- Selecting/building deducts orbs and sets the active mount.
- Movement speed bonus is applied while mounted (0.35 currently).
- Exact mech stat tables are not recovered; values are evidence-backed
  reconstructions where possible, native extensions otherwise.

## 13. Save/load

Files:

```text
src/game/save/SaveData.hpp
src/game/save/SaveSerializer.*
src/game/save/SaveManager.*
src/app/Application.cpp (SaveGameToDisk/LoadGameFromDisk)
tests/unit/save_test.cpp
```

- Native JSON schema version 5, not reference-compatible.
- Slot path under XDG data: `~/.local/share/soul-knight/saves/slot_0.json`.
- Atomic write via temp file.
- Persists campaign level/scene, player, weapons, health/energy, wallet,
  cleared rooms, chest/coin state, plants/seeds, materials/blueprints, trader
  seed/sold offers, pet, statue/mercenary/mount, potion state, map seed, UI
  settings.
- `SaveSerializer` must read/write every field; the material/unlocked-blueprint
  read path was a real bug fixed in this pass.
- `SaveManager` is the only object that should touch the slot file.

## 14. Progression / special rooms

Files:

```text
src/game/progression/
src/game/bosses/
src/app/Application.cpp (special_objects_, seller_, room graph)
```

- Special rooms: start, transfer gate, chest, weapon provider, statue, mineral,
  mercenary, mount.
- Statue purchase applies a buff; mercenary purchase spawns an ally; mineral
  mining grants materials/coins; mount sets active mount.
- Boss rooms use their recovered room templates and boss chest pools.
- Full character unlock/progression economy is not reference-complete.

## 15. Audio

Files:

```text
src/platform/audio/AudioEngine.*
src/game/audio? (see Application PlayEffect/PlayMusic)
analysis/assets/audio/*
```

- Audio uses miniaudio behind `AudioEngine`.
- UI clicks, pickup, chest open, weapon fire, music, and special effects are
  routed through `PlayUiSound`/`PlayEffect`/`PlayMusic`.
- Per-weapon `audio_clip` is loaded from the scene definition; do not fall back
  to the Bad Pistol clip.
- If a clip is missing, prefer silence over the wrong weapon sound.
- Exact audio trigger timing is partial.

## 16. Known gameplay gaps

See `08-CURRENT_STATE_AND_ROADMAP.md` for the current list. The big ones:

- full reference EnemyAI04/pathfinding;
- exact procedural dungeon algorithm;
- reference save format;
- full plant/mech economy tables;
- exact blueprint acquisition loop;
- full crit/element/status system;
- full boss/special-room behavior;
- complete weapon-family special behaviors;
- pixel-perfect Rogue sitting animation (no dedicated recovered clip).
