# Soul Knight Linux — Evidence Audit

> **HISTORICAL DOCUMENT (2026-10-31, pre-repair).** This audit records the false
> claims found before the P0/P1 integrity pass and the evidence that contradicted
> them. The claims have since been corrected in the live documents. See
> `docs/PROJECT_STATE.md` and `analysis/REPOSITORY_INTEGRITY_AUDIT.md` for
> current truth. The verdict counts below describe the pre-repair repository,
> not the present tree.

**Date:** 2026-10-31
**Purpose:** Check the project's factual claims against the actual APK, metadata, build, and source tree.
**Companion report:** `analysis/PROJECT_DIAGNOSIS.md`

---

## Verdict Summary

| Verdict | Count | Meaning |
|---|---:|---|
| VERIFIED | 9 | Directly confirmed from the artifact or environment |
| PARTIAL | 12 | Directionally true, but imprecise or incomplete |
| UNSUPPORTED | 4 | Plausible hypothesis with no evidence in the repository |
| CONTRADICTED | 13 | Directly contradicted by the artifact or environment |
| BROKEN CITATION | 2 | Claim cites evidence that does not contain the claim |
| **Total** | **40** | |

The most damaging contradictions are:

1. Unity **2021.x** is claimed; the APK is **2018.4.12f1**.
2. Metadata **v24.1** is claimed; the header is **v24**.
3. `RGRandom` is treated as replaceable by **PCG32**; the binary delegates to **UnityEngine.Random**.
4. "Phase 2 complete / Gate B" is claimed; behaviour has not been reconstructed.
5. "Blocked: none" is claimed; there is no runtime oracle and no asset pipeline.
6. Tests are reported as zero; they are actually **disabled**, and do not compile when enabled.

---

## Claim-by-Claim Audit

