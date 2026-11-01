# Chest System

**Status:** 13 reference chest classes catalogued; three chest definitions are
fully integrated with actual visuals, a 1.0 s open clip and reference-derived
loot selection. Other chest classes remain partial/classified.

## Recovered catalogue

`tools/asset/audit_chest_catalogue.py` searches every AssetBundle and records
GameObjects carrying `RGChest*`/`ItemChest*` components:

```text
chest GameObjects discovered: 86
distinct classes:             13
with visual children:         49
with Animator clips:           1 root (chest00/01/02 share the `chest` controller)
```

Classes found:

```text
ChestClosableSeller
ItemChestBattery
ItemChestBig
ItemChestClosable
ObjectRoomChest
RGChest
RGChestAppoint
RGChestBig
RGChestBigButton
RGChestBoss
RGChestMaterial
RGChestRoomReward
UIMaterialChest
```

Evidence: `analysis/assets/chests/chest_catalogue.json/.md`.

## Runtime chests (integrated)

| ID | Reference class | Loot | Visual |
|---|---|---|---|
| `chest00` | `RGChestAppoint` | fixed appointed `weapon_021` (one recovered variant) | actual box03/04/05/06 + shadow |
| `chest01` | `RGChest` level 1 | `WeaponDropInfo` level 1 weighted entry | actual chest01 visuals |
| `chest02` | `RGChest` level 2 | `WeaponDropInfo` level 2 weighted entry | actual chest02 visuals |

- Animator `chest_open` duration **1.0 s**; `chest_close` **0.4666667 s** at
  30 fps. The old native 0.45 s lid lift was wrong and is retired.
- Chest loot selection is deterministic under a room/chest seed and uses the
  reconstructed `RGRandomStream`.
- `TakePendingWeaponDrops()` gives Application a real weapon id; the runtime
  spawns a normal weapon pickup if that weapon is Tier A. If it is not bundled,
  it is logged as catalogue-only rather than silently replaced.
- Repeated interaction cannot duplicate loot; room transitions preserve opened
  state in memory.
- `chest00` is a fixed `RGChestAppoint` variant. The recovered chest00 variants'
  fixed objects include `weapon_021`, `weapon_034`, `weapon_049`, `weapon_062`
  and `restore_pot_max`. The exact room-to-variant association remains UNKNOWN.

## Loot tables

Recovered `WeaponDropInfo` levels: **7** (0..6), with weighted weapon entries.
The item pack contains those entries as `weapon_drops`. Level 0/1/2 are used by
the integrated chests.

Still UNKNOWN / not claimed:

- `RGChestBoss`, `RGChestBig`, `RGChestMaterial`, `RGChestRoomReward`,
  `ItemChestBattery` and seller distributions beyond fixed `object_list`/count
  fields.
- Rarity tables and room/difficulty modifiers not referenced by the recovered
  chest methods.
- Save-v3 persistence of per-room opened chests and world coin state.

## Review harness

`./scripts/test-explore.sh chest` launches r1_1 with chest00/01/02 in a review
row. `E` opens; the card shows the actual loot name/stats. The 30-second freeze
and ENTER confirmation remain in force.
