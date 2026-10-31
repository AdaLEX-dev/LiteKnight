# Damage Path — Evidence-Backed Reconstruction

**Status:** player/enemy/box damage entry points and the projectile target bridge are reconstructed (CONFIRMED); effect-trigger/critical/element details and full death/reborn branches remain PARTIAL.
**Date:** 2026-10-31
**Reference:** Soul Knight 2.6.8, Unity **2018.4.12f1**, IL2CPP metadata **v24**
**Binary:** `analysis/apktool_out/lib/arm64-v8a/libil2cpp.so`

> **Evidence rule:** field names are structural; the formulas below come from disassembly. Unresolved pieces are labelled, not filled in with guesses.

---

## 1. Question

Reconstruct:

- `DamageCarrier`, `BulletInfo`, `DamageInfo`
- `RoleAttribute`, `RoleAttributePlayer`
- `RGController` damage entry points
- armor, health, energy
- damage modifiers
- death conditions

---

## 2. Structural layout (metadata)

### `DamageCarrier` — TypeDefIndex 6557

| Field | Offset |
|---|---:|
| `damageType` (`emDamageType`) | `+0x18` |
| `camp` | `+0x1C` |
| `bulletInfo` (`BulletInfo`) | `+0x20` |
| `damageInfo` (`DamageInfo`) | `+0x60` |

Methods:

| Method | RVA |
|---|---:|
| `UpdateInfo(BulletInfo, DamageInfo)` | `0xDC8614` |
| `UpdateInfoWithMotion(BulletInfo, DamageInfo)` | `0xDC86B4` |
| `GetBullet()` | `0xDC86E8` |
| `HasDamageType(emDamageType)` | `0xDC87BC` |
| `IsAlliance(int)` | `0xDC87F0` |
| `IsEnemy(int)` | `0xDC8810` |

### `BulletInfo`

Fields: `bulletProto`, `sourceObject`, `sourceWeapon`, `createPosition`, `initVelocity`, `speed`, `size`, `directionAngle`, `reflectCount`, `duration`, `camp`.

### `DamageInfo`

Fields: `damageType`, `damage` (`+0x4`), `throughCount`, `critic` (`+0xC`), `repel` (`+0x10`), `damageInterval`, `camp` (`+0x18`).

### `RoleAttribute` — TypeDefIndex 6910

| Field | Offset |
|---|---:|
| `camp` | `+0x18` |
| `speed` | `+0x1C` |
| `speed_rate` | `+0x20` |
| `max_hp` | `+0x24` |
| `hp` | `+0x28` |
| `deviation` | `+0x2C` |
| `critical` | `+0x30` |
| `OnHpChanged` | `+0x48` |

### `RoleAttributePlayer` — TypeDefIndex 6912

| Field | Offset |
|---|---:|
| `max_armor` | `+0x50` |
| `armor` | `+0x54` |
| `max_energy` | `+0x58` |
| `_energy` | `+0x5C` |
| `atk` | `+0x60` |
| `in_skill_time` | `+0x64` |
| `c_index` | `+0x68` |
| `c_level` | `+0x6C` |
| `default_buff` | `+0x70` |
| `atk_speed` | `+0x74` |
| `defence` | `+0x78` |
| `this_skill_time` | `+0x7C` |
| `armor_load` | `+0x80` |
| `armor_rate` | `+0x84` |
| `armor_time` | `+0x88` |
| `energy_time` | `+0x8C` |
| `OnSkillReady` | `+0x90` |
| `lowEnergy` | `+0x98` |

### `RGController` relevant offsets

| Field | Offset |
|---|---:|
| `awake` | `+0x18` |
| `role_attribute` (`RoleAttributePlayer`) | `+0x70` |
| `mount` (`RGMountController`) | `+0x78` |
| `clip_hit` | `+0x88` |
| `can_hurt` | `+0xB0` |
| `oneshot_shields` (`List<BuffShield>`) | `+0x128` |

### `RGEController` relevant offsets

| Field | Offset |
|---|---:|
| `awake` | `+0x30` |
| `dead` | `+0x78` |
| `role_attribute` (`RoleAttribute`) | `+0xD0` |
| `onEnemyDamageProcess` (`Func<int,bool,int>`) | `+0x168` |
| `invincible_mode` | `+0x131` |
| `netId` | `+0x12C` |

---

## 3. `DamageCarrier.UpdateInfo` — `0xDC8614` (CONFIRMED)

