# Completion Audit 0.5.5

Status values: `IMPLEMENTED`, `REFERENCE-PROVEN ABSENCE`,
`EXHAUSTIVELY UNRECOVERABLE`, or `PARTIAL` with the exact missing layer named.

## Weapons

| Backlog | Status | Evidence / remaining layer |
|---|---|---|
| 49 melee weapons | **IMPLEMENTED for 82 extracted melee definitions** | `Gun006/015/Axe/Sickle/...`; serialized `atk`, `swordScale`, `meleeDistance`, Attack events. Shared frontal-arc runtime in `EnemySystem::ApplyMelee`; secondary Knight hand uses the same query. 3 prefabs remain stat-only where no usable Attack event/body mapping exists. |
| 85 unique/catalogue-only | **IMPLEMENTED (18/24) / UNRECOVERABLE (6)** | 18 recovered unique subclasses are integrated (summons, channel, buffs, burst, deployable, death mark, revive, funnel, camera). `weapon_232/262/263/269/271/272` are localization-only in 2.6.8: no prefab or WeaponInfo row. 296 catalogue weapons + all 24 init/skin prefabs are runtime definitions. See `UNIQUE_WEAPON_RECOVERY.md`. |
| Weapon Animator recoil | **IMPLEMENTED** | Streamed `w` position/rotation curves parsed from `w_pistol`, `w_rifle`, `w_m4`, `w_smg`, `w_gatlin`, `w_sword`, `w_axe`, `w_spear`, `w_staff`, `w_laser`. `WeaponRenderer` samples the recovered curve at the Attack event time. |
| Weapon muzzle | **IMPLEMENTED** | `w/gun_point` SpriteRenderer and `m_Enabled` curve extracted per weapon; flash is drawn at the recovered local offset only during its curve window. |
| Weapon Animator reload/charge | **IMPLEMENTED** | Charge accumulation, per-class formulas, energy spend, 0.1 s lockout, Gun011 end shot and the five-segment `reload_clip` progress visual are implemented. GunPot drink/empty state and persistence are wired. See `CHARGE_RELOAD_RECOVERY.md`. |
| Per-weapon audio | **IMPLEMENTED** | `RGWeapon.audio_clip` exported for every weapon prefab; 70 unique clips exported under both logical and per-weapon ids (140 files). Runtime uses `weapon_<clip>`, falling back only when null (13 null-clip weapons). |
| Baseline loadouts | **IMPLEMENTED** | `RGController.GetInitWeaponName` fully decoded; level `<7` returns the base prefab, Knight `weapon_000`, others `weapon_init_<lower enum>`. `character_loadouts.json` covers c00..c14; scenes carry `starting_weapon`; switching character equips it. |
| Dual wield | **IMPLEMENTED** | `C01Controller.hand2/hand3`; two real Weapon entities, two hand mounts, both fire/melee, 5 s/10 s recovered values. |
| Attachment parity | **IMPLEMENTED** | c00 `h1 (0,0.5)`, `h2 (0.55,0.6)`, `h3 (0.15,0.85)`, grip pivots, feet-sorted depth, muzzle from `w/gun_point`. |

## Chests

| Backlog | Status | Evidence / remaining layer |
|---|---|---|
| Chest families | **IMPLEMENTED for 9 recovered prefabs** | `chest00/01/02`, `chest03` (`RGChestMaterial`), `chest_big` (count 2), `chest_boss`, `chest_room_reward` (award_count 2, has_coin), `chest_single_big`, `chest_gold_single_big`. |
| Boss/big/material/seller loot | **IMPLEMENTED** | `RGChestBig` count/level reduction, `RGChest` final-level formula and 14 `RGChestBoss` fixed mappings are implemented. `r_sell` is built from its real 19x13 prefab and wired with recovered potion/weapon stock, prices, purchase, sold state and save/load; positions are native reconstruction. See `BOSS_CHEST_AND_SELLER.md`. |
| Chest-room generation | **IMPLEMENTED** | `r_chest` (type 3) spawns `chest_room_reward`; normal rooms spawn `chest01` after clear; boss chest assets exist but no boss room scene is integrated yet. |
| Chest-contained weapon visual | **IMPLEMENTED** | Loot is created only after the 1.0 s open window; no weapon renders above a closed chest. |
| Per-room chest/coin persistence | **IMPLEMENTED** | Save v5 stores every room's chests (definition, position, state, timer, pending weapon list) and coins, plus cleared/chest-spawned room sets. |

## Rooms / maps / biomes

