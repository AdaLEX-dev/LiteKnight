# 10 — Glossary and Source Map

## 1. Project terms

| Term | Meaning |
|------|---------|
| LiteKnight | Native Linux reconstruction/runtime. |
| Reference | Local Soul Knight 2.6.8 APK/Unity data used as evidence. |
| Recovered | Derived from a specific reference object/field with a traceable path. |
| Confirmed | Directly serialized/symbol-backed, not inferred. |
| Hypothesis | Plausible reconstruction, explicitly not confirmed. |
| Native extension | LiteKnight behavior with no reference-complete equivalent. |
| Devroom | Native playable r1_1 test room. |
| Hub / Living Room | Recovered HeroRoom scene (`hero_room`). |
| PatternRoom | Recovered dungeon room template (108 total). |
| HUB | Living Room scene id `hero_room` / `world_hub_id_`. |
| RGChest / RGChestAppoint | Recovered chest classes; appointed chest has a fixed weapon. |
| RGRandomStream | Recovered deterministic RNG stream. |
| PPU | Pixels per world unit; normally 16. |
| Foot Y | World Y of an entity’s feet, used for depth sorting. |
| Blueprint weapon | One of 17 `blueprint_weapon_*` gated weapons. |
| Orb | Trader/mech currency. |
| Material | Forge/workshop resource (iron, wood, gear, magic fragments, etc.). |
| Hostile camp | `camp` value used by projectile damage. |

## 2. Evidence tags

Use these in code comments/JSON:

```text
CONFIRMED   serialized field/symbol/observed runtime object
RECOVERED   extracted from a specific prefab/animation/asset
HYPOTHESIS  inferred reconstruction
UNKNOWN     not recoverable from current artifacts
NATIVE      LiteKnight-only behavior
```

The existing `classification`/`evidence` fields in generated metadata are the
canonical examples.

## 3. Repository map

```text
CMakeLists.txt                  build targets, tests, install
CMakePresets.json               debug/release/clang presets
VERSION                         current version
CHANGELOG.md                    release history
README.md                       landing page
LEGAL.md / NOTICE / LICENSE     legal boundary and license
scripts/                        build/test/play/asset wrappers
src/                            native source
tests/                          unit and tool tests
tools/asset/                    reference extraction and scene builders
tools/qa/                       strict runtime/coordinate QA
analysis/                       recovery evidence and committed metadata
docs/                           documentation
config/                         default config
shaders/                        Vulkan shaders
data/                           small runtime data
```

## 4. Key source files

| File | Purpose |
|------|---------|
| `src/app/main.cpp` | Entry point. |
| `src/app/AppConfig.*` | CLI parsing and defaults. |
| `src/app/Application.*` | Composition root, main loop, hub/dungeon flow, UI actions. |
| `src/app/AssetEnvironment.*` | Asset-root resolution. |
| `src/engine/assets/SceneAsset.hpp` | Central runtime scene data contract. |
| `src/engine/assets/SceneLoader.cpp` | Scene JSON -> runtime types. |
| `src/engine/collision/AabbCollision.*` | AABB world/movement. |
| `src/engine/rendering/SpriteBatch.*` | Sprite draw collection. |
| `src/engine/rendering/VulkanRenderer.*` | Vulkan backend. |
| `src/game/weapons/WeaponRenderer.*` | Authoritative weapon presentation. |
| `src/game/combat/Weapon.*` | Fire cadence, energy, events, projectile spawn. |
| `src/game/combat/ProjectileSystem.*` | Projectile simulation. |
| `src/game/items/ItemWorld.*` | Chests/coins/loot seeds. |
| `src/game/hub/Workshop.*` | Recipes, materials, blueprints. |
| `src/game/save/SaveData.hpp` | Save schema. |
| `src/game/save/SaveSerializer.*` | JSON write/read. |
| `src/game/ui/GameUI.*` | Screens/HUD/station UIs. |
| `src/game/ui/UIRenderer.*` | Logical pixel UI drawing. |
| `tools/asset/build_hub_scene.py` | Living Room source of truth. |
| `tools/asset/build_item_assets.py` | Chest/item/material source of truth. |
| `tools/qa/living_room_coordinate_regression.py` | Coordinate acceptance. |
| `tools/qa/trader_stock_regression.py` | Trader A/B/A acceptance. |
| `tests/unit/weapon_presentation_test.cpp` | Walking/aim/fire hard rule. |
| `tests/unit/weapon_init_ranger_test.cpp` | Rogue starter regression. |
| `tests/unit/weapon_station_test.cpp` | Blueprint/material crafting. |

## 5. Key data files