Reconstructed pseudocode:

```csharp
void UpdateInfo(BulletInfo bulletInfo, DamageInfo damageInfo)
{
    int addition = MapManager.Instance.GetAdditionDamage(damageInfo.camp);
    damageInfo.damage += addition;

    this.bulletInfo = bulletInfo;
    this.damageInfo = damageInfo;
    this.camp = damageInfo.camp;
}
```

Evidence:

- `ldr w24, [x19, #0x4]` — original `DamageInfo.damage`.
- `MapManager.get_Instance` then virtual call at vtable `+0x208` with `w1 = damageInfo.camp` (`+0x18`).
- `add w8, w0, w24; str w8, [x19, #0x4]` — adds the returned modifier to damage.
- Copies `BulletInfo` (0x40 bytes) and `DamageInfo` (0x1C bytes) into the carrier.
- `str w8, [x20, #0x1c]` — stores `camp`.

**Confidence:** CONFIRMED. `MapManager.GetAdditionDamage` itself is not yet reconstructed.

---

## 4. Player damage path

### 4.1 `RGController.GetHurt(int damage, GameObject source)` — `0x11795CC`

Entry checks:

- returns if `!IsLocalPlayer()` (non-local path at `0x1179890` only plays a hit effect),
- returns if `!awake` (`+0x18`) or `!can_hurt` (`+0xB0`).

Reconstructed core:

```csharp
// after local/awake/can_hurt checks
int dmg = damage;

if (BattleData.isBadass)
    dmg += 1;

int defence = role_attribute.defence;   // RoleAttributePlayer +0x78
dmg = Mathf.Max(0, dmg - defence);

if (mount != null)
    dmg = Mathf.Max(0, dmg - mount.defence); // RGMountController +0x28

// one-shot shields absorb the hit
if (oneshot_shields.Count > 0)
    dmg = 0;

if (role_attribute.armor >= 1)
    HurtArmor(dmg, source);
else
    HurtHp(dmg, source);
```

Evidence:

- `BattleData.get_isBadass`, `and w24,w0,#1`, `add w21,w24,w21`.
- `role_attribute` at `+0x70`; `ldr w22,[x22,#0x78]` is `RoleAttributePlayer.defence`.
- `Mathf.Max(0, dmg - defence)` at `0x1B9C314`.
- mount field at `+0x78`; `ldr w22,[mount,#0x28]` is `RGMountController.defence`; second `Mathf.Max`.
- `oneshot_shields` at `+0x128`; `get_Count > 0` → `csel w21, wzr, w21, gt` (damage = 0).
- `role_attribute.armor` at `+0x54`; `cmp >= 1` selects vtable `+0x248` (`HurtArmor`) vs `+0x258` (`HurtHp`).

**Battle factor branch (partly decoded):** `CompareFactor(8)` is `emBattleFactor.HardShield` (metadata enum). When it is active and `role_attribute.armor >= 1`, the code at `0x1179718`..`0x11797a4` sets `dmg = 1` when `mount == null` or `mount.hasHp` (`+0xAC`) is false; if a mount exists with `hasHp == true`, the normal damage path continues. The exact design intent of this branch is not proven, but the predicate and constant are static evidence.

**Confidence:** HIGH CONFIDENCE for subtraction/selection and the HardShield predicate; intent of HardShield remains a hypothesis.

### 4.2 `RGController.HurtArmor(int damage, GameObject source)` — `0x1179D44`

Reconstructed:

```csharp
int armorAfter = role_attribute.armor - damage;
role_attribute.RestoreArmor(-damage, false);

if (armorAfter < 0)
{
    if (!BattleData.HasBuff(6))
        HurtHp(-armorAfter, source); // overflow damage
    else
        PlayHitEffect();
}
else
{
    PlayHitEffect();
}
```

Evidence:

- `ldr w8, [role_attribute, #0x54]` (armor), `sub w23, w8, damage`.
- `neg w1, damage; RoleAttributePlayer.RestoreArmor(-damage,false)` — `0xD3FFD8`.
- `tbz w23,#31` tests `armorAfter < 0`.
- `BattleData.HasBuff(6)` (`0x124DAB8`); if false, calls vtable `+0x258` with `w1 = -w23` (leftover damage) and source.
- If `emBuff.ShieldBlue` (index 6) is present, plays the hit effect instead of overflowing.

**Confidence:** CONFIRMED for subtraction/overflow; index 6 is `emBuff.ShieldBlue` from metadata.

