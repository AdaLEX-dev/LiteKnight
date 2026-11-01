# Melee Recovery

## Result

- 82 melee definitions were already recovered from serialized `is_melee`,
  `swordScale`/`meleeDistance`, Attack events and animator clips.
- The three former Tier B melee prefabs are now integrated:
  - `weapon_216` Bayonet Rifle: `GunThrowSword`; rifle `bullet_12` plus
    thrown/melee `spear_1_5` (`RGSword`, Metal/Melee damage), recovered
    `meleeDistance=3.5`, `force=-15`. Attack events: rifle `w_rifle` 0.033 s;
    Bayonet override `w_bayonet_prepare` Prepare 0; base `w_spear` 0.1 s.
  - `weapon_214` Extra Crown: `GunCrown : GunShield`; `is_melee=1` is the hold
    pose, not a sword swing. The actual mechanic is the serialized
    `BuffStealth` (10 s, alpha 0.297).
  - `weapon_242` Box: `GunBox`; `is_melee=1` hold pose, actual mechanic is a
    deployable `RGBox` (HP4) at the placement position.
- No arbitrary attack timings were invented. Where a prefab has no Attack
  event (Trekking Pole/Nirvana Cross), the weapon is classified passive rather
  than given a fake swing.

## Runtime

- `EnemySystem::ApplyMelee` remains the shared frontal-arc query used by all
  swords/axes/spears and the Knight offhand.
- `WeaponDefinition` carries the recovered `melee_range`, `melee_arc_degrees`,
  damage and attack events; `Weapon` fires on the serialized event times.
- `weapon_216` uses the generic primary projectile path plus its recovered
  thrown-spear variant; the mode-switch condition remains partial and is
  documented rather than guessed.
- `weapon_214` and `weapon_242` emit special events consumed by the buff and
  deployable-box paths instead of being forced into `ApplyMelee`.

## Tests

- `unique_weapon_test` covers the burst/charge/projectile formulas used by the
  hybrid.
- `room_visual_test` and the weapon catalogue tests cover load/classification.
- The gameplay smoke test exercises the shared melee query against enemies and
  the training stake.

## Remaining limitation

- `GunThrowSword` mode selection (distance/energy `get_melee`, `CheckMode`) is
  recovered but the native runtime currently fires the rifle path and stores
  the thrown-spear variant; the exact manual mode switch is not exposed as a
  separate input. No alternative attack timing is fabricated.
