# Weapon and Projectile Reconstruction

**Scope:** one confirmed weapon/projectile pair for the first native slice. This is
not a full weapon catalogue reconstruction.

---

## 1. Selected technical pair

The first slice can use the serialized assets below without claiming a display name:

| Asset | Bundle/path | Script/fields |
|---|---|---|
| `weapon_000` | `common`, `Assets/RGPrefab/Weapon/weapon_000.prefab` | `Gun001`, `WeaponMaterial`, `WeaponFusion`, `WModDeviation`, `WeaponItemConfig` |
| `bullet_0` | `common`, `Assets/RGPrefab/Bullet/bullet_0.prefab` | `Bullet01` |

`weapon_000` is a **CONFIRMED weapon prefab**. It is **not** proven to be the
Knight's starting weapon or the display name "Bad Pistol". Do not label it as such
without a serialized item-name mapping.

## 2. Serialized weapon fields (`weapon_000`)

`Gun001`:

```text
activate = 1
atk_move_speed = -0.1
bulletsInfo = [BulletInfoInspector{
    bulletProto = bullet_0 (path ID 3205921451686524956),
    speed = 36.0,
    size = 1.0,
    damage = 3,
    critic = 0,
    repel = 2,
    throughCount = 0
}]
consume = 0
deviation = 5
has_effect = 0
is_melee = 0
item_value = 4
need_lock = 1
weapon_speed = 1.0
weapon_tags = [305]
weapon_type = 14
```

`WeaponItemConfig` support items are serialized PPtrs; their identities are not
resolved.

For comparison, `weapon_000x` has `GunMasterKnight`, damage 4, speed 36, deviation
7, `multiCount=5`, item_level 1. This confirms a related knight-flavoured variant,
but not the starting-weapon identity.

## 3. Projectile prefab (`bullet_0`)

`Bullet01` fields:

```text
awake = 1
camp = 1
can_rebound = 1
critical = 0
damageType = 0
destroy_time = 5.0
repel = 0
rotate_angle = 0
speed = 10.0
```

`bullet_0` components: `Transform`, `Rigidbody2D` (dynamic, gravity scale 0,
freeze rotation, mass 1), `Bullet01`.

`RGBullet` fields (`dump.cs` TypeDefIndex 6599):

```text
float speed;             // +0x7C
Rigidbody2D rigid2d;     // +0x80
float destroy_time;      // +0x88
int rotate_angle;        // +0x8C
bool awake;              // +0x90
bool can_rebound;        // +0x91
GameObject source_objcet;// +0x98
Vector2 initial_velocity;// +0xA0
int damage;              // +0xA8
bool can_through;        // +0xAC
int repel;               // +0xB0
int critical;            // +0xB4
Transform _b;            // +0xB8
bool dont_destroy;       // +0xC0
bool manual_destroy;     // +0xC1
bool _isHitOn;           // +0xC2
```

`BulletInfo` (`dump.cs` TypeDefIndex 6558):

```text
GameObject bulletProto;  // +0x0
GameObject sourceObject; // +0x8
RGWeapon sourceWeapon;   // +0x10
Vector2 createPosition;  // +0x18
Vector2 initVelocity;    // +0x20
float speed;             // +0x28
float size;              // +0x2C
float directionAngle;    // +0x30
int reflectCount;        // +0x34
float duration;          // +0x38
int camp;                // +0x3C
```

`DamageInfo` (`dump.cs` TypeDefIndex 6559):

```text
emDamageType damageType; // +0x0
int damage;              // +0x4
int throughCount;        // +0x8
int critic;              // +0xC
int repel;               // +0x10
float damageInterval;    // +0x14
int camp;                // +0x18
```

## 4. Creation and update path

`RGBullet.UpdateInfo(BulletInfo, DamageInfo)` at `0x1168000` copies data from the
structs into the bullet:

- `BulletInfo.speed` (`+0x28`) → `RGBullet.speed` (`+0x7C`)
- `BulletInfo.initVelocity` (`+0x20`) → `RGBullet.initial_velocity` (`+0xA0`)
- `DamageInfo.damage` (`+0x4`) → `RGBullet.damage` (`+0xA8`)
- `DamageInfo.repel` (`+0x10`) → `RGBullet.repel` (`+0xB0`)
- `DamageInfo.critic` (`+0xC`) → `RGBullet.critical` (`+0xB4`)
- `DamageInfo.throughCount` (`+0x8`) → through state
- `DamageInfo.damageType` → through/damage type handling

`Bullet01.Start` (`0x14C10E8`) resolves the bullet's `RGBulletTrigger` component,
sets renderer sorting, and tail-calls the base `Start` (slot `+0x218`).

`Bullet01.FixedUpdate` (`0x14C1230`):

- checks `awake` (`+0x90`) and `rotate_angle` (`+0x8C`),
- reads `Transform.eulerAngles`,
- adds `rotate_angle` to Z each fixed tick,
- writes back `Transform.eulerAngles`.

