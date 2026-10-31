# Soul Knight 2.6.8 — Parity Matrix

> **Current truth (2026-10-31 integrity pass).** The previous matrix marked many
> subsystems `HIGH` confidence even though they were inferred from class/field
> names. Those rows were planning hypotheses, not evidence. They are preserved in
> Git history and summarized in `analysis/EVIDENCE_AUDIT.md`; they are not
> current parity claims.

**Status Definitions:**
- `UNKNOWN` — no evidence-backed behaviour reconstructed.
- `PARTIAL` — some behaviour reconstructed/implemented, with named gaps.
- `FUNCTIONAL` — works natively but is not verified against the reference.
- `HIGH-FIDELITY` — verified behavioural match with minor differences.
- `VERIFIED` — reference-verified with regression tests.

---

## Current Evidence-Backed Matrix

| Subsystem | Reference evidence | Native state | Evidence | Status |
|---|---|---|---|---|
| Reference identity | Unity **2018.4.12f1**, IL2CPP metadata **v24**, APK hash verified | N/A | `analysis/ARTIFACT.md`, `analysis/EVIDENCE_AUDIT.md` | **VERIFIED (structural)** |
| Build/test baseline | N/A — platform tooling, not game parity | Debug/RelWithDebInfo build; 22 doctest cases | `analysis/REPOSITORY_INTEGRITY_AUDIT.md` | **VERIFIED (tooling)** |
| RNG | `RGRandom` wrapper over Unity `Random`; Xorshift128 engine and wrapper reconstructed; map-seed path resolved | `UnityRandom` / `RGRandomStream` implemented and tested | `analysis/re/rgrandom.md`, `tests/unit/unity_random_test.cpp` | **PARTIAL** — `seed_state` 1/2 and map-length branch open |
| Player damage state mutation | `RGController.GetHurt`, `HurtArmor`, `HurtHp`, `RoleAttribute*`, `CanDie` partly reconstructed | Not implemented | `analysis/re/damage.md` | **PARTIAL** — projectile→controller bridge/crit/element unresolved |
| Enemy damage state mutation | `RGEController.GetHurt` HP subtraction/callback/death partly reconstructed; projectile→box `RGBox.Hit` bridge reconstructed | Not implemented | `analysis/re/damage.md` | **PARTIAL** — projectile→controller bridge/source filtering/death branches open |
| Asset extraction | Bundle contents readable with UnityPy; candidates are unverified | Not a runtime subsystem | `analysis/assets/extraction_manifest.json` | **PARTIAL** — identities unverified; conversion not started |
| Player movement/combat | Class/field structure only | Placeholder code only | — | **UNKNOWN** |
| Enemy AI | Class names/coroutine signatures only | Nothing | — | **UNKNOWN** |
| Weapons/projectiles | Class names/field structure only | Nothing | — | **UNKNOWN** |
| Rooms/generation | Class names/serialization classes only | Nothing | — | **UNKNOWN** |
| Items/progression | Class names only | Nothing | — | **UNKNOWN** |
| UI/audio/save | Class names/design proposals only | Audio/file-system wrapper exists; no game integration | — | **UNKNOWN** |
| Renderer | Unity renderer target known; no reference behaviour reconstructed | No Vulkan context/swapchain/batcher | `docs/RENDERING.md` | **UNKNOWN / NOT IMPLEMENTED** |

---

## Parity Gates

| Gate | Requirement | Status |
|---|---|---|
| Gate B (Understanding) | All subsystems mapped with evidence | ❌ **NOT MET** — RNG/damage partial; movement/AI/weapons/rooms/items/UI/audio/save not reconstructed |
| Gate C (Runtime) | Native app launches, renders, inputs | ❌ **NOT MET** — no renderer/swapchain/simulation; build/test baseline alone is not Gate C |
| Gate D (First Playable) | Core loop playable and FUNCTIONAL | ⛔ **BLOCKED** by Gate B/C and `docs/VERTICAL_SLICE.md` gates |
| Gate E (Content) | All discovered content PARTIAL+ | ⛔ **BLOCKED** |
| Gate F (Release) | All VERIFIED, tests pass | ⛔ **BLOCKED** |

---

## Historical Planning Rows (Superseded)

The 2026-10-30 matrix claimed high confidence for movement, combat, AI, weapons,
rooms, generation, items, progression, UI, audio, and save. Those claims were
based on class/field names and design assumptions. They must not be cited as
evidence. The old matrix is available in Git history and its claims are audited
in `analysis/EVIDENCE_AUDIT.md`.

---

## Tracking Rules

- Update a row only after an evidence document exists.
- Link the exact evidence file and test.
- A field name or class name is never sufficient evidence.
- `PARTIAL` rows must list their exact gaps.
- Do not mark Gate B/C complete to make the roadmap look green.

**Last Updated:** 2026-10-31 (integrity pass)
**Next Review:** When a new subsystem evidence document is accepted.
