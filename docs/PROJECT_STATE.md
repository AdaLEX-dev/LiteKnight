# Soul Knight 2.6.8 — Project State

**Last Updated:** 2026-10-31 (P0/P1 integrity pass)
**Commit:** session head; run `git log -1 --format='%h %s'`
**Phase:** Phase 3 — behavioural reconstruction (CURRENT)
**Current status:** Build/test/evidence pipeline is clean and reproducible; gameplay and renderer work remain blocked by evidence gates.

---

## Current Phase and Gates

| Phase | Status |
|---|---|
| Phase 0 — Environment | ✅ COMPLETE |
| Phase 1 — APK Forensics | ✅ COMPLETE (artifact inventory) |
| Phase 2 — Engine Reconstruction | ⚠️ PARTIAL — Gate B **NOT MET** |
| Phase 3 — Behavioural Reconstruction | 🔄 **CURRENT** — RNG core and damage state mutation partial |
| Phase 4 — Native Runtime | ⬜ NOT STARTED — Gate C **NOT MET** |
| Phase 5 — First Playable | ⛔ BLOCKED |
| Phase 6+ — Systems/Content/Fidelity | ⛔ BLOCKED |

Canonical baseline:

- Soul Knight 2.6.8
- Unity **2018.4.12f1**
- IL2CPP metadata **v24**
- CachyOS x86-64
- Wayland only
- Vulkan target
- 1920×1080, 60 FPS target
- native executable
- no Android runtime, Waydroid, emulator, Wine, X11, or XWayland

---

## Completed Integrity Work

- Corrected Unity/metadata claims and removed PCG32 parity.
- Build now uses system Vulkan headers, `fmt`, and the Vulkan loader.
- SDL3 Vulkan API, filesystem paths, ECS, doctest, input injection, and CTest filters fixed.
- Tests execute for real: **22 test cases / 1273 assertions**.
- Intentional test failure turns CTest red; restored suite passes.
- Debug and RelWithDebInfo builds verified from clean build directories.
- Added reproducible IL2CPP metadata → RVA → disassembly → xref tooling.
- Added optional Ghidra annotation script and validated it on a symbol set.
- Added reproducible UnityPy extraction tooling and first-pass manifest (284 entries).
- Removed fabricated Vulkan compatibility headers and unimplemented renderer/audio placeholders.
- Added fail-fast Wayland-only driver enforcement.
- Removed `-march=native` from Release and DEB packaging assumptions.
- Rewrote current project state/parity/roadmap to distinguish current truth from history.

---

## In Progress — Phase 3

### RGRandom — PARTIAL

Evidence: `analysis/re/rgrandom.md`
Native: `src/engine/math/UnityRandom.*`
Tests: `tests/unit/unity_random_test.cpp`

Reconstructed and tested:

- `seed_state`, `seed`, `this_state` layout,
- `SetRandomSeed` global save/restore,
- lazy seeding from `RGGameInfo.sample_random_seed`,
- map seed path through `MapManager.Start` and virtual `SetRGRandomSeed`,
- per-object independent streams,
- `RGController.rg_random` seeded in `AwakeController` with `sample_random_seed + skillIndex`,
- Unity Xorshift128 engine, 23-bit float values, integer edge cases, `insideUnitCircle`.

Still open:

- `seed_state` values `C_READY`/`S_SEND_SEED` (1/2) — no static write path found.
- `MapManagerLevel.SetRGRandomSeed` extra `Range(0,100)`/LongMap branch details.
- Cross-platform bit-exact `sinf`/`cosf`/`sqrtf` parity.

### Damage — PARTIAL

Evidence: `analysis/re/damage.md`
Native gameplay: **not implemented** (correctly blocked).

Reconstructed:

- `DamageCarrier.UpdateInfo` addition modifier,
- player `GetHurt` badass/defence/mount/one-shot-shield/armor path,
- `HurtArmor` overflow into HP, `emBuff.ShieldBlue` (`6`) guard,
- `HurtHp` → `Dead`,
- `RoleAttribute`/`RoleAttributePlayer` HP clamp and `CanDie` survival,
- enemy `GetHurt` HP subtraction/callback/death,
- `RGBulletTrigger.GetFinalDamage` effect-factor multiplication,
- `emBattleFactor.EnemyDefence` (`6`) and `HardShield` (`8`),
- `emBuff.ShieldIce` (`23`) damage factor branch,
- the projectile → box bridge through shared `GameObject.GetComponent<T>` (`0xFF8A84`) and virtual `RGBox.Hit` at `0x1160B70` (HIGH CONFIDENCE),
- the enemy/player branch virtual call at `0x1160F78` (vtable `+0x2C8`), target type unresolved.