| ID | Claim | Where | Check | Verdict |
|---|---|---|---|---|
| A-001 | Reference APK SHA-256 is `8faff890...0137ea` | `analysis/original-apk.sha256`, `docs/ROADMAP.md:5` | `sha256sum original/SoulKnight-2.6.8.apk` matches exactly | **VERIFIED** |
| A-002 | Package is `com.ChillyRoom.DungeonShooter`, version 2.6.8 (20680) | `analysis/ARTIFACT.md` | Confirmed from APK manifest/apktool output and `bundleVersionData.txt` | **VERIFIED** |
| A-003 | Engine is Unity IL2CPP | `analysis/ARTIFACT.md`, `docs/ARCHITECTURE.md` | `libil2cpp.so` 52 MB, `global-metadata.dat`, Unity libs present | **VERIFIED** |
| A-004 | All gameplay logic is in `libil2cpp.so`; DEX has no game logic | `analysis/class-map.md` | DEX inventory shows Unity bootstrap + SDKs; no gameplay packages found | **VERIFIED** (high confidence, not exhaustively proven) |
| A-005 | 20 numbered enemy AI classes exist | `docs/ARCHITECTURE.md`, `docs/PROJECT_STATE.md` | `dump.cs` has `EnemyAI01`–`EnemyAI20` plus special variants | **VERIFIED** |
| A-006 | 216 room bundles exist | `analysis/resources.md` | `ls assets/AssetBundles/room | wc -l` = 216 | **VERIFIED** |
| A-007 | AssetBundles contain sprites, audio, prefabs, animations | `analysis/resources.md` | UnityPy read `common`/`level-objects`/`hall`/`r1_1` successfully | **VERIFIED** |
| A-008 | System tooling works (SDL3, Vulkan, Wayland, GCC, CMake, Git) | `docs/REVERSE_ENGINEERING.md` | `pkg-config` finds SDL3 3.4.18 and Vulkan 1.4.363; Wayland session active; build tools present | **VERIFIED** |
| A-009 | Unity version is 2021.x | `analysis/ARTIFACT.md:33`, `docs/PROJECT_STATE.md` (2026-10-30 section), `docs/ARCHITECTURE.md:11` | Bundle header says `2018.4.12f1`; `libunity.so` and `libil2cpp.so` strings say `2018.4.12f1` | **CONTRADICTED** |
| A-010 | IL2CPP metadata is v24.1 | `docs/PROJECT_STATE.md` (2026-10-30 section), `docs/REVERSE_ENGINEERING.md:87` | `global-metadata.dat` bytes 4–7 = `24`; no `.1` revision in header | **CONTRADICTED** |
| A-011 | AssetBundles use Unity 2021 format | `analysis/resources.md` §2 "AssetBundle Format Details" | Header is UnityFS with build string `2018.4.12f1` | **CONTRADICTED** |
| A-012 | RNG can be replaced with PCG32 while preserving parity | `docs/TECHNOLOGY_DECISIONS.md` ADR-0014 | `RGRandom.Range` at `0xDD07E8` calls `UnityEngine.Random$$Range` at `0x1AF8058` and saves/restores `UnityEngine.Random.state` | **CONTRADICTED** |
| A-013 | `RGSaveManager` uses multiple save slots and a custom binary format | `docs/ARCHITECTURE.md:346-357` | Actual fields: `char_list`, `pet_list`, `hero_skin_list`, `AboGameData game_data`, `store_key`; Sirenix/Odin serialization is referenced by the assembly | **CONTRADICTED** (the native JSON schema is a new design, not a reconstruction) |
| A-014 | Phase 2 is complete and Gate B is met | `docs/PROJECT_STATE.md` (2026-10-30 section), `docs/PARITY.md:83` | No behavioural specs, no disassembly, no runtime experiments; `ARCHITECTURE.md` fields are explicitly "inferred" | **CONTRADICTED** |
| A-015 | Blocked: none | `docs/PROJECT_STATE.md` (2026-10-30 section) | Build fails; tests disabled; no observation oracle; no assets; no data | **CONTRADICTED** |
| A-016 | Tests: 0; framework ready | `docs/PROJECT_STATE.md` (2026-10-30 section) | `DOCTEST_CONFIG_DISABLE=0` disables all tests; enabling them fails to compile (`World.hpp:15`, `ecs_test.cpp:80`) | **CONTRADICTED** |
| A-017 | All tooling is operational | `docs/REVERSE_ENGINEERING.md:16-32`, `docs/PROJECT_STATE.md` (2026-10-30 section) | Tools are installed, but no Android device, emulator, Waydroid, QEMU, Frida, or Ghidra project | **CONTRADICTED** (as a statement about the whole workflow) |
| A-018 | IL2CPP metadata extraction is complete | `docs/PROJECT_STATE.md` (2026-10-30 section) | Full dump exists outside repo; in-repo `analysis/il2cpp_dump` is partial/truncated; extraction pending note remains in `analysis/ARTIFACT.md` | **CONTRADICTED** (as repository evidence) |
| A-019 | The IL2CPP class map is complete | `docs/PROJECT_STATE.md` (2026-10-30 section) | `analysis/class-map.md` explicitly says the IL2CPP class map is "to be populated" and contains only DEX/SDK classes | **CONTRADICTED** |
| A-020 | `analysis/SOURCE_MAP.md` is backed by `analysis/class-map.md` | `analysis/SOURCE_MAP.md` evidence column | `class-map.md` does not contain the cited `RG*` classes | **BROKEN CITATION** |
| A-021 | `analysis/il2cpp_dump` contains `il2cpp.h` and the full dump | `docs/PROJECT_STATE.md` (2026-10-30 section) | Directory contains only DLLs, a truncated `.il`, and a helper project; `il2cpp.h`, `dump.cs`, `script.json` are in `/home/jules/bin` | **BROKEN CITATION** |
| A-022 | 200+ `RG*` game classes were discovered | `docs/PROJECT_STATE.md` (2026-10-30 section), `docs/REVERSE_ENGINEERING.md` | `dump.cs` has 132 top-level `RG*` classes; the 200+ figure may include nested types but is not qualified | **PARTIAL** |
| A-023 | 25+ weapon types and 35+ projectile types | `docs/ARCHITECTURE.md`, `docs/ROADMAP.md` | Class names support the count; behaviour is not reconstructed | **PARTIAL** |
| A-024 | 8 MapManager variants exist | `docs/ARCHITECTURE.md` | Several variants exist (`MapManagerLevel`, `EndlessMapManager`, `MapManagerBR`, `MapManagerDefence`, `MapManagerSandbox`, `MapManagerSoccer`, etc.); exact count needs qualification | **PARTIAL** |
| A-025 | Reference uses deterministic RNG | `docs/ARCHITECTURE.md:409-418` | `RGRandom` wraps `UnityEngine.Random.State`; it is deterministic *given state/seed*, but the exact algorithm has not been identified | **PARTIAL** |
| A-026 | FixedUpdate runs at 50 Hz and this is CONFIRMED | `docs/ARCHITECTURE.md:117` | No evidence pointer or runtime measurement is present; the native timer uses 60 Hz instead | **UNSUPPORTED** |
| A-027 | Damage formula is `(atk - def) * crit * element` | `docs/REVERSE_ENGINEERING.md` RE-007 | Labelled HYPOTHESIS, but repeated in `PARITY.md` as a high-confidence plan; no disassembly or runtime evidence | **UNSUPPORTED** |
| A-028 | Room generation is BSP + graph + template placement | `docs/REVERSE_ENGINEERING.md` RE-008, `analysis/UNKNOWN_SYSTEMS.md` UNK-003 | Labelled HYPOTHESIS; no room-graph dump or algorithm reconstruction | **UNSUPPORTED** |
| A-029 | Enemy AI is coroutine state machines | `docs/ARCHITECTURE.md` | Method names include coroutine markers, but no state machine has been reconstructed | **PARTIAL** |
| A-030 | Save system is BinaryFormatter or custom binary | `docs/ARCHITECTURE.md:357`, `analysis/UNKNOWN_SYSTEMS.md` UNK-007 | Actual class uses `AboGameData` with Sirenix/Odin references; format still needs static analysis | **UNSUPPORTED** |
| A-031 | 40+ MB of AssetBundle content | `analysis/ARTIFACT.md`, `docs/REVERSE_ENGINEERING.md` | 39 MB bundles + 70 MB `bin/` assets; approximate but reasonable | **PARTIAL** |
| A-032 | The build is not yet configured | `docs/PROJECT_STATE.md` (2026-10-30 section) | `build/` is configured; `cmake --build build` fails | **PARTIAL** (outdated) |
| A-033 | "Dependencies: all decided" | `docs/PROJECT_STATE.md` (2026-10-30 section) | ADRs exist; `fmt` is used but not declared, and VMA is referenced in headers but not vendored/linked | **PARTIAL** |
| A-034 | Phase 3 is the next phase | `docs/PROJECT_STATE.md` (2026-10-30 section), `docs/ROADMAP.md` | Native code has already jumped into Phase 4 skeleton work; neither phase is complete | **PARTIAL** |
| A-035 | Test framework is doctest | `docs/TECHNOLOGY_DECISIONS.md` ADR-0008 | doctest is fetched and linked, but disabled in the build | **PARTIAL** |
| A-036 | XDG save/config paths are used | `docs/SAVE_SYSTEM.md`, `src/platform/filesystem/FileSystem.cpp` | Implementation uses XDG variables with `~/.local/share` fallback | **VERIFIED** (design, not reference parity) |
| A-037 | Wayland-only target | `NEMOTRON_MASTER_PROJECT.md`, `docs/PLATFORM.md` | SDL3 Wayland is available and session is Wayland; no Vulkan surface/swapchain is implemented yet | **PARTIAL** |
| A-038 | The project has a reproducible build | `NEMOTRON_MASTER_PROJECT.md` §43, `docs/ROADMAP.md` Phase 10 | No clean build; `src/` is untracked; `fmt`/Vulkan issues; missing install files | **CONTRADICTED** |
| A-039 | The repository is ready for another engineer | `NEMOTRON_MASTER_PROJECT.md` §75 | Missing `README.md`; source untracked; evidence generated outside repo; no regeneration scripts | **CONTRADICTED** |
| A-040 | Asset extraction is the next step | `analysis/resources.md` §7 | Still true, but the claim that tools must be acquired is outdated: UnityPy 1.25.4 works today | **PARTIAL** |