### 4.3 `RGController.HurtHp(int damage, GameObject source)` — `0x1179E38`

Reconstructed:

```csharp
role_attribute.RestoreHealth(-damage, false);

if (role_attribute.hp > 0)
    PlayHitEffect();
else
    Dead(source);
```

Evidence:

- `neg w1, damage`; virtual call through vtable `+0x188` (RoleAttributePlayer.RestoreHealth).
- `ldr w8, [role_attribute, #0x28]` (hp); `cmp > 0` → hit effect; otherwise branch to `RGController.Dead`.

**Confidence:** CONFIRMED.

### 4.4 `RoleAttribute.RestoreHealth(int value, bool sync)` — `0xD3DD2C`

Reconstructed:

```csharp
hp += value;

if (value >= 0)
{
    ShowTextHp(value);
    if (hp > max_hp) hp = max_hp;
}
else
{
    if (hp <= 0) hp = 0;
}

OnHpChanged?.Invoke(hp, max_hp);
```

Evidence:

- `ldr w8,[+0x28]; add w8,w8,w1; str w8,[+0x28]`.
- if `value` sign bit clear: `ShowTextHp`; `ldp w8,w9,[+0x24]` (max_hp, hp); clamp.
- if negative: `cmp w8,#0; b.gt`; else `str wzr,[+0x28]`.
- `OnHpChanged` at `+0x48` invoked with `hp`, `max_hp`.

**Confidence:** CONFIRMED.

### 4.5 `RoleAttributePlayer.RestoreHealth(int value, bool sync)` — `0xD40418`

Reconstructed:

```csharp
if (!is_local_player && !sync) return;

RGController controller = GetComponent<RGController>();
if (controller.dead) return;

hp += value;

if (value >= 1)
{
    ShowTextHp(value);
    if (hp > max_hp) hp = max_hp;
}
else if (hp <= 0)
{
    if (CanDie())
        hp = 0;
    else
        hp = 1; // survive at 1 HP
}

if (is_local_player && !sync)
    UICanvas.UpdateHpBar(...);
```

Evidence:

- `RoleAttributePlayer.get_is_local_player`; if false and `sync` false → return.
- `GetComponent<RGController>`; `ldrb w8,[+0x19]` dead flag; return if set.
- `hp += value` at `+0x28`.
- positive path clamps to `max_hp` (`+0x24`).
- negative path: `CanDie()`; if true `hp = 0`, else `hp = 1`.
- local-only UI update.

**Confidence:** CONFIRMED.

### 4.6 `RoleAttributePlayer.CanDie()` — `0xD40718`

Exact static behaviour:

```text
RGGameProcess process = Singleton<RGGameProcess>.get_Inst();
if (process.this_index == -1) return false;
if (process.this_index == -3) return false;
if (BattleData.IsSandboxEditing) return false;
return true;
```

The generic singleton is resolved through GOT slot `0x030F2358` → metadata method
slot `0x3276848`, which `script.json` identifies exactly as:

```text
Method$Singleton<RGGameProcess>.get_Inst()
```

The field at `+0x1C` is `RGGameProcess.this_index` (`dump.cs` TypeDefIndex 7344).
The earlier `RGGameInfo.map_random_seed` interpretation was wrong; do not reintroduce
it. The `-1`/`-3` values are sentinel map indexes, not seed values. Their high-level
meaning (which scenes/modes they represent) is still a HYPOTHESIS, but the type and
field identity are CONFIRMED.

**Confidence:** CONFIRMED for the singleton type, field, constants, and sandbox
check; HYPOTHESIS for the high-level meaning of sentinel indexes -1/-3.

### 4.7 `RGController.Dead(GameObject source)` — `0x1179ECC`

Confirmed effects:

- returns if already `dead` (`+0x19`),
- sets local player game-fail flag in `BattleData`,
- cancels invokes,
- sets alive/dead flags at `+0x18`/`+0x19`,
- sets rigidbody kinematic,
- disables the current weapon (`RGHand.DeadWeapon`),
- disables a collider (`GetComponent<Collider2D>` → `Behaviour.enabled = false`),
- triggers the death animation (`Animator.SetTrigger`),
- handles local-player game-over/reborn flow and UI.

Full death/reborn branching is not yet reconstructed.

---

## 5. Enemy damage path

### 5.1 `RGEController.GetHurt(int damage, GameObject source)` — `0x1108E60`

Entry checks:

