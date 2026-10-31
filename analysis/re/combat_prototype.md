# Native Combat Prototype — Evidence and Approximations

**Scope:** first playable combat slice in the recovered `r1_1` dev room:
player attack input → weapon → projectile → collision → damage → enemy death →
player damage/death → developer reset. It is not a full combat reconstruction.

**Reference:** Soul Knight 2.6.8, Unity 2018.4.12f1, IL2CPP metadata v24.
**Binary:** `analysis/apktool_out/lib/arm64-v8a/libil2cpp.so`.
**Assets:** `common` (`weapon_000`, `bullet_0`, `c00..c14`) and
`level-objects` (`e_slime01`).

---

## 1. Attack input (reference vs native)

### Recovered path

```text
RGJoyControl.UpdateBtnAtkClick (0xDD87FC)
    UnityEngine.Input.GetButtonDown / GetButtonUp
    -> RGController.BtnAtkClick(bool) (0x117C3DC)
    -> RGHand / current weapon attack path
    -> RGWeapon.SetAttack(bool, manual) (0xF89904)
    -> AttackKeyDown / AttackKeyUp (0xF89C18 / 0xF89C70)
    -> AttackStart / AttackStop (0xF89D54 / 0xF89E08)
    -> set_atk_b(bool) -> weapon Animator parameter
    -> Gun001.Attack (0x1241260) from the w_pistol AnimationEvent
```

`RGJoyControl.UpdateBtnAtkClick` reads a fixed Unity input button (the exact
configured button-name string is not resolved) and calls `BtnAtkClick(true)` on
down and `false` on up. The multiplayer branch calls
`NetPlayerController.RoleAtk` instead. `RGController.BtnAtkClick` routes to the
current hand/weapon for the normal non-special case.

**Confidence:** CONFIRMED for the method path and bool direction; **UNKNOWN** for
the serialized Unity button name. The reference is a mobile/joystick button, so
there is no native keyboard parity requirement.

### Native test binding

- `J`, `Space`, and left mouse button: attack held.
- Mouse cursor, right stick, or keypad `8/2/4/6`: aim vector (native test
  binding; facing remains movement-driven).
- No debug-control conflict (`P`, `O`, `C`, `R`, `1..0`, `[`/`]` remain).
- The input is sampled once per render frame and consumed by fixed steps.

This is explicitly a **NATIVE TEST BINDING**, not a claim of reference keyboard
parity.

---

## 2. Weapon (`weapon_000` / `Gun001`)

### Serialized values

```text
bulletProto       bullet_0
speed             36.0
size              1.0
damage            3
critic            0
repel             2
throughCount      0
deviation         5
consume           0
weapon_speed      1.0
atk_move_speed    -0.1
```

`weapon_000` is a CONFIRMED `Gun001` prefab. It is **not** proven to be the
Knight's starting weapon or the display name "Bad Pistol".

### Fire path

```text
Animator event "Attack" (w_pistol)
    -> Gun001.Attack (0x1241260)
    -> RGWeapon.get_bullet_speed -> BulletInfo.speed 36
    -> Gun001.CreateBullet(speed, RGBaseController.fixed_angle) (0x124131C)
    -> BulletInfoInspector.GetBulletInfo (0x14BFF30)
    -> BulletFactory.TakeBullet (0x14BEB40)
```

`Gun001.CreateBullet` reads the controller's `fixed_angle` (`RGBaseController`
`+0x38`), not the movement vector directly. `RGController.AutoLock` sets
`fixed_angle` from a locked target or the control/aim input. This is the evidence
that aim, facing and movement are separate concepts.

### Attack period (CONFIRMED asset evidence)

`weapon_000` uses AnimatorController `weapon_pistol`:

- default state `w_ide` (0 s),
- attack state `w_pistol` (loop, duration **0.333333343 s**),
- one AnimationEvent `Attack` at **0.033333335 s**.

`RGWeapon.Start` (`0xF88BC0`) sets
`Animator.speed = weapon_speed * controllerAtkSpeed * _speedFactor *
_buffAtkSpeedFactor`. The serialized `weapon_speed` is 1.0; `RGWeapon`'s
constructor initializes `_speedFactor` and `_buffAtkSpeedFactor` to 1.0
(`0xF919CC`), and `RoleAttributePlayer`'s constructor initializes `atk_speed` to
1.0 (`0xD41564`). Therefore:

```text
shot interval  = 1/3 s
first shot     = 1/30 s after entering the attack state
```

Native uses exactly those values in `WeaponDefinition` metadata, applies them on
the 50 Hz fixed tick, and keeps firing while attack is held. The first shot can
therefore appear on the second 0.02 s tick.

### Spread / deviation (`deviation = 5`) — CORRECTED

`Gun001.CreateBullet` computes:

```text
spread = RoleAttribute.deviation * weapon.deviation
       + RoleAttribute.deviation
       = RoleAttribute.deviation * (weapon.deviation + 1)
```

The field read at `RoleAttribute +0x2C` is `deviation`; c00 does not serialize
it and `RoleAttribute..ctor` leaves it at 0. Therefore weapon_000's effective
spread is **0 degrees** for c00. The serialized weapon deviation 5 is a multiplier
on the player's deviation, not a standalone ±5°.