The base `RGBullet` movement/velocity path is not fully decoded. `Bullet01`'s
serialized speed is 10, while the weapon's `BulletInfo.speed` is 36; `UpdateInfo`
copies 36 into `RGBullet.speed`, so the effective initial velocity must be resolved
from `RGBullet.SetBulletVelocity`/`UpdateInfoWithMotion` before claiming exact
trajectory parity.

`RGBullet.DestroySelf(float)` (`0x11689FC`) starts the `Destroying` coroutine
(`0x1169310`); `DestroySelf()` (`0x1169308`) uses `destroy_time` (`+0x88`).
`destroy_time` = 5.0 on `bullet_0`.

## 5. Collision/damage path

The collision side was resolved in `analysis/re/damage.md`:

```text
Bullet collider trigger
    ↓
RGBulletTrigger.OnTriggerEnter2D (0x1160324)
    ↓
box branch: GameObject.GetComponent<RGBox> (0xFF8A84, GOT 0x31003B0)
    ↓
RGBox.Hit (vtable +0x168 / 0x11675F8)          [HIGH CONFIDENCE]
    OR
player branch: GetComponent<RGController> (0x30E8F80)
    ↓
RGController.GetHurt (vtable +0x228 / 0x11795CC)
    OR
enemy branch: GetComponent<RGEController> (0x309A4E8)
    ↓
RGEController.GetHurt (vtable +0x2C8 / 0x1108E60)
```

The vtable offsets are confirmed by the actual `Il2CppClass` layout:
vtable base `0x128`, 16-byte `VirtualInvokeData`; slot 16 → `0x228` (player
`GetHurt`), slot 26 → `0x2C8` (enemy `GetHurt`).

## 6. Fire path reconstruction (new evidence)

### 6.1 Input → weapon

`RGController.RoleAtk` (`0x117C588`) calls `RoleAttackEvent`, finds the current
`RGHand` (`+0x68`), and invokes the hand/weapon attack path. `RGController.BtnAtkClick`
(`0x117C3DC`) selects the hand/default attack and calls the hand path. The exact
branch for every special weapon is not needed for `weapon_000`; the minimal path is:

```text
BtnAtkClick / RoleAtk
  → RGHand/current weapon attack
  → Gun001.Attack
```

### 6.2 `Gun001.Attack` (`0x1241260`)

Observed sequence:

1. Calls the weapon virtual at vtable `+0x308`/`+0x310` (pre-attack/state hook).
2. Calls `RGWeapon.get_bullet_speed` (`0xF879FC`), which returns
   `bulletInfo.speed` (`BulletInfoInspector +0x18`).
3. Reads controller `+0x38` as the base angle.
4. Calls `Gun001.CreateBullet(speed, angle)` (`0x124131C`).
5. Calls `RGWeapon.AfterBulletCreateEvent`, a virtual at `+0x3C8`, and
   `RGWeapon.AfterAttackEvent`.
6. Plays the weapon SFX via `RGMusicManager.PlayEffect`.

For `weapon_000`, `RGWeapon.get_bullet_speed` returns **36.0** (the serialized
`Gun001.bulletsInfo[0].speed`), not the `Bullet01.speed` default 10.0.

### 6.3 `Gun001.CreateBullet` (`0x124131C`)

Observed:

- `deviation = RGWeapon.deviation` (`+0x48`), value 5 for `weapon_000`.
- Reads `RoleAttribute.deviation` at `+0x2c` (obtained through the controller
  virtual at `+0x1a8`); computes
  `spread = RoleAttribute.deviation * weapon.deviation + RoleAttribute.deviation`,
  i.e. `attribute.deviation * (weapon.deviation + 1)`.
- Calls `RGRandom.Range(-spread, +spread)` (`0xDE93DC`) for the actual angle
  offset. c00 does not serialize `deviation`; `RoleAttribute..ctor` leaves it at
  0, so weapon_000's effective spread is 0° for c00. The serialized
  `deviation = 5` is a multiplier on the player deviation, not a standalone ±5°.
- Calls a weapon virtual at `+0x558` (likely `GetBulletInfo`/decorated info) and
  `BulletInfoInspector.GetBulletInfo` (`0x14BFF30`), which copies
  `BulletInfoInspector.speed` into `BulletInfo.speed` (`+0x28`).
- Calls `BulletFactory.TakeBullet(BulletInfo, DamageInfo, invokeOnTake=true, ...)`
  (`0x14BEB40`) to obtain the projectile instance.
- Calls `RGWeapon.BulletCreateEvent` (`0xF9058C`) and, if `has_trail`, clears the
  trail renderer.

The `+0x2c` read is `RoleAttribute.deviation` (via `get_attribute`), not
`fixed_angle`. The base angle is `RGBaseController.fixed_angle` (`+0x38`), set by
`RGController.AutoLock`. For weapon_000/c00 the serialized deviation is 5 but the
effective spread is 0° because the player deviation is 0.

