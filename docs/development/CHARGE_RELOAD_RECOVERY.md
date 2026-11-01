# Charge / Reload Weapon Recovery

## Reference evidence

All addresses are RVAs in `libil2cpp.so`; values are serialized prefab fields.

- `Gun020` (`ChargeWeaponInterface`): `max_time`, `a_size_max`, `force`,
  `reflectCount`, `clip_hold`, `clip_shoot_max`, `reload_obj`.
  `CreateBullet @0x01431F5C`; no minimum charge threshold.
- `Gun007` (`ChargeWeaponInterface`, `IDelayReleaseWeapon`): `max_time`,
  `multiCount`, `continuousCount`, `a_interval`, `angle`, `inactiveTime`,
  `clip_hold`, `clip_big`. `<CreateBullet>d__35.MoveNext @0x01245CEC`.
- `Gun005` (`ChargeWeaponInterface`, `IDelayReleaseWeapon`): `max_time`,
  `s_ide`, `s_atk`, `clip_hold`, `reload_obj`. `Attack @0x012432B0` reuses the
  held arrow and calls `RGArrowThroughTrigger.SetPercentage(p)`.
- `Gun011`: normal animation-event shot plus a free end shot after
  `shoot_end_time`; `clip_end_shoot`.
- `ReloadClip.PlayReloadAnim @0x00D37B4C`: `anim.speed = 1 / time`, trigger
  `play`; the 1.0 s clip event calls `FlnishReload`, which only clears the
  reloading flag. There is **no magazine/ammo refill**.

## Implemented native subset

- `WeaponSpecial` carries `duration`, `hold_speed`, `a_size_max`, `force`,
  `reflect_count`, `multi_count`, `continuous_count`, `a_interval`, `angle`,
  `inactive_time`, alternate projectile variants and clip names.
- `Weapon::UpdateCharge` accumulates charge at `dt * hold_speed * attackSpeed`,
  auto-releases at `max_time`, fires at zero charge (no invented threshold),
  consumes energy once per release, and applies the recovered 0.1 s lockout.
- `FireCharged` implements:
  - Gun020: base projectile below full charge, `bulletInfo2` at full charge,
    damage `base + p*B1.damage`, `a_size_max` at full.
  - Gun007: `floor(p * (a_interval==0 ? multiCount : continuousCount))`
    projectiles; angle spread only when `a_interval==0`; speed/damage
    interpolation; `through=999` above `p=0.6`.
  - Gun005: damage and speed interpolation, held-arrow release semantics
    represented by the recovered B1 stats.
  - Gun011: normal animation-event shots continue; release after
    `shoot_end_time` spawns the free B1 end shot.
- Audio events `charge_start`, `charge_release`, `charge_full` and `end_shot`
  drive `clip_hold`, base `audio_clip`, `clip_shoot_max`/`clip_big` and
  `clip_end_shoot`.
- `reload_obj` is treated as a charge-progress visual only. No ammo model is
  invented.

## Per-weapon data

The builder extracts every `Gun020` (1), `Gun007` (11), `Gun005` (16, including
init bows), `Gun011` (2) and `GunPot` (3) prefab. `GunPot` remains a normal
projectile definition because its drink/empty state is a separate consumable
mechanic; no invented healing behavior is attached.

## Tests

`tests/unit/unique_weapon_test.cpp` covers:

- Gun020 partial vs full projectile switch and damage formula.
- Gun007 full-charge multi-burst, 15° spread and 999 through.
- Gun005 damage/speed interpolation.
- zero-charge release (no minimum threshold).
- Gun011 normal event shot plus free end shot.

## Remaining limitations

- The `reload_clip` visual/animation is not rendered; its timing and
  `FlnishReload` semantics are documented, and charge accumulation is timed
  from the same serialized `max_time`.
- `GunPot` drink/heal/empty-state behavior and `GunThrowSword` mode switching
  remain explicitly partial; neither is replaced with an invented generic
  mechanic.
- `clip_big` selection in Gun007 uses the recovered 0.5/size-sign condition
  only for audio; the projectile formula is fully implemented.