**Confidence:** CONFIRMED for the arithmetic and field offsets; the exact
`RGRandom.Range` stream seed is not serialized, so the native seed is a test
binding.

### Ammo, recoil and status

- `consume = 0`: no energy/ammo cost for weapon_000.
- `critic = 0`: no critical roll/effect from this BulletInfo. The player's
  `RoleAttribute.critical = 5` is not applied by this weapon path.
- `atk_move_speed = -0.1`: `AttackStart` adds this to
  `RoleAttributePlayer.speed_rate`; `AttackStop` subtracts it. Native applies the
  same modifier while attack is held, reducing effective speed from 9.75 to 9.1.
- No positional recoil was recovered. `atk_move_speed` is a movement modifier,
  not a knockback impulse.

### Muzzle position (APPROXIMATION)

The recovered prefab exposes c00 hand `h1` local `(0, 0.5)` and weapon_000
`gun_point` local `(0.6, 0.15)`. The native composition rotates the gun-point
offset by the aim angle and adds the hand offset. The exact animator-driven hand
transform is not modeled, so this is a documented approximation. It is never
derived from sprite dimensions.

---

## 3. Projectile (`bullet_0`)

Serialized `Bullet01`:

```text
awake           1
camp            1
can_rebound     1
critical        0
damageType      0
destroy_time    5.0
repel           0
rotate_angle    0
speed           10.0
```

`BulletInfo.speed` 36 overwrites the prefab speed 10 through
`RGBullet.UpdateInfo` (`0x1168000`). Native preserves both values: the
`ProjectileDefinition` keeps `prefab_speed = 10`, while spawned projectiles use
the weapon's 36. Lifetime comes from `destroy_time = 5.0`.

Collider: child `b` `BoxCollider2D` trigger, size `(0.5, 0.4)`, offset
`(0.25, 0)`. Sprite `bullet_5`, sorting layer 6 / order 0. Native uses the same
sprite and a swept AABB with those extents; the recovered offset is rotated with
the projectile and included in the swept collider center.

Movement: the exact `RGBullet` Rigidbody2D velocity composition is not fully
decoded. Native uses deterministic fixed-step linear movement through the same
50 Hz clock. This is an explicit native approximation, not a claim of exact
Unity Physics2D parity. `can_rebound`, `through = 0` and `repel = 2` are retained
as metadata; rebound/knockback integration is not implemented.

---

## 4. Projectile collision

Recovered target bridges in `RGBulletTrigger.OnTriggerEnter2D` (`0x1160324`):

```text
box       -> RGBox.Hit (vtable +0x168)             HIGH CONFIDENCE
player    -> RGController.GetHurt (+0x228)         CONFIRMED
enemy     -> RGEController.GetHurt (+0x2C8)        CONFIRMED
```

The first branch compares a special tag and handles a non-damage hit (exact tag
string unresolved). Native treats recovered room walls, obstacles and doors as
blocking geometry: a projectile stops at the first static solid. Enemy targets
are checked against the same swept segment, and a target behind a recorded
wall-hit fraction is rejected. This keeps bullets from passing through room
geometry or damaging through walls.

Dying enemies are excluded from target selection after `RGEController.Dead`
disables their collider. Native does the same by skipping `health.dead` enemies.

**Confidence:** CONFIRMED for the three target bridges; HIGH CONFIDENCE for
static geometry blocking; PARTIAL for the exact tag/effect-trigger branch.

---

## 5. Damage

### Player (`RGController.GetHurt`, `0x11795CC`)

Recovered core:

```text
if (!awake || !can_hurt) return
damage += BattleData.isBadass ? 1 : 0
damage = max(0, damage - role_attribute.defence)
damage = max(0, damage - mount.defence)        // if mounted
if (oneshot_shields.Count > 0) damage = 0
if (role_attribute.armor >= 1)
    HurtArmor(damage)
else
    HurtHp(damage)
```

`HurtArmor` subtracts armor and overflows the negative remainder into HP;
`HurtHp` subtracts HP and calls `Dead` at 0. Native `HealthState::ApplyDamage`
implements defence subtraction, armor absorption/overflow, HP clamp, camp
filtering and death. `isBadass`, mounts, one-shot shields and buffs are not
implemented in this slice.

### Enemy (`RGEController.GetHurt`, `0x1108E60`)

Recovered core: subtract HP, clamp at 0, call `Dead`. No armor field exists on
the enemy `RoleAttribute`; `e_slime01` has hp 12 and no defence. Native
`HealthState::ApplyEnemyDamage` implements the same HP path (defence is 0 for
this fixture). Damage callbacks, source filters and multiplayer messages are
not implemented.

### Critical

The player path rolls `UnityEngine.Random.Range(0,100)` against
`DamageTrigger.critical` and forwards the result to effect triggers, but the
observed HP-damage argument is `GetFinalDamage()` and is not crit-multiplied.
`weapon_000` has `critic = 0`. Native does not implement a critical multiplier.

### Addition damage

