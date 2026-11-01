# Knight Ability UI and Blue Charge

## Recovered reference mechanism

`UICanvas` serializes:

```text
private Image skill_icon_mask;   // +0xA8
private Image skill_mask_ready;  // +0xB0
```

`UICanvas.UpdateSkillMask(thisSkillTime, cd, multiCount, isPowerUp)`:

- enables the skill mask parent and `skill_icon_mask`;
- toggles `skill_mask_ready` when `multiCount > 0` / readiness;
- computes `fillAmount = thisSkillTime / cd`;
- applies a ready/power color;
- uses the actual dark/white lightning sprites `ui_joy_2` / `ui_joy_3`.

This is a recovered `Image.fillAmount` mechanism, not a hard-coded spinner.

## Native implementation

The HUD uses the same recovered sprites:

- `ui_joy_0`: circular button base
- `ui_joy_2`: dark lightning mask
- `ui_joy_3`: white lightning fill
- `ui_joy_7`: blue-diamond recharge marker

The `ui_joy_3` sprite is clipped vertically by `AbilityChargeFraction`:

```text
Empty     0%
Cooldown  1 - remaining/cooldown
Active    100%
Ready     100%
```

The clipped fill is blue (`#379BFF`), with a lighter active tint and a subtle
blue ready ring. The old native cooldown bar was removed because the reference
uses the icon fill mask.

`AbilityChargeFraction` is unit-tested for empty/cooldown/ready/active states.
`Q` remains zero-energy for the recovered Knight Dual Wield (5 s active, 10 s
cooldown).

The native ability now instantiates a real second `Weapon`: it consumes energy,
fires at the recovered cadence, and is rendered at the c00 `h2` off-hand mount.
It is not just a duplicate sprite.

## Current limitation

The exact reference `Image.Type`/fill origin (`Horizontal`/`Vertical`/`Radial`)
is not serialized in the recovered object fields. The native implementation uses
the equivalent vertical bottom-up fill; this is `HIGH CONFIDENCE` for the
mechanism and `HYPOTHESIS` for the exact fill direction.
