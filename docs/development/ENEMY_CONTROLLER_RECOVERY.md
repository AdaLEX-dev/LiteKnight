# Enemy Controller Recovery

## Reference result

All 21 recovered enemy controllers inherit `RGEController`. There are **no
summon, heal or teleport methods** in these classes. The distinct mechanics are:

| Family | Controllers | Recovered behavior |
|---|---|---|
| Contact chaser | `EnemyAI04` | direct contact `HitUnit`, delayed `dead_obj` spawn |
| Hand chassis | `EnemyAI01/02/06/07/15` | `RGEHand.SetAttackTrigger`, `shoot_cd`, optional second hand / scripted projectiles |
| Continuous/rotating | `EnemyAI03/18/19` | `SetAttack(true)` for `shoot_time`, stand/lock flags, rotating hand (AI18), dual hand (AI19) |
| Scripted projectiles | `EnemyAI10/11/12/14/16/20` | fan/burst/laser/fireball patterns and timers |
| Dash/jump | `EnemyAI09`, `EnemyAIFrog`, `EnemyAIWitch` | directed high-speed window + contact; Witch waves every 0.225 s |
| Proximity/death bomber | `EnemyAI13` | `BulletBoom.StartBoom` at radius 6 and on death; 2 s fuse, damage 10 |
| Fade ambusher | `EnemyAIGhost` | Animator `fade` bool, show/fade events, hand attack |
| Loot mimic | `EnemyAI08` | `ItemInterface`, chest-level HP scaling, weapon drop |

Exact addresses and per-prefab values are in the recovery report; the native
data lives in `analysis/assets/enemies/enemy_catalogue.json` and each scene's
`enemy_definitions`.

## Implemented native families

`EnemySystem` now consumes additional serialized fields:

- `projectile_count`, `fan_angle` — AI20 emits `2*bulletCount+1` bullets in 15°
  steps; AI01 with `img/h2` emits two hands; AI10 emits its second bullet when
  `bullet02` is non-null.
- `burst_count`, `burst_interval` — AI11/14 and continuous AI03/18/19 fire
  timed volleys instead of one shot per `shoot_cd`.
- `dash_speed`, `dash_duration` — AI09 (8.0/1.5 s), AIFrog (0.5 s) and AIWitch
  (10.0/1.5 s) use a directed dash window; Witch emits its 0.225 s waves.
- `death_explosion_radius`, `death_explosion_damage` — AI13 and AIFrog apply
  their recovered death blast.
- `stationary` — AI06/AI18 do not chase; AI18 keeps its rotating-hand attack.
- `invulnerable_while_attacking` — AI16 receives its recovered ~1.562 s
  invulnerable window at the start of its burst; damage is rejected for the
  window and then accepted again.
- `mimic` — AI08 is classified as an item container rather than a generic
  combat chaser.
- `stand_in_shooting`, `lock_in_shooting`, `shoot_time` remain parsed and are
  used by the continuous family.

All movement remains deterministic 50 Hz and collision-slides against the
recovered room world. Contact damage keeps the recovered 1.5 s cooldown.

## Tests

`tests/unit/enemy_family_test.cpp` covers:

- AI20 fan count/angles.
- AI11/14 timed burst count.
- AI09 dash distance/window.
- AI13 death explosion damage.
- stationary controller fires without chasing.

## Remaining limitations

- AI07's three random attacks, AI12's lobbed fireball arc, AI14's exact laser
  visual, AI13's armed idle animation, AIGhost fade timing and AI08's loot UI
  are not fully reproduced. The shared runtime uses the strongest serialized
  value available (burst/fan/dash/death/invulnerability fields); these
  remaining pieces are data-limited presentation/state details, not invented
  alternative mechanics.
- `shoot_time` continuous fire uses a native 0.2 s cadence because the
  hand-weapon projectile interval is not serialized on the controller; this is
  documented as a native timing policy, not a prefab value.
