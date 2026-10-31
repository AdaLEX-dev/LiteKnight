# Soul Knight 2.6.8 — Unknown Systems Tracker

**Purpose:** Track unresolved mysteries from reverse engineering. Never allow unresolved mysteries to disappear.

---

## Unknown Systems

| ID | Description | Evidence | Possible Explanations | Next Experiment | Priority | Status |
|----|-------------|----------|----------------------|-----------------|----------|--------|
| UNK-001 | Exact `RGRandom` semantics over `UnityEngine.Random` | `RGRandom` fields `seed_state`, `seed`, `this_state`; wrapper/Xorshift128/map-seed path reconstructed | Wrapper around Unity 2018.4 `Random`, not PCG32 | Resolve `seed_state` 1/2 and DateTime seed exactness; obtain reference vectors | HIGH | ⚠️ PARTIAL (`analysis/re/rgrandom.md`) |
| UNK-002 | Damage formula and application path | Player `+0x228`, enemy `+0x2C8`, box `+0x168` bridges confirmed; armor/HP/CanDie reconstructed | Crit/element/effect-trigger details remain | Decode effect-trigger `TriggerWith`/crit consumers | HIGH | ⚠️ PARTIAL (`analysis/re/damage.md`) |
| UNK-003 | Room generation algorithm | `MapManagerLevel` + `SerializeLinkerRoom` + `RGRoomX` | BSP + graph + template placement is a hypothesis | Dump room graph for fixed seeds; compare structure | HIGH | ⏳ Pending |
| UNK-004 | Enemy AI state machine details | 20 `EnemyAI##` classes + special variants; coroutines like `CheckPositionValid`, `DizzyScout` | Hierarchical state machine via coroutines; behavior trees are hypotheses | Log AI state transitions at runtime for each type | HIGH | ⏳ Pending |
| UNK-005 | Projectile physics (gravity, bounce, pierce) | `Bullet01` fields + `RGBullet.UpdateInfo`; `bullet_0` destroy_time 5, can_rebound 1 | Exact velocity/rebound/through math unresolved | Trace `RGBullet.SetBulletVelocity`/rebound; compare bullet_0 | MEDIUM | ⚠️ PARTIAL (`analysis/re/weapons_projectiles.md`) |
| UNK-006 | Status effect stacking rules | `RGBuff` + many `RGBulletBuffTrigger` variants | Stack additive, multiplicative, or refresh duration? | Apply same buff multiple times; measure effect | MEDIUM | ⏳ Pending |
| UNK-007 | Save file format/versioning | `RGSaveManager` fields/`AboGameData`; serialization not decoded | Format UNKNOWN | Disassemble `RGSaveManager.Save/Load` | MEDIUM | ⏳ Pending |
| UNK-008 | Animation event-driven mechanics | `Animator` parameters referenced in coroutines (e.g., `Attack`, `Hit`) | Animation events trigger code callbacks | Hook `AnimationEvent` receiver; log all events | LOW | ⏳ Pending |
| UNK-009 | Shader/effect system | `GraphicsSettings` has no custom render pipeline asset; built-in shaders present | Built-in RP indicated; exact shader/material behavior unknown | Decode graphics settings + material shaders | LOW | ⚠️ PARTIAL |
| UNK-010 | Multiplayer synchronization | `RGNetWorkManager`, `PendingPlayer`, `EnemySoccer`, `RaceController` | Photon? Unity Netcode? Custom? | Out of scope for v1 — document only | OUT OF SCOPE | 📝 Documented |
| UNK-011 | Boss phase transition logic | `RGEBossController` + `DetectingBossInfo` coroutine | HP thresholds/time/room-state are hypotheses | Fight each boss; log phase transitions with HP % | HIGH | ⏳ Pending |
| UNK-012 | Weapon upgrade/mod system | `WeaponItemInfo`, `WeaponInBattle`, `SupportWeapon` | Level-based stats? Mod slots? | Check weapon data in `RGGameConst`/`RGGameInfo` | MEDIUM | ⏳ Pending |
| UNK-013 | Pet/companion system | `RGPetController`, `RGPetInfo`, `PetBugHandler` coroutine | Passive followers? Active attackers? | Spawn pet; observe behavior | LOW | ⏳ Pending |
| UNK-014 | Mount/vehicle system | `RGMountController`, `RGRaceCar`, `ConsumingHp` coroutine | Temporary powerups? Persistent? | Enter race mode; test mount mechanics | LOW | ⏳ Pending |
| UNK-015 | Sandbox/creative mode | `MapManagerSandbox`, `SandboxObjectData`, `ItemObjectProvider` | Level editor? Debug mode? | Enter sandbox; test object placement | OUT OF SCOPE | 📝 Documented |

---

## Experiment Template

For each unknown, create experiment file: `analysis/experiments/EXP-<ID>.md`

```markdown
# Experiment EXP-<ID>

## Question
What is the exact behavior of X?

## Hypothesis
Based on evidence Y, we expect Z.

## Setup
- Environment: Native / Android (specify)
- Tools: Ghidra / Frida / Runtime logging / Custom test harness
- Seed: Fixed (e.g., 12345)

## Action
1. Step 1
2. Step 2

## Observation
- Raw data
- Screenshots/video

## Expected (if hypothesis correct)

## Actual (observed)

## Conclusion
- Hypothesis CONFIRMED / REJECTED / INCONCLUSIVE
- New evidence gained

## Next Steps
```

---

## Status Legend

| Icon | Status | Meaning |
|------|--------|---------|
| 🔍 | Investigating | Active experiment in progress |
| ⏳ | Pending | Not yet started |
| ✅ | Resolved | Confirmed with evidence |
| ❌ | Contradicted | Hypothesis disproven |
| 📝 | Documented | Out of scope or fully understood |

---

## Update Protocol

1. When starting investigation: Change status to 🔍, create experiment file
2. When evidence gathered: Update experiment file with results
3. When resolved: Change to ✅, update `docs/ARCHITECTURE.md` and relevant subsystem doc
4. If contradicted: Change to ❌, record new hypothesis
5. Never delete — move to bottom with "Resolved" section

---

## Resolved (Moved from Active)

- **UNK-016 Timing:** RESOLVED — `TimeManager.Fixed_Timestep = 0.02` (50 Hz); see `analysis/re/timing.md`.
- **UNK-017 Damage bridge:** RESOLVED — player/enemy/box dispatch confirmed; see `analysis/re/damage.md`.
- **UNK-018 CanDie singleton:** RESOLVED — `RGGameProcess.this_index`; see `analysis/re/damage.md`.

---

**Last Updated:** 2026-10-31 (Gate B / Phase 3 pass)
**Next Review:** After each Phase 3 experiment batch