- returns if `!awake` (`+0x30`) or `dead` (`+0x78`),
- filters source/carrier and alliance; exact predicate is partly decoded.

Core damage:

```csharp
int dmg = damage;
if (BattleData.CompareFactor(6))
    dmg -= 1;                       // confirmed subtraction
dmg = Mathf.Max(0, dmg);
dmg = onEnemyDamageProcess != null ? onEnemyDamageProcess(dmg, true) : dmg;

int hp = role_attribute.hp;         // +0x28
role_attribute.hp = hp - dmg;

if (role_attribute.hp <= 0) {
    role_attribute.hp = 0;
    // metadata names +0x90 "friction"; code writes 0.95f (0x3F733333)
    // before the virtual Dead call. Do not label it "death delay" without
    // further evidence.
    Dead(source, false);
}

// local/UI: show damage text, set hit material, etc.
// multiplayer: RGGameInfo.SendHurtEnemyMessage(netId, dmg)
```

Evidence:

- `BattleData.CompareFactor(6)` (`0x124C948`), `and w10,w0,#1`, `sub w21,w21,w10`.
- `Mathf.Max` at `0x1B9C314`.
- `OnEnemyDamageProcess` virtual/callback `0x110D778`; it invokes `Func<int,bool,int>` at `+0x168`.
- `role_attribute` at `+0xD0`; `hp` at `+0x28`; subtraction and clamp confirmed.
- a float at `+0x90` (metadata: `friction`) is written with `0x3F733333` (≈0.95) on death; the semantic and whether another delay field is involved are unresolved.
- virtual `Dead` through `+0x318/+0x320`.
- `RGGameInfo.SendHurtEnemyMessage` tail call at `0x11092BC`.

**Confidence:** HIGH CONFIDENCE for HP subtraction, clamp, callback, and death trigger; PARTIAL for source filtering/alliance/buff branches.

### 5.2 `RGEController.SyncGetHurt(int damage)` — `0x1109494`

Not yet decoded beyond its existence and callers. It is likely the network-sync equivalent of `GetHurt`; treat as UNKNOWN.

---

## 6. Projectile / trigger pipeline

### 6.1 `RGBulletTrigger.GetFinalDamage` — `0x116A680`

Reconstructed:

```csharp
if (!makeDamage) return 0;

float factor = 1.0f;
foreach (RGBulletEffectTrigger effect in effectTriggers)
    factor *= effect.damageFactor; // +0x18

return (int)(damage * factor);      // fcvtzs truncation
```

Evidence:

- `ldrb w8,[+0x75]` (`makeDamage`); false → return 0.
- `List<RGBulletEffectTrigger>` at `+0x78`; enumerate; `ldr s0,[effect,#0x18]` (`damageFactor`); `fmul s8,s8,s0`.
- `ldr s0,[+0x18]` (`damage`); `scvtf`, `fmul`, `fcvtzs` → int.

**Confidence:** CONFIRMED.

### 6.2 `RGBulletTrigger.GetDamageFactor` — `0x116A7D0`

```csharp
if (has_ice_buff && BattleData.HasBuff(23))
    return 0.5f;
return 1.0f;
```

**Confidence:** CONFIRMED for the branch and constant; index 23 is `emBuff.ShieldIce` from metadata. The `has_ice_buff` condition at `+0x74` is confirmed; its gameplay meaning remains to be verified.

### 6.3 `RGBulletTrigger.OnTriggerEnter2D` — `0x1160324`

Confirmed outline:

- `finalDamage = GetFinalDamage()`.
- `GameObject.CompareTag` is used to detect a special collider class (the tag string was not resolved at this address); in that branch damage is set to 0 and the trigger's virtual `OnHit` is called.
- otherwise iterate `effectTriggers` and invoke each effect trigger's virtual `OnHit` with the final damage and through flag.
- `RGBulletTrigger.OnHit` itself (`0x11621B4`) is `ret` in the base class; concrete effect triggers override the behaviour.
- `RGBulletEffectTrigger.TriggerWith` (`0x1169D84`) is also `ret` in the base class; concrete subclasses implement it.

**Resolved target bridges:** the `RGBulletTrigger.OnTriggerEnter2D` body has three
concrete target paths:

