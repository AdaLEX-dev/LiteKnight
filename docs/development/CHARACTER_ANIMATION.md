# Character Animation and Weapon Holding

## Recovered c00 animation set

The common-bundle `char_0_skin_0` AnimatorController contains only:

```text
char_1_skin_0_ide   (idle)
char_1_skin_0_run   (run)
char_1_skin_0_dead  (death)
char_hit            (hit)
```

There are **no separate up/down/diagonal body clips** in the recovered c00
controller. Left/right is a horizontal mirror of the same side-view clips.
`FacingDirection` therefore intentionally exposes only Left and Right.

## Movement / aim separation

```text
movement direction  -> body facing while moving
aim direction       -> weapon orientation and idle body facing
body facing         -> horizontal sprite mirror
animation state     -> idle/run/dead from actual movement
```

`ResolveBodyFacing()` is unit-tested:

- moving left wins over aim pointing right;
- vertical movement keeps the last horizontal body facing;
- when idle, body facing follows aim with hysteresis.

The 360° weapon aiming remains independent and Auto Aim remains the fresh
default.

## Idle / walking weapon holding

The weapon prefab root and child `w` Transform have localScale 1.0. The grip is
placed at the recovered hand-local `(0.2,0.55)` concept on the body-facing side;
the weapon rotates around that grip. The previous native behavior rotated the
whole hand offset with the aim vector, which caused the walking left/right
wobble and exposed mostly the barrel.

The render aim direction is interpolated between fixed simulation states using
the same frame alpha as the player/camera, so 50 Hz simulation does not produce
visible 60 Hz aim jitter.

Muzzle position (`gun_point`) and projectile math are unchanged.

## Evidence boundary

`CONFIRMED`: c00 clip set, side-view-only animation, prefab scale/pivot,
horizontal mirroring, character hand-local concept.

`HYPOTHESIS`: exact Knight hand mount object (the explicit serialized hand
objects recovered so far belong to wolf prefabs), native hand offset and the
final visual scale.

## Review

`./scripts/test-explore.sh living_room` and `r1_1` are the visual review states.
Watch for body flip vs movement, weapon grip stability, foreground/weapon
sorting and projectile muzzle alignment.
