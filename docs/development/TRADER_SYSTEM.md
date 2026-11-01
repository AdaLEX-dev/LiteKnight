# Orb Trader

## Reference structure

- `ItemRoomSeller` (RGStateItem) owns a queue of `ItemSellPlaceGem` slots,
  refresh timing and a talk transform.
- `ItemSellPlaceGem` serializes `index`, `emItemType`, `minLevel`, `maxLevel`,
  `unlockByTv` and conditional object distributions.
- `RGSaveManager` stores the Living Room currency in `AboGameData.gems`
  (`gems`, `lastGems`, `varGems`) and exposes `ConsumeGem`/`GetGem`.
- `gems_icon` is the actual blue sprite `ui_52`.

The three `room_seller/container0/1/2` platforms and `npc01` exist in the
recovered HeroRoom hierarchy.

## Native implementation

- **Orbs:** separate `player_orbs_` count and SaveData v3 field. HUD shows the
  actual `ui_52` blue gem icon left of the coin counter, only in the Living Room.
- **Offers:** three actual integrated weapons selected deterministically from
  the recovered `WeaponDropInfo` levels 1/2/3 (fallback: any integrated weapon
  with a positive serialized value).
- **Price:** `RGWeapon.item_value` (the recovered `GetItemValue` data), not an
  invented price.
- **Interaction:** one `trader_station` marker in front of the three platforms;
  E opens the trader panel.
- **Purchase:** Enter buys the selected offer, deducts orbs, marks it SOLD and
  spawns a normal weapon pickup. Insufficient orbs are rejected.
- **Developer control:** `O` grants +100 orbs while in the hub so purchase flow
  can be reviewed without a cheated default economy.

## Confidence

- `CONFIRMED`: trader classes, three platform hierarchy, gems icon, item value
  field, gems currency fields.
- `HYPOTHESIS`: native use of `WeaponDropInfo` levels for the three offers and
  the orb price mapping; the exact `ItemSellPlaceGem` random distribution was not
  fully decompiled.