| File | Contents |
|------|----------|
| `analysis/assets/scenes/hero_room.json` | Living Room instances, sprites, collision, markers, characters, weapons. |
| `analysis/assets/scenes/hero_room_manifest.json` | Hub builder manifest/hashes. |
| `analysis/assets/items/item_assets.json` | Chests, coins, materials, shop items, portals, energy, weapon drops. |
| `analysis/assets/workshop/weapon_materials.json` | Forge recipes/material costs. |
| `analysis/assets/weapon_catalogue.json` | 302 weapon entries. |
| `analysis/assets/character_loadouts.json` | 15 hero identities and starter weapons. |
| `analysis/assets/localization.json` | Recovered terms/dialogue. |
| `analysis/assets/asset_inventory.json` | Reference inventory/classification. |
| `analysis/re/` | Behavioral evidence (RNG, damage, rooms, timing, etc.). |

## 6. Important IDs

### Living Room markers

```text
character_station
forge_station
weapon_station
material_chest
garden_station
trader_station
mech_station
hub_door
mission_board
weapon_chest
dungeon_portal
npc_explorer
npc_magic
npc_cXX
```

### Hub chest

```text
runtime definition: hub_weapon_chest
visual source:      chest00
position:           (-2.89, 6.31)
marker:             weapon_chest at (-2.89, 6.31)
reference coordinate: (-2.643, 5.410)
```

`chest00` itself is an appointed `weapon_021` chest and must not be used as the
random hub starter chest.

### Blueprint weapons

```text
weapon_184, 188, 189, 195, 211, 212, 213, 217,
weapon_223, 224, 237, 238, 240, 242, 245, 252, 294
```

### Rogue

```text
character id: c01
display name: Rogue
starter weapon: weapon_init_ranger ("Jack and Mary")
audio: fx_sword1
projectile: bullet_107
projectile visual: weapons3_62
special: rogue_throw
```

### Table/seating characters

```text
c13 -> chair (0.58, 0.32)
c04 -> chair (-2.445, -0.95)
c08 -> chair (2.942, -0.902)
seat plane = chair Y + 0.40
```

### NPC / grave

```text
npc_magic marker: (32.20, -17.94)
grave/guardian sprite: hub_spr_1047 at (32.15, -14.37)
reference grave coordinate: (32.515, -16.910)
distance marker -> reference: ~1.08
```

### Portal

```text
magic_protal1 top: hub_spr_2248 at (13.53, -14.87), layer 6
magic_protal1 base: hub_spr_2215 at (13.53, -15.74), layer 4
marker: dungeon_portal at (13.53, -14.90)
reference coordinate: (13.724, -14.920)
```

### Oxygen bottles

```text
visual: room/objects/static/research/bottle, sprite hub_spr_2172
visual position: (23.45, -6.97)
reference coordinate: (23.625, -4.740)
collision footprint: recovered polygon AABB
```

## 7. Coordinate conventions

- All supplied bug coordinates are world coordinates.
- Player foot position is the player entity position; the collision center is
  `foot.y + 0.5`.
- Player half extents are `(0.4, 0.4)`.
- Review teleports place the player south of the subject.
- A “nearest object” distance is Euclidean in world units.
- Collision boxes are `[min_x, min_y, max_x, max_y]`.
- A thin (≈0.02) floor-edge box is an intentional dark-void boundary.

## 8. Source map by question

| Question | Look here |
|----------|-----------|
| Why does the player move this fast? | `PlayerCharacterSet`, `PlayerLocomotion`, scene player stats, `Application::FixedUpdate`. |
| Why does this weapon fire this way? | scene `weapons`, `SceneLoader.cpp`, `Weapon.cpp`, `WeaponDefinition`. |
| Why is this sprite here? | builder + `analysis/assets/scenes/*.json`, then runtime asset copy. |
| Why is this collider here? | builder collider section, scene `collision.solids`. |
| Why does this interaction appear? | scene `interaction_markers`, `Application::NearbyInteraction/TryInteract`. |
| Why is this UI screen shown? | `GameUI::Screen`, `Application::ProcessUIAction`, `GameUIData`. |
| Why did save data vanish? | `SaveData`, `SaveSerializer`, `SaveManager`, `Application::Save/Load`. |
| Why is the room graph this shape? | `RoomManager`, `DungeonPlan`, `BiomeSystem`, `map_seed`. |
| Why are trader items these? | `ConfigureTraderOffers`, item drop tables, weapon catalogue, stock seed. |
| Why is the weapon offset? | `WeaponRenderer::ComputePresentation`. |
| Why is this test failing? | test source + current source-of-truth data; update the test if behavior intentionally changed. |

## 9. Validation vocabulary

- **PASS** — the checked property holds at the supplied coordinate/state.
- **FAIL** — the checked property does not hold.
- **BLOCKED** — the reference artifact required to verify/fix is absent.
- **VISUAL** — geometry/logic passes but pixel/pose acceptance needs a human
  screenshot.
- **STALE** — the runtime asset was not refreshed after a builder change.
- **NATIVE** — implemented as a LiteKnight behavior, not reference-confirmed.

Use these words explicitly in reports. Do not collapse them into “tests pass”.