1. **Box bridge — HIGH CONFIDENCE.** `0x1160ADC` calls shared
   `GameObject.GetComponent<RGBox>` (`0xFF8A84`, GOT `0x31003B0` → method slot
   `0x3254270`, `script.json`: `Method$UnityEngine.GameObject.GetComponent<RGBox>()`).
   It then calls virtual `RGBox.Hit(damage, source_object, camp)` through vtable
   `+0x168` at `0x1160B70` (`RGBox.Hit` `0x11675F8`).
2. **Player bridge — CONFIRMED.** The player branch at `0x116078C` calls
   `GameObject.GetComponent<RGController>` (GOT `0x30E8F80` → method slot
   `0x32542B0`) and calls `RGController.GetHurt` through vtable `+0x228` at
   `0x11608A0`. The vtable base is `0x128`; slot 16 × 16 bytes = `0x228`, matching
   `RGController.GetHurt` (`0x11795CC`, `dump.cs` Slot 16).
3. **Enemy bridge — CONFIRMED.** The enemy branch at `0x1160F78` calls
   `GameObject.GetComponent<RGEController>` (GOT `0x309A4E8` → method slot
   `0x32542C8`, `script.json`: `Method$UnityEngine.GameObject.GetComponent<RGEController>()`)
   and calls `RGEController.GetHurt` through vtable `+0x2C8`. Vtable base `0x128` +
   slot 26 × 16 bytes = `0x2C8`, matching `RGEController.GetHurt` (`0x1108E60`,
   `dump.cs` Slot 26).

The same `GetComponent<RGEController>` → vtable `+0x2C8` sequence appears in
`RGLaserTrigger.OnTriggerStay2D` (`0xDDD6E0`), independently confirming the slot.
The projectile → controller bridge is therefore closed. The effect-trigger
`TriggerWith` override chain and critical/element details remain separate items.

### 6.4 `DamageCarrier` / `DamageTrigger` / `DamageTriggerHitChecker`

- `DamageTrigger` (`0xDC89DC`..) owns `damage`, `critical`, `hit_object`, `repel`, `camp`, `_carrier`, and `CastModTrigger`.
- `DamageTriggerHitChecker` (`0xDC91CC`) resolves the hit object and its damage carrier; it is a collision/router component, not the final HP mutator.
- `RGBulletEffectTrigger` has `damageFactor` and `TriggerWith`.

The complete chain from bullet collision to target HP is **PARTIAL**.

---

## 7. Confidence summary

| Claim | Confidence | Evidence |
|---|---|---|
| `DamageCarrier.UpdateInfo` adds `MapManager.GetAdditionDamage(camp)` | CONFIRMED | `0xDC8614` |
| Player `GetHurt` subtracts `defence` and mount defence, clamps ≥0 | HIGH CONFIDENCE | `0x11795CC` |
| Player `GetHurt` zeroes damage when one-shot shields exist | CONFIRMED | `0x11795CC` |
| Player chooses armor path when `armor >= 1` | CONFIRMED | `0x11795CC` |
| `HurtArmor` overflows negative armor into HP unless `emBuff.ShieldBlue` (6) | CONFIRMED | `0x1179D44` |
| `HurtHp` subtracts HP and calls `Dead` at ≤0 | CONFIRMED | `0x1179E38` |
| `RoleAttribute.RestoreHealth` clamps HP | CONFIRMED | `0xD3DD2C` |
| `RoleAttributePlayer.RestoreHealth` can survive at 1 HP via `CanDie` | CONFIRMED | `0xD40418`, `0xD40718` |
| Enemy `GetHurt` subtracts HP and calls `Dead` | HIGH CONFIDENCE | `0x1108E60` |
| `RGBulletTrigger.GetFinalDamage` multiplies effect `damageFactor` | CONFIRMED | `0x116A680` |
| Projectile → `RGBox.Hit` box bridge | HIGH CONFIDENCE | `0x1160ADC`/`0x1160B70`; `GetComponent<RGBox>` method slot `0x3254270`; `RGBox.Hit` `0x11675F8` |
| Projectile → `RGController.GetHurt` player bridge | CONFIRMED | `GetComponent<RGController>` `0x30E8F80`; vtable `+0x228` = slot 16; `0x11608A0` |
| Projectile → `RGEController.GetHurt` enemy bridge | CONFIRMED | `GetComponent<RGEController>` `0x309A4E8`; vtable `+0x2C8` = slot 26; `0x1160F78` |
| Complete collision → target HP chain | HIGH CONFIDENCE | target dispatch resolved; effect-trigger override chain/crit/element still partial |
| Critical-hit application | UNRESOLVED | `DamageInfo.critic` exists; no formula yet |
| Element/status application | UNRESOLVED | `emDamageType` exists; no application path yet |
| `CanDie` singleton/field `+0x1C` identity | CONFIRMED | `Singleton<RGGameProcess>.get_Inst()`; field `RGGameProcess.this_index` |

