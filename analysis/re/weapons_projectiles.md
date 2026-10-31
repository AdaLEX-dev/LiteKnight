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

## 6. Fire/attack behavior status

`Gun001` is a concrete weapon script (`0x?` not yet disassembled in this pass), and
`RGWeapon`/`RGHand`/`RGBaseHand` own firing, ammo, and hand state. The following are
**not yet reconstructed**:

- attack input → `RGController.BtnAtkClick` → hand/weapon fire call chain,
- fire cooldown from `weapon_speed`/`atk_speed`,
- spread application from `deviation` and bullet count,
- ammo/consume behavior,
- exact `BulletInfo` → `Rigidbody2D.velocity` math,
- rebound/through implementation (`can_rebound`, `throughCount`),
- weapon pickup/switch state machine.

These are Phase 3 blockers; the vertical slice must use a minimal, explicitly
labelled implementation of this confirmed pair and cannot claim full weapon parity.

## 7. Confidence

| Claim | Confidence | Evidence |
|---|---|---|
| `weapon_000` is a real `Gun001` weapon prefab | CONFIRMED | serialized prefab + manifest path |
| `bullet_0` is a real `Bullet01` projectile | CONFIRMED | serialized prefab + manifest path |
| Weapon damage/speed/deviation/bullet proto | CONFIRMED | serialized `Gun001` fields |
| `Bullet01` destroy_time 5 / rotate_angle behavior | CONFIRMED | serialized field + FixedUpdate disassembly |
| `RGBullet.UpdateInfo` field copies | HIGH CONFIDENCE | `0x1168000` disassembly |
| Effective projectile velocity formula | UNRESOLVED | `SetBulletVelocity`/motion path not fully decoded |
| Attack → fire path/cooldown/ammo | UNRESOLVED | needs `Gun001`/`RGHand` trace |
| Display name / Knight starting weapon | CANDIDATE/UNRESOLVED | no serialized item-name mapping |
