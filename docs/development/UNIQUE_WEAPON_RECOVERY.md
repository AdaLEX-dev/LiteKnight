# Unique Weapon Recovery (Soul Knight 2.6.8)

This document records the 24 catalogue entries that were previously Tier B/C.
The APK is the authority. `weapon_runtime_integration.json` is the machine
readable status table; this page is the evidence/implementation summary.

## Result

- 18 of 24 prefabs recovered and integrated.
- 6 names are localization-only in 2.6.8: no prefab, no `WeaponInfo` row, no
  serialized stats, no controller association. They remain catalogue stubs.
- Catalogue total: **302**. Runtime definitions: **296**. Localization-only:
  **6**.
- New runtime families: animation-event bursts, multi-pellet elemental cycles,
  summons/funnel actors, buffs, full-screen area effects, deployable boxes,
  death marks, revive, channel black hole, and energy accounting for all of
  them.

## Recovered table

| Weapon | Class | Evidence | Implemented behavior | Evidence level | Test |
|---|---|---|---|---|---|
| 116 Staff of Skeleton | `GunSummon01` | `Attack @0x0129FA44`; `npc_skeleton_01/02`; NpcSummon01 fields | summon skeleton ally, 15 s, HP8, melee or ranged buff branch | CONFIRMED | `Summon actors expire…` |
| 160 Black Hole Missile | `GunChannel` | `AttackStart @0x0143AD40`, `CreateEndShootBullet @0x0143B810`; `bullet_84`/`bullet_89` | hold-to-channel field, 0.8 s end explosion, energy drain | CONFIRMED | gameplay/structural |
| 173 Rainbow | `GunMultiBullet02` | `Attack @0x01295FAC`, `CreateBullet @0x012960A0`; six `bullets` | 4 pellets at -16/-8/0/+8°, cycles six elemental prefabs | CONFIRMED | `GunMultiBullet02 cycles…` |
| 174 Staff of Illusion | `GunPhantom` | `Attack @0x01299814`; `npc_char_phantom`; `PhantomSplitProcessor` | 50%-damage illusion summon, HP100, copies weapon path | HIGH | `Summon actors expire…` |
| 195 Trekking Pole | `Gun012` | `Gun012.Attack @0x0142DF78`; `bullet_21` Divide | passive walk-staff; nothing invented without cadence | HYPOTHESIS | structural |
| 214 Extra Crown | `GunCrown` | `AttackStart @0x0143DE58`; `BuffStealth` fields | 10 s stealth, alpha 0.297, enemies lose target | HIGH | manual/structural |
| 216 Bayonet Rifle | `GunThrowSword` | `Attack @0x00D4D71C`, `CreateSword @0x00D4DB50`; two bullet entries | rifle shot + recovered melee/thrown spear data; mode switch partial | HIGH | weapon catalogue |
| 218 Assault Sniper Rifle | `Gun001` | override `w_combo_rifle` events 0.1/0.4/0.7667 | three-round animation-event burst | CONFIRMED | `Recovered animation burst…` |
| 238 Retouch-free Camera | `GunCamera` | `Attack @0x01439474`; `BulletFullScreen` | full-screen Energy damage 2, range 13, flash 0.15 s | HIGH | `Camera flash applies…` |
| 242 Box | `GunBox` | `Attack @0x01434378`; `RGBox.hp=4` | deployable box, HP4, solid, destructible, drops energy | HIGH | structural |
| 244 Nirvana Cross | `GunRebornCross` | only `SetAttack @0x0129C93C`; no Attack/projectile | passive revive on lethal damage; consumed once | HYPOTHESIS | manual |
| 254 Staff of Anubis | `Gun012` | `multiCount=3`, `angle=10`, Magic bullet | 3-shot Anubis pattern; status semantics not serialized | HYPOTHESIS | weapon catalogue |
| 270 Bayonet in a Gourd | `GunGourdWithFunnelSword` | `Attack @0x014448E4`, `CreatingFunnel @0x01444A50` | 2 flying sword funnels for 8 s, HP5, atk_cd 2 | HIGH | `Funnel swords orbit…` |
| 273 Trumpet | `GunTrump` | `Attack @0x00D50BF0`; `BuffAtkSpeedChange` | friendly attack-speed buff 4 s, +25%; weapon add_atkspeed documented | HIGH | `Attack-speed buff…` |
| 275 Sweet Talk | `Gun001` | `bullet_27_invisible`; `WeaponItemConfig` | invisible Fire+Explode missile; support item list retained | HIGH | structural |
| 276 Death Note | `GunDeadNote` | `Attack @0x0143F1C0`; `BuffDeadNote.KillEnemy @0x013502D8` | mark target, kill after 6 s | HIGH | `BuffDeadNote kills…` |
| 293 Happy New Year | `GunThrow` | `WeaponSpecialEatable`; `WeaponEatGift.weaponWeights` | throwable gift + eatable special data; reward pool not serialized | HIGH | structural |
| 294 Ranger's Revolver | `Gun008` | `Attack @0x0142C1CC`, `CreateBullet @0x0142C21C`; two bullet entries | 6-round burst at 0.12 s, alternating recovered entries | CONFIRMED | `Gun008 burst count…` |

## Unrecoverable entries

`weapon_232`, `weapon_262`, `weapon_263`, `weapon_269`, `weapon_271`,
`weapon_272` have only localization names. There is no prefab in the common,
hall, level-common, level-objects, sandbox, test, scene_game or defence
bundles, and no `WeaponInfo` row in the 2.6.8 table. No stats, recipes, prices,
projectiles, animators or mechanics are invented for them.

## Runtime design

The existing `WeaponDefinition`/`Weapon` architecture is extended, not
replaced:

- `attack_event_times` holds every scaled `Attack` animation event per cycle.
- `WeaponSpecial` holds the serialized subclass values and alternate prefabs.
- `Weapon` owns the attack cadence, energy cost and projectile arrays.
- `Application` consumes `WeaponFireEvent` for effects that need world state
  (summons, buffs, area damage, black hole, deployable box, death mark, revive).
- `SummonSystem` implements the recovered follower/funnel actors.
- `EnemySystem` owns death marks, area damage and black-hole pull.

All timings are serialized animation-event or code-field timings. Where the
data does not contain a value (mode-switch condition, Anubis status, exact
revive hook), the table marks HYPOTHESIS and the implementation stays
evidence-backed rather than inventing a different mechanic.
