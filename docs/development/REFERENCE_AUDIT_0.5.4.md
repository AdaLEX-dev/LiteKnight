# 0.5.4 Reference Audit

Status labels: **CONFIRMED**, **HIGH CONFIDENCE**, **HYPOTHESIS**, **UNKNOWN**,
**CONTRADICTED**.

| # | Topic | Finding | Status | Evidence |
|---:|---|---|---|---|
| 1 | Weapon rarity fields | No separate `WeaponRarity`; `WeaponInfoRow.level/dropLevel`, `RGWeapon.item_level/item_value` are the coded fields | CONFIRMED | IL2CPP class dump; `weapon_catalogue.json` |
| 2 | Weapon drop sources | 7-level `WeaponDropInfo` weighted tables; RGChest uses `GetWeaponWithDropLevel` | CONFIRMED | `item_assets.json`; IL2CPP |
| 3 | Chest loot selection | `RGChestAppoint.object_list` and `RGChest` level→WeaponDropInfo | CONFIRMED | chest audit; `OpenChest` disasm |
| 4 | Chest-room generation | `MapManagerLevel.room_chest_distributions`/special lists exist in class; serialized HeroRoom/map configs do not retain a usable chest-room table | UNKNOWN | MapManagerLevel fields |
| 5 | Boss weapon drops | Boss chest/enemy reward fields exist; exact boss drop tables not recovered | UNKNOWN | `RGChestBoss`, enemy reward fields |
| 6 | Orb currency | `AboGameData.gems/lastGems/varGems`, `RGSaveManager.GetGem/ConsumeGem`, `ui_52` icon | CONFIRMED | RGSaveManager prefab; native trader/HUD |
| 7 | Trader implementation | `ItemRoomSeller` + 3 `ItemSellPlaceGem` platforms; native orb trader implemented | CONFIRMED structure / HYPOTHESIS offer pool | `TRADER_SYSTEM.md` |
| 8 | Workshop/material inventory | `ItemData.materials/forgeWeapons`, `UIForge.FlushMaterials`; full recipe table not recovered | PARTIAL | `MATERIALS_INVENTORY.md` |
| 9 | Pet system | `RGPetInfo`, `RGSaveManager.pet_list`, follow fields on `RGPetController`; no native pet yet | PARTIAL | `PET_SYSTEM.md` |
| 10 | NPC speech | `TalkCharNpc`/`SequenceTalkTrigger` fields; no native dialogue yet | PARTIAL | `NPC_DIALOGUE.md` |
| 11 | Baseline weapons | `RGController.GetInitWeaponName` + `UIHeroSkinWeaponIndex`; Knight `InitWeapon` = `weapon_000` | CONFIRMED Knight / UNKNOWN others | WeaponInfo tag; c00 |
| 12 | Dual wield | `C01Controller.hand2/hand3`; native second weapon fires and renders at h2 | CONFIRMED structure | IL2CPP; native |
| 13 | Weapon hand transforms | c00 `img/h1 (0,0.5)`, `h2 (0.55,0.6)`, `h3 (0.15,0.85)`; sprite pivots near grip | CONFIRMED | prefab hierarchy |
| 14 | Weapon AnimatorControllers | `w_ide`, attack clips and Attack events recovered; native cadence uses them | CONFIRMED cadence / PARTIAL visuals | weapon prefabs |
| 15 | Enemy catalogue | 102 enemy prefabs, 77 biome-assigned | CONFIRMED catalogue / PARTIAL runtime | `enemy_catalogue.json` |
| 16 | Enemy biome pools | `MapManagerLevel.enemy_infos` weights per level | CONFIRMED | `map_catalogue.json` |
| 17 | Map generation | 18 MapManagerLevel configs, map_long/start/end/floor/wall/obstacle lists | CONFIRMED data / UNKNOWN full `CreateRoom` | `MAP_GENERATION.md` |
| 18 | Room relation generation | `room_relation` is runtime-only; native BFS labels implemented | HYPOTHESIS | MapManagerLevel class |
| 19 | Corridor generation | Native physical doors sealed when no graph edge; no separate corridor scenes | HIGH CONFIDENCE | topology audit |
| 20 | Biome/stage progression | level/chest_level/map_long present; native uses 1-1..1-3 room codes | PARTIAL | `BIOMES.md` |
| 21 | Dungeon portal progression | Return portal implemented; forward level progression not yet | PARTIAL | portal asset/runtime |
| 22 | Room title transition | native fade/slide only; exact reference clip not recovered | HYPOTHESIS | `ROOM_TYPES.md` |
| 23 | Audio system | 11 actual clips wired; 56 candidates audited | CONFIRMED first pass | `AUDIO_SYSTEM.md` |
| 24 | Save/load system | native JSON v3; orbs; S/L UI; per-room state missing | PARTIAL | `SAVE_LOAD.md` |
| 25 | S/L UI | S/L button plus save/load panel implemented | CONFIRMED native | UI tests |
| 26 | HeroRoom hierarchy | default active hierarchy, 9,036 instances, cleanup filters | CONFIRMED | hub audit |
| 27 | Planter/garden hierarchy | `garden/land_root/land_0..7`, fertilize/water/lock children; floor tilemap | CONFIRMED hierarchy / PARTIAL shape | HeroRoom JSON |
| 28 | Workshop hierarchy | `static/research` forge/mech/bookshelf/station; material/recipe table missing | PARTIAL | HeroRoom JSON |
| 29 | Trader hierarchy | `room_seller/container0..2`, `npc01`, 3 platforms; native trader implemented | CONFIRMED | HeroRoom JSON |
| 30 | Pet hierarchy | `pet`, `wild_pet`, `pet_list` icons; native follower pending | PARTIAL | HeroRoom/RGSaveManager |

## Other confirmed corrections

- `WeaponInfo` has **no rarity enum**; assigning arbitrary colors/rarities would
  be fiction.
- `RGRoomX.room_type` is `1` for every one of the 108 PatternRoom prefabs; room
  category must come from MapManagerLevel lists, not this integer alone.
- `RGEnergy` serializes `value=8`, `speed=14`, `range=6`; energy-pot prefabs
  serialize 80/150 energy.
- `MapManagerLevel` `1-1` uses `map_long=3`, `chest_level=1`, and
  `r_start_no_event` → `r_transgete`.
