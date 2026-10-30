# Soul Knight 2.6.8 — Unknown Systems Tracker

**Purpose:** Track unresolved mysteries from reverse engineering. Never allow unresolved mysteries to disappear.

---

## Unknown Systems

| ID | Description | Evidence | Possible Explanations | Next Experiment | Priority | Status |
|----|-------------|----------|----------------------|-----------------|----------|--------|
| UNK-001 | Exact RNG algorithm | `RGRandom`/`RGRandomInterface` classes exist; PCG32 suspected | PCG32, xoshiro256**, Unity's `Random` (not deterministic) | Disassemble `RGRandom.Next()` in Ghidra; compare output to known PCG32 | HIGH | 🔍 Investigating |
| UNK-002 | Damage formula | `RGBullet` → `DamageCarrier`; armor/crit/element fields exist | `(atk - def) * crit * element_mult`; or more complex with diminishing returns | Hook `DamageCarrier.ApplyDamage` in runtime; log inputs/outputs | HIGH | 🔍 Investigating |
| UNK-003 | Room generation algorithm | `MapManagerLevel` + `SerializeLinkerRoom` + `RGRoomX` | BSP + graph + template placement; `RGRandom` for randomness | Dump room graph for fixed seeds; compare structure | HIGH | 🔍 Investigating |
| UNK-004 | Enemy AI state machine details | 20 `EnemyAI##` classes + special variants; coroutines like `CheckPositionValid`, `DizzyScout` | Hierarchical state machine via coroutines; behavior trees | Log AI state transitions at runtime for each type | HIGH | 🔍 Investigating |
| UNK-005 | Projectile physics (gravity, bounce, pierce) | `RGBullet` derivatives: `RGBDelayDivision`, `RGBMoveToSpawn`, `RGBTRebound` | Per-projectile physics params; some homing, some bouncing | Spawn each projectile type; track trajectory frame-by-frame | MEDIUM | ⏳ Pending |
| UNK-006 | Status effect stacking rules | `RGBuff` + many `RGBulletBuffTrigger` variants | Stack additive, multiplicative, or refresh duration? | Apply same buff multiple times; measure effect | MEDIUM | ⏳ Pending |
| UNK-007 | Save file format/versioning | `RGSaveManager` exists; no JSON schema visible | BinaryFormatter? Custom binary? JSON? | Trigger save; inspect file in `$XDG_DATA_HOME` | MEDIUM | ⏳ Pending |
| UNK-008 | Animation event-driven mechanics | `Animator` parameters referenced in coroutines (e.g., `Attack`, `Hit`) | Animation events trigger code callbacks | Hook `AnimationEvent` receiver; log all events | LOW | ⏳ Pending |
| UNK-009 | Shader/effect system | Unity URP/Built-in? Custom shaders in AssetBundles? | Built-in RP with custom shaders; or URP | Check `GraphicsSettings.renderPipelineAsset` in dump | LOW | ⏳ Pending |
| UNK-010 | Multiplayer synchronization | `RGNetWorkManager`, `PendingPlayer`, `EnemySoccer`, `RaceController` | Photon? Unity Netcode? Custom? | Out of scope for v1 — document only | OUT OF SCOPE | 📝 Documented |
| UNK-011 | Boss phase transition logic | `RGEBossController` + `DetectingBossInfo` coroutine | HP thresholds? Time? Room state? | Fight each boss; log phase transitions with HP % | HIGH | 🔍 Investigating |
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

*None yet — Phase 3 beginning*

---

**Last Updated:** 2026-10-30
**Next Review:** After each Phase 3 experiment batch