`DamageCarrier.UpdateInfo` adds `MapManager.GetAdditionDamage(camp)`. The
modifier is not reconstructed; native uses 0 for the development room.

---

## 6. Enemy fixture (`e_slime01`)

Serialized components and values:

```text
RoleAttribute: hp/max_hp 12, speed 3.0, speed_rate 0.0, camp 0
BoxCollider2D: size (1,1), offset (0,0.5), trigger
EnemyAI04: damage 3, later_time 1.5
enemyTag 66, destory_on_dead 0
```

`EnemyAI04.OnTriggerEnter2D` (`0x10E54D0`) directly tail-calls
`RGEController.HitUnit(other, 3, 1.5f)`, confirming contact damage 3 on a 1.5 s
cadence. Native applies contact damage while the player overlaps the recovered
collider and is alive, with the same cooldown.

Fixtures are explicit scene metadata entries at `(18, 15.5)` and `(22, 15.5)`.
No AI, spawning, pathfinding or room-clear logic is implemented.

### Death behavior

`RGEController.Dead` (`0x110DE18`):

1. Returns if already dead.
2. Sets `dead`.
3. Disables the collider immediately.
4. Calls virtual dead hooks and removes delegates.
5. Plays the death animator state / hit material path.
6. For `e_slime01`, `EnemyAI04.ChildDead` schedules `DeadEvent` after
   `later_time = 1.5 s`; `DeadEvent` spawns the `slime_dead` object.
7. The controller destroy path uses `dead_destory_delay_time`, initialized to
   **4.0 s** in `RGEController..ctor` (`0x110A960`). The float written to
   `friction` (`+0x90`) during `GetHurt` is 0.95 — it is **not** a death delay.

Native uses the recovered dead sprite (`enemy01_6`), keeps the enemy rendered for
4.0 s, disables target/collision checks immediately, and then removes it. It does
not spawn `slime_dead` or implement room-clear logic.

**CORRECTION:** earlier notes described a `~0.95` enemy death delay. Disassembly
shows 0.95 is a friction write; the constructor's destruction delay is 4.0 s.
The exact runtime destruction predicate (`destory_on_dead` / enemy-tag flag) is
PARTIAL, but e_slime01's serialized tag path is consistent with the 4.0 s value.

---

## 7. Player death and reset

`RGController.Dead` (`0x1179ECC`) sets `dead`, sets the local game-fail flag,
makes the Rigidbody kinematic, calls `RGHand.DeadWeapon`, disables the collider,
triggers the Animator death state, and enters the reference game-over/reborn
flow. The native slice:

- stops movement and attack immediately when HP reaches 0,
- sets `HealthState.dead` and `player_dead_`,
- renders the recovered final dead sprite (`c01_8` for c00),
- leaves collision movement disabled (no normal locomotion),
- ignores further contact damage because `Alive()` is false,
- resets on `R` to the recovered spawn `(20,15)`, full hp/armor, enemies, and open
  doors.

The full death animation curve (scale/color tween) and production game-over /
reborn UI are not implemented. The static dead sprite is the recovered final
frame.

---

## 8. Confidence summary

| Claim | Confidence | Evidence |
|---|---|---|
| Attack is a held button path through `BtnAtkClick`/`SetAttack` | CONFIRMED | `0xDD87FC`, `0x117C3DC`, `0xF89904` |
| Native keyboard/LMB mapping | NATIVE TEST BINDING | no reference keyboard parity |
| `w_pistol` period 1/3 s, Attack event 1/30 s | CONFIRMED | serialized AnimatorController/AnimationClip |
| Animator speed factors are 1.0 for weapon_000/c00 | HIGH CONFIDENCE | `0xF88BC0`, `0xF919CC`, `0xD41564` |
| Effective projectile speed 36 | CONFIRMED | `BulletInfo.speed`, `RGBullet.UpdateInfo` |
| Spread = attribute.deviation * (weapon.deviation+1) | CONFIRMED | `0x12413A4`..`0x12413D4` |
| c00 spread is 0° | HIGH CONFIDENCE | c00 has no serialized deviation; ctor leaves 0 |
| Bullet lifetime 5 s | CONFIRMED | `destroy_time` + `RGBullet.DestroySelf` |
| Bullet wall/obstacle blocking | HIGH CONFIDENCE | tag/collision branch; native swept solid test |
| Player damage armor/HP/defence | HIGH CONFIDENCE | `0x11795CC`, `0x1179D44`, `0x1179E38` |
| Enemy damage HP/death | HIGH CONFIDENCE | `0x1108E60`, `0x110DE18` |
| Enemy collider disabled on death | CONFIRMED | `0x110DE18` collider `set_enabled(false)` |
| Enemy destruction delay 4.0 s | HIGH CONFIDENCE | `RGEController..ctor` `+0x9C = 4.0f`; 0.95 is friction |
| Player full game-over/reborn flow | UNKNOWN/UNIMPLEMENTED | out of scope |
| Exact Rigidbody2D projectile integration/rebound | UNKNOWN | native swept linear approximation |
| Critical/element/effect triggers | PARTIAL/UNIMPLEMENTED | effect-trigger bodies not decoded |