---

## Specific Evidence Details

### Unity version

```text
$ xxd -l 32 analysis/apktool_out/assets/AssetBundles/common
00000000: 556e 6974 7946 5300 0000 0006 352e 782e  UnityFS.....5.x.
00000010: 7800 3230 3138 2e34 2e31 3266 3100 0000  x.2018.4.12f1...

$ strings analysis/apktool_out/lib/arm64-v8a/libunity.so | grep 2018.4
2018.4.12f1
2018.4.12f1 (59ddc4c59b4f)
2018.4/release
```

### Metadata version

```text
$ xxd -l 8 analysis/apktool_out/assets/bin/Data/Managed/Metadata/global-metadata.dat
00000000: af1b b1fa 1800 0000
```

- magic: `0xFAB11BAF`
- version: `24` (`0x18`)

### RNG delegation

- `dump.cs` declares `RGRandom.Range(int, int)` at RVA `0xDD07E8`.
- `llvm-objdump` shows calls from `0xDD07E8` to:
  - `0x1AF8058` = `UnityEngine.Random$$Range`
  - `0x1AF7EC4` = `UnityEngine.Random$$get_state`
  - `0x1AF7F58` = `UnityEngine.Random$$set_state`
- `script.json` maps those addresses to those names.

This means the reference is using Unity's RNG state machine. Replacing it with PCG32 changes every random result.