| Backlog | Status | Evidence / remaining layer |
|---|---|---|
| MapManagerLevel generation | **IMPLEMENTED** | 5×5 relation grid, `map_long`, main-path walk with backtracking, 70/40/10% branch thresholds, relation values, entrance rule, slot types 0/1/2/3/4, weighted special selection, unique normal templates. 1000 seeds checked for determinism/connectivity/uniqueness. |
| 108 PatternRooms | **IMPLEMENTED** | All 108 `room/r1_N` bundles built by `build_pattern_rooms.py`; runtime auto-discovers every `r1_*_visual` scene. Sizes 15×15, 15×21, 21×15 and 21×21 are preserved by the builder. `LITEKNIGHT_ALL_ROOMS=1 ./scripts/prepare-assets.sh` regenerates the full set. |
| Special rooms | **IMPLEMENTED (native reconstruction)** | Statue two-press purchase, mineral 40 HP/10 drops, mercenary hire/trainer and mount purchase/dismount are wired using recovered fields and deterministic reconstructed placement. Water/bar remain recovered-only. See `SPECIAL_ROOM_INTERACTIONS.md`. |
| Biome system | **IMPLEMENTED** | 16 campaign levels loaded with map_long/chest_level/enemy weights. Default branch A/G1 floor/wall themes are extracted from the actual MapManagerLevel assets and applied to every PatternRoom at runtime with the recovered CreateFloor/CreateWall algorithm. Geometry/collision stay from the PatternRoom. Obstacle-prefab theme switching remains a documented limitation. See `LEVEL_VISUALS.md`. |
| Enemy runtime | **IMPLEMENTED core / PARTIAL presentation** | 101 definitions with biome-weighted spawning, chase/contact, ranged projectiles, AI20 fan, AI11/14 and AI03/18/19 timed bursts, AI09/Frog/Witch dashes, AI13/Frog death blasts and stationary controllers. AI07 attack selection, AI12 fireball arc, AI16 invulnerability window and AI08 loot UI remain data/presentation gaps. See `ENEMY_CONTROLLER_RECOVERY.md`. |
| Dungeon progression | **IMPLEMENTED base** | Start room → normal rooms → transfer-gate end room → next recovered level (map_long and level label update, new deterministic plan, dynamic texture upload). Final 3-6 reports CAMPAIGN COMPLETE. Boss rooms are not present among the recovered PatternRoom bundles. |
| Room dimensions | **IMPLEMENTED** | Builder derives interior bounds from `RGRoomX.room_width/height` and keeps 11/15/21 sizes; 41-unit cell pitch and 18..22 door gaps preserved. |

## Hub / world systems