---

## 8. Unresolved questions / next experiments

1. **Close the effect-trigger chain and critical/element rules.**
   - Controller bridges are resolved (player `+0x228`, enemy `+0x2C8`).
   - Enumerate `RGBulletEffectTrigger` subclasses and their `TriggerWith`/`OnHit` overrides.
   - Trace where the `isCritic` bool is consumed and whether any subclass multiplies damage.
   - Trace `damageType`/buff application separately from base HP subtraction.
2. **Decode critical hits.**
   - Find where `DamageInfo.critic` is read and how it changes `damage`.
   - Likely in `RGBulletTrigger` or `Weapon.BulletCreateUtils`; not yet located.
3. **Battle factors — partially resolved.** `CompareFactor(6)` is `emBattleFactor.EnemyDefence`; `CompareFactor(8)` is `emBattleFactor.HardShield`. Their exact high-level intent still needs confirmation.
4. **Resolve the high-level meaning of `RGGameProcess.this_index` sentinels.** The type/field are confirmed (`-1`/`-3` checks); the scene/mode meaning is still a hypothesis.
5. **Enemy damage modifiers.**
   - `OnEnemyDamageProcess` callback and `RGEBossController` overrides.
6. **Elements/status.**
   - `emDamageType` and `RGBulletBuffTrigger`/`RGBuff` application.
7. **Death/reborn.**
   - Full `RGController.Dead` and `RGEController.Dead` branches, including multiplayer and reborn cards.

---

## 9. Native implementation requirements (do not implement yet)

Once the bridge and formulas are confirmed:

1. Separate `DamageInfo` (incoming packet) from `HealthState` (`hp`, `max_hp`, `armor`, `max_armor`, `energy`).
2. Implement the exact ordered pipeline:
   - `MapManager` addition modifier,
   - badass/defence/mount reduction,
   - one-shot shield absorption,
   - armor overflow,
   - HP subtraction,
   - `CanDie` survival rule,
   - death.
3. Preserve integer operations and the order of clamps.
4. Keep damage numbers and UI events separate from state mutation.
5. Add tests for:
   - armor absorbs and overflows,
   - armor exactly equal to damage,
   - shield absorbs entire hit,
   - badass +1 before defence subtraction,
   - mount defence subtraction,
   - `CanDie == false` leaves 1 HP,
   - HP clamp to 0/max,
   - enemy damage callback and death threshold.
6. Do not hardcode values from field names; use evidence-backed constants and add a test per rule.

---

## 10. Reproduction commands

```bash
tools/re/il2cpp_re.py class DamageCarrier
tools/re/il2cpp_re.py class RoleAttribute
tools/re/il2cpp_re.py class RoleAttributePlayer
tools/re/il2cpp_re.py class RGController
tools/re/il2cpp_re.py class RGEController
tools/re/il2cpp_re.py disasm --address 0x11795CC --size 0x180
tools/re/il2cpp_re.py disasm --address 0x1179D44 --size 0xF0
tools/re/il2cpp_re.py disasm --address 0x1179E38 --size 0xB0
tools/re/il2cpp_re.py disasm --address 0xD3DD2C --size 0x1D0
tools/re/il2cpp_re.py disasm --address 0xD40418 --size 0x2B0
tools/re/il2cpp_re.py disasm --address 0xD40718 --size 0x120
tools/re/il2cpp_re.py disasm --address 0x1108E60 --size 0x250
tools/re/il2cpp_re.py disasm --address 0xDC8614 --size 0xA0
tools/re/il2cpp_re.py disasm --address 0x116A680 --size 0x150
```

---

**Bottom line:** the player damage path is largely reconstructed at the state-mutation level: incoming damage is reduced by `defence` and mount defence, one-shot shields absorb it, armor takes the hit first and overflows into HP, and `CanDie` can leave the player at 1 HP. The enemy path is a direct HP subtraction with a callback and death threshold. The projectile target bridge is resolved for player (`RGController.GetHurt`), enemy (`RGEController.GetHurt`), and box (`RGBox.Hit`). The remaining combat blockers are effect-trigger override behavior, critical/element rules, and full death/reborn branches.
