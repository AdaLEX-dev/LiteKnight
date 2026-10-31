# EnemyAI04 / e_slime01 Minimum Behaviour

**Scope:** the first-slice enemy only. This is not a reconstruction of all 20 AI
classes.

---

## 1. Asset identity

`e_slime01` prefab path: `Assets/RGPrefab/Enemy/2-castle/e_slime01.prefab`
(bundle `level-objects`).

Serialized components:

- `Transform`
- `Rigidbody2D` (dynamic, gravity scale 0, freeze rotation, mass 1)
- `BoxCollider2D` (1×1, offset `(0,0.5)`, trigger)
- `EnemyAI04`
- `RoleAttribute` (hp/max_hp 12, speed 3.0, speed_rate 0.0, camp 0)
- `Animator`
- `RGDropper`
- `BuffPivot`

`EnemyAI04` fields (`dump.cs` TypeDefIndex 6769):

```text
int damage;        // +0x184, serialized 3
float later_time;  // +0x188, serialized 1.5
GameObject dead_obj;  // +0x190
GameObject hit_object;// +0x198
```

The serialized `enemy_id` is `e_slime01`, `enemyTag` 66, `levelScene` 20,
`ai_level` 3, `shoot_cd` 1.0, `friction` 0.8, `reward_rate` 15,
`reward_value [0,0,1,1]`.

The display name "slime" is a **CANDIDATE**; the prefab/controller/stat identity is
CONFIRMED technical.

## 2. Base `RGEController` state machine

`EnemyAI04` inherits the shared `RGEController` behavior. Relevant methods:

- `Start` (`0x110B784`) / `Update` (`0x110B790`) / `FixedUpdate` (`0x110B7A0`)
- `SetUpAttribution` (`0x110BA8C`)
- `StartEnemyAI` (`0x110C1E0`)
- `EnemyUpdate` (`0x110C500`)
- `Scout` (`0x110C610`)
- `FindNearestTarget` (`0x110C950`)
- `FollowDefault` (`0x110CD68`)
- `AttackCommand` (`0x110D230`)
- `StartAttack` (`0x110D5D4`, base `ret`)
- `InAttack` (`0x110D5D8`, sets `shooting = true`)
- `EndAttack` (`0x110D5E4`, clears `shooting` and `can_hit`)
- `HitUnit` (`0x1110C18`)
- `GetHurt` (`0x1108E60`), `Dead` (`0x110DE18`)

Observed high-level behavior:

- `FixedUpdate` integrates velocity/force with the enemy's `inertial_vel`,
  `friction`, and `RoleAttribute.speed`; it uses `Rigidbody2D.velocity`.
- `EnemyUpdate` accumulates `seach_time` (`+0x128`) using `Time.deltaTime`; when it
  reaches a threshold, it invokes the virtual at vtable `+0x2E8` (the scout/AI
  update hook; exact slot owner not yet named).
- `Scout` returns if `dead`, handles dizzy state (`DizzyScout` coroutine), clears
  `target_obj` (`+0xF8`), draws `RGRandom.Range(0,10)`, compares against
  `ai_level` (`+0x60`), and chooses between target search/default follow and attack
  command paths.
- `FindNearestTarget(radius, masks)` and `FollowDefault` provide the movement/
  target acquisition primitives.
- `HitUnit(other, atk, cd)` checks buff 7 (`emBuff.ShieldSting`), the enemy's
  awake/dead flags, the target's tag (`Body_P`/`Body_E`), invulnerability, and then
  calls the player/pet damage path (including `RGController.GetHurt` / related
  controller methods). The target branch is partially decoded; it is not a generic
  `GetHurt` call for every target type.

## 3. `EnemyAI04` overrides

- `Awake` (`0x10E4A8C`): caches `Animator` (`+0xA0`), `RoleAttribute` (`+0xD0`),
  body renderer (`+0xB8`), finds the `body`/`hit_object` transforms, and sets up
  the base references.
