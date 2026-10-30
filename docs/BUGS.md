# Soul Knight 2.6.8 — Bug Tracker

**Format:**
```
ID: BUG-XXXX
Severity: P0-P4
Reproduction: Steps to reproduce
Expected: What should happen
Actual: What happens
Suspected Cause: Hypothesis
Evidence: Logs, screenshots, save files
Fix: Resolution (when fixed)
Regression Test: Test name added
```

---

## Severity Definitions

| Level | Label | Description | Response Time |
|-------|-------|-------------|---------------|
| P0 | Crash/Data Loss | Game crashes, saves corrupted, undefined behavior | Immediate |
| P1 | Game-Breaking | Core mechanic broken, softlock, major progression block | Same day |
| P2 | Major Fidelity | Behavior deviates significantly from reference | This sprint |
| P3 | Minor | Visual glitch, audio desync, UI issue | Next sprint |
| P4 | Polish | Cosmetic, nice-to-have, edge case | When possible |

---

## Active Bugs

*None yet — Phase 3 not started*

---

## Resolved Bugs

*None yet*

---

## Bug Template (Copy for new bugs)

```markdown
## BUG-XXXX: Brief Title

**Severity:** P0/P1/P2/P3/P4
**Reported:** YYYY-MM-DD
**Build:** Commit hash / version
**Platform:** CachyOS / Wayland / Vulkan

### Reproduction
1. Step 1
2. Step 2
3. Step 3

### Expected
What should happen based on reference behavior.

### Actual
What actually happens (with evidence).

### Suspected Cause
Hypothesis based on code inspection.

### Evidence
- Log excerpt
- Screenshot/video link
- Save file (if applicable)
- Minimal repro project (if applicable)

### Fix
(When resolved)
- What was changed
- Why it fixes it

### Regression Test
- Test name: `test_<subsystem>_<bug_description>`
- Location: `tests/<category>/`
- Verifies: The specific bug is fixed
```

---

## Bug Triage Process

1. **Report** — Fill template, assign ID (next sequential)
2. **Triage** — Assign severity, owner, milestone
3. **Investigate** — Minimize repro, locate boundary
4. **Fix** — Implement fix, add regression test
5. **Verify** — Run test, check for regressions
6. **Close** — Update BUGS.md with fix details

---

## Bug Categories

| Prefix | Subsystem |
|--------|-----------|
| BUG-REND- | Rendering |
| BUG-INPT- | Input |
| BUG-AUDI- | Audio |
| BUG-PHYS- | Physics/Collision |
| BUG-AI-- | Enemy AI |
| BUG-COMB- | Combat/Damage |
| BUG-GEN- | Procedural Generation |
| BUG-SAVE- | Save/Load |
| BUG-UI-- | UI/UX |
| BUG-PERF- | Performance |
| BUG-CORE- | Engine Core |
| BUG-PLAT- | Platform (Wayland/Vulkan/SDL) |

---

## Example Entries

### BUG-REND-0001: Sprite atlas UV bleeding at mip boundaries
**Severity:** P3
**Reproduction:** 1. Zoom camera out 2. Observe 1px lines at sprite edges
**Expected:** Clean sprite edges at all zoom levels
**Actual:** 1px color bleeding from adjacent atlas sprites
**Suspected Cause:** Atlas padding insufficient for mipmap generation
**Evidence:** Screenshot at 0.5x zoom
**Fix:** Increase atlas padding to 2px + premultiply alpha
**Regression Test:** `test_rendering_atlas_mip_bleeding`

### BUG-COMB-0002: Critical hits not applying on first frame of dodge
**Severity:** P2
**Reproduction:** 1. Start dodge 2. Get hit by enemy projectile on frame 1 3. Observe no crit
**Expected:** Crit rolls should be independent of dodge state
**Actual:** Crit chance = 0% during dodge invincibility frames
**Suspected Cause:** `DamageCarrier` checks `target.is_dodging` before crit roll
**Evidence:** Combat log shows 0% crit during dodge
**Fix:** Move crit roll before dodge check
**Regression Test:** `test_combat_crit_during_dodge`

### BUG-GEN-0003: Same seed generates different dungeon on Linux vs Android
**Severity:** P0 (if true) / P1
**Reproduction:** 1. Generate dungeon with seed 12345 on Android 2. Generate on Linux 3. Compare room graph
**Expected:** Identical room graph, enemy placement, loot
**Actual:** Room 3 has different enemy type
**Suspected Cause:** RNG stream contamination (combat RNG called during gen?)
**Evidence:** Room graph dumps from both platforms
**Fix:** Strict RNG stream isolation
**Regression Test:** `test_generation_determinism_cross_platform`

---

## Bug Statistics

| Severity | Open | Resolved This Sprint | Total Resolved |
|----------|------|---------------------|----------------|
| P0 | 0 | 0 | 0 |
| P1 | 0 | 0 | 0 |
| P2 | 0 | 0 | 0 |
| P3 | 0 | 0 | 0 |
| P4 | 0 | 0 | 0 |
| **Total** | **0** | **0** | **0** |

---

**Last Updated:** 2026-10-30
**Next Triage:** After Phase 4 (Gate C)