# Weapon Attachment

## Confirmed hierarchy

The serialized c00 prefab contains an `img` transform with three actual
`RGHand` mounts:

```text
img/h1  local (0.00, 0.50)   primary hand
img/h2  local (0.55, 0.60)   off-hand mount
img/h3  local (0.15, 0.85)   third hand mount
```

The weapon prefab’s visual child is `w` at local (0,0), with `gun_point` as its
muzzle child. The sprite pivot is near the grip/handle (for example
`weapons_19` pivot `(0.036, 0.38)`, `weapons_21` `(0.30, 0.45)`).

## Native rendering

- Primary weapon is placed at `h1` (0, 0.5), not a guessed ±0.2 hand offset.
- Weapon rotation uses the aim angle around the sprite pivot.
- The off-hand/second weapon is drawn at `h2` (0.55, 0.6), mirrored when the
  body faces left.
- Weapon depth is sorted using the character’s feet Y with a +30 tie break.
  Using the hand’s higher Y previously lowered the weapon by 50 depth units and
  hid it behind the body; that was the missing-gun bug.
- Left-aimed weapons use `flip_y` to stay upright, matching the confirmed
  `FlipWhenRotate` convention used by bullet sprites.

## Known gaps

- Weapon Animator idle/fire/recoil curves are not yet applied; the reference
  `w_ide` and attack clips are extracted as cadence evidence only.
- Per-weapon hand offsets beyond h1/h2/h3 are `UNKNOWN`.
- `GunInitKnightXXSkillListener.handPositionDic` has not been fully applied to
  all skins; current h2/h3 mounts are from the c00 prefab.
