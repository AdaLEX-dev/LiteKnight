# Boss Chest and Dungeon Seller

## Boss chest

### Recovered evidence

- `RGChestBoss.Start @0x1170F78`:
  `finalLevel = max(5, level + MapManagerLevel.chest_level)`.
- `OpenChest @0x1171064`: `RGRandom.Range(0,100) <= 24` and a non-empty
  `object_list` uses a fixed boss weapon; otherwise
  `WeaponDropInfo.GetWeaponWithDropLevel(finalLevel, rg_random, null)`.
- Fixed boss weapon rolls use `UnityEngine.Random`, not `RGRandom`.
- `MelleValid @0x11713E8` gates the fixed path under
  `emBattleFactor.MelleOnly`.
- Boss chests produce one weapon, no coins/potions.

### Implemented

- `build_item_assets.py` now extracts `chest_boss01..14`, `boss`, `big`, and
  the exact `object_list` mapping:
  `chest_boss01→weapon_077`, `02→145`, `03→116`, `04→141`, `07→134`,
  `08→123`, `09→122`, `10→127`, `13→175`, `14→176`.
- `ChestDefinition` carries `boss`, `big`, `object_list`; `ItemWorld` implements
  the 25% fixed path, level-5/6 sample pool and deterministic room/chest seed.
- `RGChestBig` now rolls `weapon_count` items with the recovered 50% one-level
  reduction (clamped 0..5).
- `RGChest` now uses the reference final-level formula
  `level>=6 ? 6 : max(5, level + chest_level)`, not the raw prefab level.
- Tests: `item_test` covers boss fixed/pool output and big-chest item count.

### Remaining limitation

- No recovered boss room Bundle exists among the 108 PatternRooms, so boss
  chests are data-complete and runtime-complete but not placed in a campaign
  boss room. No boss room is fabricated.

## Dungeon seller (`r_sell`)

### Recovered evidence

- `r_sell` is a `level-common` room prefab using `RGContainer` +
  `ItemSellPlaceAttrbute`.
- Exact stock:
  - `sell1-1-2`: restore_pot 5, restore_pot_big 1.
  - `sell1-2`: same.
  - `sell1-3`: same + material_skin_fragment_shop 50 (conditional).
  - `sell2-1/0`: health_pot 5, health_pot_big 1.
  - `sell2-1/1`: energy_pot 5, energy_pot_big 1.
  - `sell2-2/0`: health_pot 5, health_pot_big 1.
  - `sell2-2/1`: energy_pot 5, energy_pot_big 1.
  - `sell_multi/0`: health_pot 5, health_pot_big 1.
  - `sell_multi/1`: energy_pot 5, energy_pot_big 1.
- `read_xml=1` containers roll a weapon via `WeaponDropInfo`, not XML stock.
- Price adjustment: 4-player factor and ExpertShop buff 10.
- `ItemSellPlaceAttrbute` mystery sellers charge HP, not coins:
  `ceil((item_value-5)/20)`, clamped 1..3 (6 under Inflation), ExpertShop
  halves.
- Mystery stock: `ball_buff` weight 1; weapon containers at levels 3/3/6.

### Status

**WIRED (native reconstruction).** A real `r_sell` room shell was built from
the recovered `r_sell` prefab (19x13 geometry, doors, walls, level themes) and
added to the special-room pool. The exact `sell2-1` stock is used:

- `health_pot` 25 coins, +2 HP
- `energy_pot` 20 coins, +80 energy
- one `read_xml=1` weapon container from the recovered level 5/6 pool, priced
  by the weapon’s recovered `item_value`

The container transforms are Odin-serialized, so the three positions are
explicitly labelled **NATIVE RECONSTRUCTION** on the recovered room floor.
Purchase deducts coins, applies potion restoration, spawns weapon pickups,
marks the offer sold, plays `fx_buy`, shows the dialogue/feedback, and persists
sold ids in `sold_shop_items`.

### Remaining limitations

- The exact `RGContainer`/`ItemSellPlaceAttrbute` container transforms are not
  decoded; positions are reconstructed, stock/prices are exact.
- Shop refresh timing is not decoded; sold offers persist for the run.
- The full `r_sell` random layout pool (mystery/HP sellers) is recovered but
  only `sell2-1` is implemented as the native presentation.
