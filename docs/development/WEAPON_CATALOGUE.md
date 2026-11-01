# Weapon Catalogue and Runtime Integration

**Status:** full reference catalogue recovered; 15 standard weapons are loaded by
the native runtime, with the remainder explicitly classified as Tier B/C.

## Sources

- `WeaponInfo` TextAsset: internal id, title, item level, drop level, type,
  forgeability and tags (`analysis/apktool_out/assets/bin/Data/51df...`).
- Localization table: 301 English/localized names (`65e9...`).
- Common-bundle weapon prefabs: serialized `Gun001`/`consume`/`bulletsInfo`,
  weapon `w` SpriteRenderer, `gun_point`, attack Animator state and projectile
  prefab.
- `WeaponDropInfo` TextAsset: 7 weighted drop levels
  (`45fd...`, recovered in the item pack).
- Audit outputs:
  - `analysis/assets/weapon_catalogue.json/.md`
  - `analysis/assets/weapon_runtime_integration.json/.md`
  - `analysis/assets/weapon_energy_audit.json/.md`

## Counts

```text
Catalogue entries discovered:              302
Weapons with complete standard runtime:     15  (Tier A)
Weapons with stats pending assets/cadence: 281  (Tier B)
Weapons requiring unique/non-bullet logic:   6  (Tier C)
```

Tier A is the set bundled into each native room scene: correct localized name,
actual `consume`, first-bullet damage, projectile id/sprite/speed/lifetime,
weapon visual, gun point and the first non-idle Attack AnimationEvent cadence.
Tier B is not silently given generic behaviour; it remains catalogue/UI data
until its visual, projectile and cadence pack is bundled.

## Integrated weapons (CONFIRMED)

| ID | Player name | Damage | Energy | Cycle (s) | Projectile |
|---|---|---:|---:|---:|---|
| `weapon_000` | Bad Pistol | 3 | 0 | 0.3333 | `bullet_0` |
| `weapon_001` | P250 Pistol | 3 | 0 | 0.2778 | `bullet_0` |
| `weapon_002` | AK-47 | 3 | 1 | 0.1667 | `bullet_14` |
| `weapon_003` | Shotgun | 3 | 3 | 0.8333 | `bullet_14` |
| `weapon_004` | Shotgun Pro | 3 | 4 | 0.8333 | `bullet_14` |
| `weapon_007` | Snow Fox L | 2 | 0 | 0.1667 | `bullet_11` |
| `weapon_010` | Desert Eagle | 4 | 1 | 0.3333 | `bullet_1` |
| `weapon_015` | Revolver | 8 | 3 | 1.0 | `bullet_14` |
| `weapon_017` | Assault Rifle | 3 | 1 | 0.1667 | `bullet_0` |
| `weapon_021` | Blaster | 3 | 1 | 0.1667 | `bullet_15` |
| `weapon_034` | Glacier | 10 | 4 | 0.8333 | `bullet_2` |
| `weapon_048` | Next-gen SMG | 3 | 1 | 0.5 | `bullet_11` |
| `weapon_049` | Next-next-gen SMG | 2 | 2 | 0.5 | `bullet_12` |
| `weapon_062` | Machete | 6 | 0 | 0.5 | `sword_1_75` |
| `weapon_287` | Revolver Gold | 4 | 1 | 0.1333 | `bullet_14` |

The native pickup card and weapon station always use the localized name, never
`weapon_xyz`.

## Energy / resource rules

`Gun001.consume` is the per-attack energy cost for the recovered gun prefabs.
`RGWeapon.MakeConsume` and `MultiHand.CheckEnergyEnough` identify the energy path.
Zero-cost weapons are real; ammunition/special resources are not claimed for
weapons whose unique implementation has not been reconstructed.

## Visual / hand binding

- Weapon prefab root and child `w` localScale are `(1,1,1)`; the previous 1.5×
  experiment is retired.
- `gun_point` is the muzzle local offset; muzzle/projectile math is unchanged.
- The native hand anchor uses the recovered hand-local `(0.2,0.55)` concept and
  places the grip on the body-facing side. Weapon rotation is independent.
- The c00 hand attachment for the Knight remains a native composition because
  the explicit runtime hand object is not serialized on the Knight prefab.

## Runtime path

```text
catalogue asset metadata
  -> SceneLoader WeaponDefinition[]
  -> WeaponRegistry
  -> WeaponInventory / weapon station / pickup
  -> Weapon + ProjectileSystem
```

## Remaining work

- Tier B visual/projectile/cadence bundles.
- Unique weapon mechanics (charge, chain, bouncing, special resources, etc.).
- Full per-weapon damage modifiers beyond the ordinary default case.
- Rarity codes from the game files applied to pickup UI.