### 6.4 Speed resolution

`RGBullet.UpdateInfo` (`0x1168000`) copies `BulletInfo.speed` (`+0x28`, i.e.
`RGBullet +0x48` because `BulletInfo` starts at `RGBullet +0x20`) into
`RGBullet.speed` (`+0x7C`). Therefore:

- `bullet_0` serialized `Bullet01.speed = 10.0` is a prefab/default value,
- `weapon_000` `BulletInfo.speed = 36.0` is the effective initial speed used by
  the weapon fire path.
- `BulletInfo.initVelocity` and `directionAngle` are copied into the bullet's
  initial velocity/angle; exact vector composition remains PARTIAL.

### 6.5 Ammo/consume

`weapon_000` has `Gun001.consume = 0`; there is no ammo cost for this weapon.
Other weapons may consume energy/ammo; that is not needed for the first slice.
`RGWeapon.SetAttack` checks the hand/controller state and consume count, but the
exact energy-warning branch is PARTIAL.

### 6.6 Cadence/cooldown — RECOVERED

`weapon_000` uses AnimatorController `weapon_pistol`:

```text
default state: w_ide (duration 0)
attack state:  w_pistol, loop, duration 0.333333343 s
AnimationEvent: Attack at 0.033333335 s
```

`RGWeapon.Start` (`0xF88BC0`) sets:

```text
Animator.speed = weapon_speed
               * controllerAtkSpeed
               * _speedFactor
               * _buffAtkSpeedFactor
```

`weapon_000.weapon_speed = 1.0`; `RGWeapon..ctor` (`0xF919CC`) initializes both
speed factors to 1.0; `RoleAttributePlayer..ctor` (`0xD41564`) initializes
`atk_speed` to 1.0. Therefore the recovered firing cadence is:

```text
first shot:  1/30 s after entering w_pistol
repeat:      one shot every 1/3 s while attack is held
```

`atk_move_speed = -0.1` is a movement modifier: `AttackStart` adds it to
`RoleAttributePlayer.speed_rate` (`+0x20`) and `AttackStop` subtracts it. It is
not recoil or projectile spread.

### 6.7 Rebound/through/lifetime

- `bullet_0` has `can_rebound = 1`, `destroy_time = 5.0`, `rotate_angle = 0`,
  `Camp = 1`.
- `RGBullet.DestroySelf` starts the `Destroying` coroutine; `DestroySelf()`
  (`0x1169308`) uses `destroy_time`.
- `throughCount`/`reflectCount` are copied from `DamageInfo`/`BulletInfo`; their
  exact collision decrement behavior is PARTIAL.
- `ReboundEffectTrigger.TriggerWith` exists and can alter the bullet, but its exact
  reflection math is not decoded.

### 6.8 Remaining weapon/projectile blockers

- exact `RGBullet` initial velocity composition and Rigidbody2D integration,
- rebound/through/reflection effect-trigger bodies,
- full `Gun001` ammo/consume and special-weapon branches,
- exact Unity input button name and right-stick/aim routing in `RGJoyControl`,
- exact runtime RGRandom seed for weapon spread.

`weapon_000`/`bullet_0` is still the best confirmed first-slice pair. The native
combat prototype uses the recovered 1/3 s cycle and 1/30 s first-shot event with
a deterministic swept-collision approximation.

## 7. Confidence

| Claim | Confidence | Evidence |
|---|---|---|
| `weapon_000` is a real `Gun001` weapon prefab | CONFIRMED | serialized prefab + manifest path |
| `bullet_0` is a real `Bullet01` projectile | CONFIRMED | serialized prefab + manifest path |
| Weapon damage/speed/deviation/bullet proto | CONFIRMED | serialized `Gun001` fields |
| `Bullet01` destroy_time 5 / rotate_angle behavior | CONFIRMED | serialized field + FixedUpdate disassembly |
| `RGBullet.UpdateInfo` field copies | HIGH CONFIDENCE | `0x1168000` disassembly |
| Effective projectile speed | HIGH CONFIDENCE | `RGBullet.UpdateInfo` copies BulletInfo speed 36 into `+0x7C`; prefab 10 is default |
| Attack → fire path | HIGH CONFIDENCE | `Gun001.Attack` → `CreateBullet` → `BulletFactory.TakeBullet` |
| Fire cooldown | CONFIRMED | `weapon_pistol` Animator `w_pistol` 1/3 s loop + `Attack` event at 1/30 s; factors 1.0 |
| Ammo/consume | CONFIRMED for weapon_000 | serialized `consume=0` |
| Spread formula | CONFIRMED | `attribute.deviation * (weapon.deviation+1)`; c00 attribute deviation 0 |
| Rigidbody2D/rebound parity | UNKNOWN | native swept linear movement + static-solid blocking |
| Display name / Knight starting weapon | CANDIDATE/UNRESOLVED | no serialized item-name mapping |