### Test false green

```text
$ ./soul-knight-test --list-test-cases
(no output)

$ ctest
100% tests passed out of 6
Total Test time (real) = 0.07 sec
```

`CMakeLists.txt:382` defines `DOCTEST_CONFIG_DISABLE=0`. doctest checks `#ifdef`, so the tests are compiled out.

When the define is removed in a temporary copy:

```text
tests/unit/ecs_test.cpp:80: error: no matching function for call to
  soulknight::engine::World::AddComponent<...>(..., <brace-enclosed initializer list>)

src/engine/ecs/World.hpp:15: error: conversion from pointer type ... to arithmetic type ...
  in a constant expression
```

### Full metadata dump vs repository copy

| Location | Content |
|---|---|
| `/home/jules/bin/dump.cs` | 458,499 lines, 3,722 class declarations, 132 top-level `RG*` |
| `/home/jules/bin/script.json` | 103,252 method entries |
| `/home/jules/bin/il2cpp.h` | 34 MB |
| `/home/jules/bin/DummyDll/` | 75 assemblies |
| `analysis/il2cpp_dump/` | 4.3 MB DLL, 1.1 MB truncated IL, helper project; no `dump.cs`, `script.json`, or `il2cpp.h` |

---

## What Can Be Trusted Today

Use these as the current factual baseline:

1. The APK is the correct 2.6.8 artifact.
2. It is Unity 2018.4.12f1 IL2CPP, metadata v24.
3. Gameplay code is in `libil2cpp.so`; DEX is bootstrap/SDK code.
4. `dump.cs`/`script.json`/`il2cpp.h` exist locally and can drive disassembly.
5. AssetBundles are readable and exportable with UnityPy.
6. The current native build is not reproducible and the tests are disabled.
7. No behavioural specification has been validated against the reference.

Everything else in the architecture, combat, generation, save, and parity documents should be treated as **hypothesis until it has an evidence label and a test or disassembly citation**.

---

**Audit conclusion:** the documentation is not a reconstruction yet. It is a plausible design sketch built on a small number of verified facts and a large number of unmarked inferences. Correcting the labels and building the observation pipeline is the prerequisite for every later phase.