Still open:

- exact projectile → controller `GetHurt` bridge/target type,
- critical-hit application,
- element/status formulas,
- `CanDie` generic singleton type at `+0x1C` (candidates: `RGGameInfo.map_random_seed`, `RGGameProcess.this_index`),
- full death/reborn branches.

---

## Build and Test Verification

Recorded in `analysis/REPOSITORY_INTEGRITY_AUDIT.md`. Commands run from a clean
`build/`:

```bash
./tools/configure.sh Debug
./tools/build.sh Debug
./tools/test.sh
ctest --test-dir build --output-on-failure

# negative control: break one assertion, rebuild soul-knight-test, run ctest
ctest --test-dir build --output-on-failure   # must FAIL
# restore the assertion, rebuild, run ctest again

./tools/configure.sh RelWithDebInfo   # after removing build/
./tools/build.sh RelWithDebInfo
./tools/test.sh RelWithDebInfo
ctest --test-dir build --output-on-failure
git diff --check
```

Current baseline:

- Debug clean configure/build: PASS, zero compiler warnings
- 22 test cases, 1273 assertions: PASS
- intentional test break: CTest FAIL (exit 8; 1/22 cases failed)
- restored tests: PASS
- RelWithDebInfo clean configure/build: PASS, zero compiler warnings
- RelWithDebInfo tests: PASS (22 cases / 1273 assertions)
- `git diff --check`: clean

The test count did not change. If it changes, explain the added/removed test in
the integrity audit.

---

## Performance

- **Target:** 16.67 ms/frame (60 FPS @ 1080p)
- **Current:** not measured — no renderer or playable build.
- No performance claim is valid yet.

---

## Blockers / Unknown Areas

1. No runtime observation oracle (no device/emulator/Frida); static evidence only.
2. Damage projectile→target bridge and critical/element rules.
3. RGRandom `seed_state` 1/2 and map-length branch.
4. Renderer/Vulkan context/swapchain not implemented; Gate C unmet.
5. Input semantics, room hierarchy/collision, enemy AI, weapon/projectile behaviour not reconstructed.
6. Vertical slice is blocked by the gates in `docs/VERTICAL_SLICE.md`.

---

## Next Actions (Evidence Only)

1. Close the damage bridge/critical/element gaps from static analysis.
2. Finish RGRandom residual questions or document exact missing evidence.
3. Do **not** start renderer/gameplay/vertical-slice implementation until the evidence gates pass.

---

## Evidence References

| Document | Location |
|---|---|
| APK hash | `analysis/original-apk.sha256` |
| Artifact record | `analysis/ARTIFACT.md` |
| APK inventory | `analysis/APK_INVENTORY.md` |
| DEX class map | `analysis/class-map.md` |
| Native libraries | `analysis/native-libraries.md` |
| Resources/assets | `analysis/resources.md` |
| Diagnosis (historical) | `analysis/PROJECT_DIAGNOSIS.md` |
| Evidence audit (historical + current claims) | `analysis/EVIDENCE_AUDIT.md` |
| Integrity audit | `analysis/REPOSITORY_INTEGRITY_AUDIT.md` |
| RNG reconstruction | `analysis/re/rgrandom.md` |
| Damage reconstruction | `analysis/re/damage.md` |
| Asset manifest | `analysis/assets/extraction_manifest.json` |
| Architecture inventory | `docs/ARCHITECTURE.md` |
| RE log | `docs/REVERSE_ENGINEERING.md` |
| Technology decisions / ADRs | `docs/TECHNOLOGY_DECISIONS.md`, `docs/DECISIONS/` |
| Full metadata dump (outside repo) | `/home/jules/bin/dump.cs`, `script.json`, `il2cpp.h`, `DummyDll/` |
| In-repo partial dump | `analysis/il2cpp_dump/` (not authoritative) |

---

## Historical Record

The 2026-10-30 documents claimed Phase 2 completion, Unity 2021.x / metadata
v24.1, PCG32 parity, and a working test suite. Those claims were false and are
preserved as historical evidence in:

- `analysis/PROJECT_DIAGNOSIS.md`
- `analysis/EVIDENCE_AUDIT.md`
- the Git history before the 2026-10-31 evidence-first commits.

Do not use those historical claims as current truth.