- `RunReflection` (`0x10E4C04`): animation/visual reflection hook.
- `FixedRotation` (`0x10E4F78`): faces the movement direction.
- `ChildDead` (`0x10E5270`): if `dead_obj` is set, invokes `DeadEvent` after
  serialized `later_time = 1.5 s`.
- `DeadEvent` (`0x10E530C`): spawns the serialized `dead_obj` (`slime_dead`)
  prefab at the enemy position and calls its hit path.
- `OnTriggerEnter2D` (`0x10E54D0`): **direct tail-call**:

```text
HitUnit(other, this.damage /* 3 */, 1.5f);
```

This is the slime's contact damage. `later_time` is serialized 1.5; the observed
`HitUnit` cooldown argument is 1.5, so `later_time` is at least the same serialized
value; exact field-to-argument binding is HIGH CONFIDENCE but the method body shows
the constant `1.5f` directly.

## 4. Minimum native behavior contract

For the first slice, a native `EnemyAI04` can implement:

1. Spawn at a converted `e_slime01` prefab position.
2. Use `RoleAttribute` hp 12, speed 3.0, camp 0.
3. Fixed-step movement toward the current target/default target using the base
   `RGEController` velocity/force model; exact acceleration/friction formula is
   PARTIAL.
4. Contact trigger → `HitUnit(target, 3, 1.5)` semantics: apply 3 damage on a
   1.5-second cooldown to a valid player/pet target, with tag/buff/invulnerability
   checks.
5. On `GetHurt` HP <= 0, run `RGEController.Dead`, disable the collider
   immediately, play the recovered `slime01_dead` animator state, spawn
   `DeadEvent`/`slime_dead` after the serialized 1.5 s `later_time`, and
   eventually destroy the enemy. `dead_destory_delay_time` is initialized to
   4.0 s in `RGEController..ctor` (`0x110A960`); the 0.95 write in `GetHurt` is
   `friction`, not a death delay.

Do **not** claim the full slime AI, attack animation, pathfinding, or boss/network
behavior. The minimal contract is evidence-backed, but the exact `Scout` branch
ordering and destruction-flag predicate remain HIGH CONFIDENCE/PARTIAL.

### Recovered death facts

- `GetHurt` returns immediately when `dead`; a dying/dead enemy cannot be damaged
  again.
- `RGEController.Dead` sets `dead`, disables the collider, and removes delegates
  before the visual/death-event path.
- `EnemyAI04.ChildDead` schedules `DeadEvent` after `later_time = 1.5 s`.
- `DeadEvent` spawns `slime_dead`; this is a separate reference object. The
  native slice keeps the enemy's own recovered `enemy01_6` dead sprite instead
  and does not instantiate `slime_dead`.
- `RGEController..ctor` initializes `dead_destory_delay_time` (offset `+0x9C`)
  to 4.0 s. Native uses 4.0 s for the dev fixture and documents the exact
  destruction predicate as PARTIAL.

## 5. Confidence

| Claim | Confidence | Evidence |
|---|---|---|
| `e_slime01` uses `EnemyAI04` | CONFIRMED | prefab component |
| hp 12 / speed 3 / damage 3 / later_time 1.5 | CONFIRMED | serialized fields |
| Contact damage calls `HitUnit(other, 3, 1.5)` | CONFIRMED | `0x10E54D0` |
| Collider disabled immediately on death | CONFIRMED | `0x110DE18` |
| Dead animation state `slime01_dead` 2.0 s | CONFIRMED | serialized Animator/clip |
| `DeadEvent` spawned after `later_time` 1.5 s | HIGH CONFIDENCE | `0x10E5270` |
| Destruction delay 4.0 s; 0.95 is friction | HIGH CONFIDENCE | `0x110A960`, `0x1108E60` |
| Base AI uses `Scout`/`FindNearestTarget`/`FollowDefault` | HIGH CONFIDENCE | `0x110C610`, `0x110C950`, `0x110CD68` |
| Exact scout branch ordering/cooldown formula | PARTIAL | disassembly requires full control-flow reconstruction |
| Display name "slime" | CANDIDATE | prefab name only |
