# Weapon Catalogue and Runtime Integration

**Status:** full reference catalogue recovered; 278 standard projectile/melee weapons
are loaded by the native runtime, with the remainder explicitly classified as
Tier B/C. See `WEAPON_SYSTEM.md` for the pipeline and rarity finding.

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
Weapons with complete standard/melee runtime: 278  (Tier A)
Melee prefab extraction pending:               3  (Tier B)
Unique / catalogue-only:                      21  (Tier C)
Recovered init/skin weapon prefabs:            26 runtime definitions
```

Tier A is generated automatically from every prefab with a valid visual and
either a projectile bullet or a serialized melee definition. Bullets may render
on the root or a child; bow/support weapons without Attack events use the
serialized attack clip duration. Each entry carries the
localized name, actual `consume`, first-bullet damage, projectile
id/sprite/speed/lifetime, visual, gun point, `multiCount` and Attack cadence.
Tier B/C are not silently given generic behaviour; they remain catalogue/UI data
until their actual melee/unique mechanic is reconstructed.

## Integrated weapons

The generated list is authoritative in
`analysis/assets/weapon_runtime_integration.md`; it contains all 168 Tier A ids,
names, damage and energy costs. The first integrated set includes the original
15 plus automatic extraction of the standard projectile catalogue.

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
