# Weapon System

**Status:** the recovered catalogue is fully loaded at runtime. Standard
projectile weapons are extracted automatically; melee and unique-mechanic
weapons remain explicitly classified instead of silently receiving Bad Pistol
behaviour.

## Evidence

- `WeaponInfo` TextAsset: 349 rows, 302 catalogue ids, localized names.
- 296 serialized `weapon_###` prefabs in the common bundle.
- Each accepted prefab supplies `RGWeapon`/`Gun*` fields, `bulletsInfo`,
  bullet prefab, `w` sprite, `gun_point`, weapon Animator and first `Attack`
  AnimationEvent.
- `WeaponDropInfo` supplies 7 weighted drop levels.
- `emWeaponType`, `emWeaponTag`, `is_melee`, `forgeable`, `item_value`,
  `item_level` and `weapon_speed` are retained.

## Rarity

There is **no dedicated `WeaponRarity` field** in `WeaponInfoRow` or
`RGWeapon`. The available coded fields are:

```text
WeaponInfoRow.level       (0..5 in the catalogue)
WeaponInfoRow.dropLevel
RGWeapon.item_level
RGWeapon.item_value
```

The native runtime exposes `WeaponInfo.level` as `rarity` and preserves
`drop_level`, `item_level` and `item_value`. Calling this a separate rarity enum
would be `CONTRADICTED` by the recovered fields.

## Runtime classification

```text
Catalogue entries:                 302
Tier A standard/melee runtime:     278
Tier B melee prefab pending:         3
Tier C unique or catalogue-only:    21
```

Tier A weapons are automatically extracted; the pipeline is not a hand-written
list. Every accepted scene weapon carries its serialized display name, class,
damage, energy cost, projectile, speed, cadence, visual, muzzle, spread,
`multiCount` and melee range/arc when present. The generic melee runtime uses a
frontal arc query and recovers `atk`/`swordScale`/`meleeDistance` where serialized.

Tier B/C entries are catalogue-only until their actual mechanic is reconstructed.
They are never substituted with `weapon_000`.

## Firing

`Weapon::Update` follows the recovered Attack-event cadence. `multiCount` is
used as the native bullet count; the serialized `angle`/`deviation` supplies the
spread for multi-shot weapons. Shotgun extraction therefore produces five/six
actual bullets rather than a single rifle bullet. Melee weapons use the same
Attack events but apply a shared range/arc hit query instead of a projectile.

## Remaining

- Per-weapon Animator recoil/fire/reload curves are not yet applied.
- Charge, beam, throw, chain, boomerang, summoning and other unique mechanics
  remain Tier C.
- Per-weapon audio clips are not yet wired; a generic recovered gun sound is used.