| Backlog | Status | Evidence / remaining layer |
|---|---|---|
| Workshop materials | **IMPLEMENTED** | 36 `material_*` ItemPickable prefabs extracted with sprites/prices; material inventory persists in save v5. |
| Workshop recipes/crafting | **IMPLEMENTED** | 266 forgeable weapons with exact costs from `WeaponMaterial`, `CustomWeaponMaterial` (additive and replacement) and `DefaultWeaponMaterial`; `UIForge` flow reproduced as a native recipe/material/craft/dismantle UI. |
| Workshop door | **REFERENCE-PROVEN ABSENCE / open** | No door exists to the right of `ItemWeaponStation`; the adjacent object is `chest_material`. The hall passage `room/objects/static/hall/way_research` is serialized inactive, has no collider, and its `object_up_down` `show` bool defaults false — the reference state is open/hidden. |
| NPC speech | **IMPLEMENTED** | All 15 Living Room `characters/cNN` NPCs get hit-area markers, localized `CharacterN_talk1/2` text, speaker names, deterministic line choice and the recovered 2.5 s speech panel. The player's own duplicate NPC and marker are hidden. |
| Pets | **IMPLEMENTED** | 8 recovered pet prefabs (Cat included), `RGPetController` follow distances 2/20, teleport beyond 20, attack cooldown/range/damage, Animator idle/run/attack frames, pet menu, save `pet_id`, cat opens the menu on E. |
| Punching bag | **IMPLEMENTED** | `room/objects/stake` EnemyAIStake, 40 HP reset on every hit, `stake_hit_l` 0.375 s, hit flash/shake/damage number, 30% `StakeHurtEvent` flag, never dies. |
| Boxes | **IMPLEMENTED** | RGBox obstacle variants `box01..box07` with serialized hp=4, per-marker ownership, melee/projectile damage, destruction, collision removal and hit flash. Placement comes from recovered obstacle markers, so boxes never intersect wall cells. |
| Energy bubbles | **IMPLEMENTED** | RGEnergy value 8 / speed 14 / range 6 / `energy` sprite; bubble attraction/absorption, energy gain and pickup audio. Normal box drop path instantiates the recovered energy register pickup (PrefabManager slot 8; medium-confidence prefab identity). |
| Orb system | **IMPLEMENTED** | `ui_52`, Living Room-only HUD, save field, orb trader; developer `O = +100` now requires `--devroom`/`--review`. |
| Dungeon trader | **IMPLEMENTED** | Living Room orb trader is native. Dungeon `r_sell` uses recovered `sell2-1` stock/prices and a native reconstructed container presentation; purchases deduct coins, apply potions, spawn weapons and persist sold ids. See `BOSS_CHEST_AND_SELLER.md`. |
| Room persistence | **IMPLEMENTED** | Enemies, cleared/chest-spawned sets, chest/coin state, materials, pet and player state persist in save v5. |
| Hub collision | **IMPLEMENTED** | Wall tile cells + non-trigger Box/Circle colliders from level2; removed cellar/staircase, signs, Drilmaster, promotion NPCs, lower portals, event decorations and `!` markers. |
| Audio completeness | **IMPLEMENTED core** | 70 unique recovered clips exported under 140 logical ids: UI, pickup, chest, door, transition, player/enemy hit, two BGM tracks, and every `RGWeapon.audio_clip` used by a prefab. Weapon/subclass extra clips (bow draw, charge, throw) are wired for the implemented charge/end-shot events; eat/empty-pot specials remain catalogued only. |
| Save/load completeness | **IMPLEMENTED v5** | character, weapons/slots, HP/armor/energy, coins/orbs, materials, pet, plants, map seed, cleared rooms, chest/coin room state, settings and UI layout. Migration v0/v1/v3/v4 → v5 tested. |
| Living Room map | **REFERENCE-PROVEN ABSENCE** | `UIMap.Start` only shows the map when `levelIndex != 0`; HeroRoom uses the empty `map_A0` manager. The hub therefore shows no minimap; hub areas are scene triggers, not nodes. Dungeon maps exclude the hub and use home/chest/special icons only. See `LIVING_ROOM_MAP.md`. |
| Living Room staircase | **IMPLEMENTED** | Removed cellar stair remains removed. The exposed 3×3 south-wall opening is rebuilt with the surrounding recovered `wall_u`/`wall_m` construction and a matching solid. See `LIVING_ROOM_STAIRCASE.md`. |
| Room title animation | **IMPLEMENTED** | `map_info` rect anchor top-right, animated x 200→−20 over 0.2 s, LockClock 32 label, panel `#20202040`, exact hide/show activation and 3 s show window reproduced at 1920×1080. |
| Room clear animation | **IMPLEMENTED** | `message_bar` SignpostLayer `show_signpost` 1.5 s: black bar, `ui_90` line, `ui_89` gold banner, pixel_bold 96 `clear` text, y 0→−60 and alpha keys/easing reproduced. |

## Texture gallery

The separate `liteknight-texture-gallery` executable enumerates 4481 prepared
visual entries (96 textures, 1921 sprites, 2464 prefabs) with 0 broken assets.
`--validate-all` runs headlessly; see `REFERENCE_TEXTURE_GALLERY.md`.

## Gameplay-loop validation

`--gameplay-smoke` runs the native hub → door → dungeon start → room clear →
chest open/loot → transfer-gate end room → next-level plan → save/load loop and
exits non-zero if any stage fails. A 12 s hub run measured 60.03 Hz present
pacing; the devroom measured 60.56 Hz.

## Exact searches performed

```text
UnityPy AssetBundles/bin Data GameObject/MonoBehaviour/Sprite/Animator/Clip/AudioClip
IL2CPP class/field dump and method disassembly (tools/re/il2cpp_re.py)
RGChest/RGChestAppoint/RGChestBig/RGChestBoss/RGChestRoomReward call paths
RGBox.CreateItem, RGEnergy, PrefabManager, ItemMaterialChest, UIMaterialChest
UIForge/UIWeaponStation/ItemForge/WeaponMaterial/CustomWeaponMaterial/DefaultWeaponMaterial
RGPetController/NpcMercenaryController/PetHand/PetTap, UIChoseHero/UIBtnMaskDown
TalkTrigger/TalkCharNpc/TalkMercenary/ObjectHostess/ObjectPostman/ObjectCatFood
EnemyAIStake.GetHurt/SyncGetHurt, AdvanceToturialManager.StakeHurt
MapManagerLevel.CreateMap/CreateRoom, MapManagerPattern, RGRoomX
UIMap.ShowMap/UpdateMap, UIMessageBar.ShowSignpost, UICanvas.ShowSignpost
localization I2Languages (2,088 terms extracted)
```

Remaining PARTIAL rows are data-limited, not unsearched: unique enemy AI,
79 catalogue-only weapons, boss chest loot and
the dungeon seller inventory. They are listed with the exact missing layer.
