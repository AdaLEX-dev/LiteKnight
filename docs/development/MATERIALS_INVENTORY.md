# Materials, Inventory and Workshop

## Recovered structures

`ItemData` (from `AboGameData`) serializes:

```text
plants, sellerRefreshTime, commodities, itemUnlock, forgeWeapons,
mechInRoom, blueprints, seeds, materials, tokenTickets, restoreKeys
```

Default save sample: `forgeWeapons=[weapon_057, weapon_062, ""]`,
`materials={}`, `seeds={}`, `itemUnlock=[]`.

`ItemPickable` serializes `emItemType`, `count`, `cnName`, `item_level`.
`WeaponItemConfig` on weapon prefabs lists `WeaponItemBase` support items such as
`wi20_golden_paint`, `wi21_time_machine` and `wi22_upgrade_kit`; these are weapon
upgrade/attachment items, **not** the complete forge recipe table.

`UIForge`/`ItemForge` and `UIForge.FlushMaterials` confirm the runtime forge
UI consumes a `Dictionary<GameObject,int>` of material counts. The complete
material definition list and recipe cost table were not recovered in this pass.

## Native state

No material inventory or crafting cost system is implemented. The weapon station
opens the recovered weapon selector; it does not claim to craft. No prices or
recipes were invented. `ItemData.materials` will be the persistence shape when
the authoritative recipe table is recovered.

## Next implementation

1. Extract the material GameObjects referenced by the forge UI and their
   localization names/icons.
2. Recover the actual recipe selection from `UIForge.OnClickButton` /
   `FlushMaterials` or serialized weapon support data.
3. Add `materials` and `forgeWeapons` to SaveData v3+.
4. Require exact material counts before crafting; show insufficient state.
