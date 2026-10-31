# Soul Knight Linux — Project Diagnosis

**Date:** 2026-10-31  
**Scope:** Read-only investigation of the repository, build, tests, evidence trail, asset pipeline, and reverse-engineering workflow. No native source was changed.  
**Companion report:** `analysis/EVIDENCE_AUDIT.md`

---

## Executive Summary

The project is not blocked by one missing library or one broken file. It is stuck because the project has no working **evidence pipeline** for behaviour.

The repository contains a large amount of metadata about the Unity game (class names, fields, method names, RVAs), but almost no *behavioural* evidence. Behaviour can only come from two sources:

1. running the reference game and observing it, or
2. disassembling `libil2cpp.so` and reconstructing the logic.

Neither pipeline is operational. In its place, the documentation describes behaviour that was inferred from class names, and the native code is a skeleton built before the behaviour was known. The local feedback loop is also broken: the build fails, the tests are compiled out, and the assets/data needed to evaluate anything were never extracted.

**One-sentence diagnosis:** the project is attempting a clean-room native rewrite from an IL2CPP metadata dump while its only sources of truth — a runnable reference, a disassembly workflow, extracted assets, and working tests — are absent or falsely green.

---

## Status at a Glance

| Area | Documented status | Actual status |
|---|---|---|
| Build | "Not yet configured" / Phase 4 work in progress | Configured, but `cmake --build build` fails |
| Tests | "0 tests; framework ready" | Tests are compiled out and have never run; enabling them exposes compile errors |
| Native runtime | Phase 4 / Gate C | Window + input + audio wrappers only; no renderer, no simulation, no game loop |
| Phase 2 | "Complete" / Gate B | Metadata inventory only; behavioural reconstruction has not started |
| Assets | "Catalogued, ready for extraction" | No extraction run; `assets/converted` and `assets/generated` are empty |
| Data | Data-driven design documented | `data/` is empty; no weapon/enemy/room definitions |
| Reference runtime | "ADB available" | No device, emulator, Waydroid, QEMU, or Frida installed |
| Static RE | Ghidra installed | No Ghidra project; no method-body analysis performed |
| Source control | Phase 0/1/2 committed | `src/`, `tests/`, `external/` are untracked |
| Unity version | 2021.x, metadata v24.1 | **Unity 2018.4.12f1, metadata version 24** |
| RNG | PCG32 accepted for parity | Reference `RGRandom` delegates to `UnityEngine.Random` |

---

## Finding 1 — The build is red (P0)

### Reproduction

```bash
cmake --build build -j$(nproc)
```

### Observed failures

1. **Vendored Vulkan headers are incomplete.**
   `external/vulkan/include/vulkan/vulkan_core.h` references `StdVideoH264*`, `StdVideoH265*`, `StdVideoAV1*`, and `StdVideoVP9*` types. The corresponding files under `external/vulkan/include/vk_video/` are not real Khronos headers; they are 15–16 line stubs:

   ```c
   // Stub for vulkan_video_codec_h264std.h
   #pragma once
   #include <vulkan/vulkan_core.h>
   ```

   They do not define the enums and structs that `vulkan_core.h` uses. The result is dozens of `does not name a type` errors.

2. **The local Vulkan path disables the Vulkan loader.**
   `CMakeLists.txt` sets `VULKAN_LIBRARIES` to an empty string when the local headers are found, so the executable would not link `libvulkan.so` even if compilation succeeded.

3. **`SOUL_KNIGHT_COMMIT_HASH` is not defined.**
   `src/app/main.cpp:30` logs it, but no CMake definition provides it.

4. **`fmt` is used but not found or linked.**
   `Log.hpp` and `Log.cpp` use `fmt::format` / `fmt::print`, but the CMake build never calls `find_package(fmt)` or links `fmt::fmt`. On this system, formatting a `std::filesystem::path` then fails because the formatter is incomplete.

5. **SDL3 Vulkan API mismatch.**
   `Window.cpp` calls `SDL_Vulkan_CreateSurface` without including `<SDL3/SDL_vulkan.h>`, using the SDL 3.2+ signature that takes a `VkAllocationCallbacks*` before the surface handle.

### Important distinction

This is not an architectural dead end. In a temporary copy under `/tmp/opencode/sk-src`, the build succeeded after only these changes:

- use system Vulkan headers,
- link `fmt`,
- define `SOUL_KNIGHT_COMMIT_HASH`,
- include `<SDL3/SDL_vulkan.h>` and pass `nullptr` as the allocator,
- call `.string()` on the filesystem paths passed to `fmt`.

The current tree simply cannot produce a binary. Until that is fixed, no amount of design work can be validated.

### Additional build-system defects

- `CMakeLists.txt` installs `config/default.toml`, but there is no `config/` directory.
- The root `README.md` required by the master specification does not exist.
- `external/vulkan/` is a 22 MB duplicate of system headers with fabricated stubs; the system has `vulkan-headers 1:1.4.363.0-1` installed. Vendoring is unnecessary here.

---

## Finding 2 — Tests are false green (P0)

### The tests are disabled

`CMakeLists.txt:382` defines:

```cmake
DOCTEST_CONFIG_DISABLE=0
```

doctest uses `#ifdef DOCTEST_CONFIG_DISABLE`, not a value check. Defining it to `0` still disables every test. Consequences:

```text
$ ./soul-knight-test --list-test-cases
(no output)

$ ctest
100% tests passed out of 6
Total Test time (real) = 0.07 sec
```

All six CTest entries pass because there are zero tests to run.

### Enabling them exposes compile errors

When the define is removed in a temporary copy, the test target fails to compile:

- `src/engine/ecs/World.hpp:15` uses `reinterpret_cast` inside a `constexpr` initializer, which is illegal:
  ```cpp
  static constexpr uintptr_t value = reinterpret_cast<uintptr_t>(&TypeId<T>::value);
  ```
- `tests/unit/ecs_test.cpp:80-81` calls `World::AddComponent<T>(entity, { ... })`, but only `AddComponent<T>(entity)` exists.

These errors have never been seen because the tests have never compiled.

### The CTest filters also select nothing

The tests use names such as `"Entity creation and validation"` and `"PCG32 basic"`. The CTest entries filter with `--test-case=*unit*`, `*integration*`, `*gameplay*`, etc. Even with tests enabled, those filters match zero test cases. The suite needs tags or a naming convention that matches the filters.

### Why this matters

The project has no automated way to detect regressions. "Tests: 0" in `docs/PROJECT_STATE.md` understates the problem: the test framework is wired in a way that guarantees a green result.

---

## Finding 3 — The native runtime is a skeleton, not Gate C (P0)

The code that exists is mostly interface scaffolding:

| File | Actual state |
|---|---|
| `src/app/main.cpp` | Creates SDL window/input/audio, then loops with `// TODO: Game simulation` and `// TODO: Render` |
| `src/engine/rendering/VulkanContext.hpp` | Declares `Initialize`, `Shutdown`, `BeginFrame`, etc.; no definitions exist anywhere |
| `src/engine/rendering/SpriteBatcher.hpp` | Header-only declaration; uses `VmaAllocator` without including VMA; no definitions |
| `src/platform/wayland/Window.cpp` | `Present()` is a no-op; no swapchain exists |
| `src/platform/audio/AudioEngine.cpp` | Only `SoundHandle::Init` is defined; `Play`, `Stop`, `SetVolume`, etc. are declared but undefined |
| `src/game/player/PlayerController.cpp` | Movement, damage, and death are `TODO` placeholders |

`CMakeLists.txt` does not compile the rendering implementation files at all because they do not exist. The renderer described in `docs/RENDERING.md` is a design document, not code.

Therefore Phase 4 / Gate C ("native app launches, renders, inputs, 60 FPS clear screen") has not been reached. The current program can open a window and loop, but it draws nothing and simulates nothing.

There is also a fidelity contradiction: `docs/ARCHITECTURE.md:117` says Unity `FixedUpdate()` runs at 50 Hz and calls it **CONFIRMED**, while the native design and `FrameTimer` use a 60 Hz fixed simulation. This should be resolved from evidence before movement/combat work.

---

## Finding 4 — Phase 3 has not actually started (P0)

### The full metadata dump exists outside the repository

A complete Il2CppDumper output exists in `/home/jules/bin/`:

| Artifact | Size / count |
|---|---|
| `dump.cs` | 458,499 lines |
| `DummyDll/Assembly-CSharp.dll` | 4.3 MB |
| Dummy assemblies | 75 files |
| `script.json` | 42 MB, 103,252 script methods |
| `il2cpp.h` | 34 MB |
| `stringliteral.json` | 1.2 MB |

The repository's `analysis/il2cpp_dump/` is not that output. It contains only:

- `Assembly-CSharp.dll`,
- `Assembly-CSharp-firstpass.dll`,
- a truncated `Assembly-CSharp.il` (22,587 lines, ends mid-method, only 85 class declarations),
- a small .NET helper project.

`analysis/il2cpp_dump` is also excluded by `.gitignore`, as are the apktool and jadx outputs. The evidence needed to reproduce the architecture is therefore outside version control and not described by a regeneration script.

### The documentation is inference presented as reconstruction

The actual `dump.cs` shows fields and methods, but no method bodies. That is expected: Il2CppDumper gives structure, not behaviour. The documents repeatedly turn structure into guesses and then label the result with high confidence.

Examples:

| Claim in docs | Actual metadata |
|---|---|
| `RGController` has `health`, `maxHealth`, `armor`, `energy`, `moveSpeed`, `dashSpeed`, `currentWeapon` (`ARCHITECTURE.md:142-148`, explicitly labelled "inferred") | `RGController` has `role_attribute`, `skills`, `movementInput`, `rg_random`, `skill_flags`, etc. Health/armor/energy live in `RoleAttribute` / `RoleAttributePlayer` |
| `RGGameInfo` is runtime game data / progression (`ARCHITECTURE.md:37`) | Actual fields are `sample_random_seed`, `map_random_seed`, `bundleLoaded`, `checkingNextScene`; most methods are network seed/position messages |
| `RGSaveManager` has multiple slots and a custom binary save (`ARCHITECTURE.md:346-357`) | Actual fields are `char_list`, `pet_list`, `hero_skin_list`, `AboGameData game_data`, `store_key`; the game uses Sirenix/Odin serialization, not the invented JSON schema |
| `RGWeapon` fields match the guessed list in `ARCHITECTURE.md:199-204` | Actual fields include `bulletsInfo`, `weapon_type`, `deviation`, `consume`, `weapon_speed`, `atk_move_speed`, `weapon_item`, etc. The guessed field set does not match |
| RNG can be replaced with PCG32 (`TECHNOLOGY_DECISIONS.md` ADR-0014) | `RGRandom` stores `seed_state`, `seed`, and `UnityEngine.Random.State this_state`; `Range()` saves/restores Unity's RNG state and calls `UnityEngine.Random.Range` |

The RNG point is verifiable in the binary. `dump.cs` places `RGRandom.Range(int,int)` at RVA `0xDD07E8`. Disassembly shows calls to:

- `0x1AF8058` → `UnityEngine.Random$$Range`,
- `0x1AF7EC4` → `UnityEngine.Random$$get_state`,
- `0x1AF7F58` → `UnityEngine.Random$$set_state`.

Address-to-name mapping comes from `script.json`. So the reference RNG is Unity's `Random`, not a custom PCG32. A PCG32 implementation cannot produce the same dungeon or loot sequence. This alone invalidates the current parity plan.

### Gate B is not met

The master specification defines Gate B as "Engine, loop, renderer, and major subsystems mapped." The repository has class names and guessed fields, not mapped behaviour. `analysis/class-map.md` explicitly says the IL2CPP class map is "to be populated" and contains only DEX/SDK classes. Yet `analysis/SOURCE_MAP.md` cites `analysis/class-map.md` as evidence for `RG*` classes that are not in that file.

This is circular evidence, not reconstruction.

---

## Finding 5 — There is no runtime oracle (P0)

The behavioural plan depends on runtime experiments:

- `UNKNOWN_SYSTEMS.md` says to "hook `DamageCarrier.ApplyDamage`", "log AI state transitions", "spawn each projectile type", etc.
- `ROADMAP.md` Phase 3 assumes behaviour can be observed and specified.

The environment has:

```text
adb devices          → no devices
emulator / sdkmanager → not installed
waydroid / anbox     → not installed
qemu-aarch64         → not installed
frida                → not installed
Ghidra               → installed, but no project
```

So none of the planned "Next Experiments" can be executed. This is the direct reason Phase 3 cannot progress: the project has no way to observe the reference.

A static alternative is available and untapped:

- Ghidra 12.2 is installed at `/home/jules/bin/ghidra_12.2_PUBLIC`.
- `script.json` maps 103,252 method addresses to names.
- `il2cpp.h` provides the IL2CPP type layout.
- `llvm-objdump` can already disassemble the ARM64 binary; the `RGRandom` check above was done this way.

The project does not need a device for every question, but it does need a deliberate static-analysis workflow (Ghidra project, scripted symbol application, subsystem-by-subsystem documentation).

---

## Finding 6 — The asset pipeline was never started (P0)

The APK contains the content:

- `assets/AssetBundles/` = 39 MB,
- `assets/bin/` = 70 MB,
- 216 room bundles,
- 4 resource bundles.

The repository's `assets/converted/` and `assets/generated/` are empty. `data/` is empty. `analysis/resources.md` lists extraction tools as "need to acquire" and marks every extraction step pending.

However, extraction is technically easy. In this investigation, `UnityPy 1.25.4` was installed in a temporary virtual environment and read the bundles without custom code:

| Bundle | Objects found |
|---|---:|
| `common` | 37,136 (7,707 GameObject, 6,578 MonoBehaviour, 4,181 Sprite, 163 Texture2D, 216 AudioClip) |
| `level-objects` | 35,956 |
| `hall` | 1,524 |
| `scene_game` | 1,499 |
| `room/r1_1` | 560 |
| `res/charactor_common` | 172 |

Sprites can be exported to PNG (`enemy01_3`, `enemy04_4`, `npc02_1` were exported in the test). So the asset pipeline is not blocked by tooling; it is simply unstarted.

Two implications:

1. A playable vertical slice can use real sprites/audio much sooner than the roadmap assumes.
2. The exact Unity version matters for tooling: the bundles are **Unity 2018.4.12f1**, not Unity 2021.x. The current documentation has the wrong target version.

---

## Finding 7 — Unity version and asset-format claims are wrong (P1)

The documentation says Unity 2021.x and metadata v24.1 in several places:

- `analysis/ARTIFACT.md:33`
- `docs/PROJECT_STATE.md` (2026-10-30 section)
- `docs/ARCHITECTURE.md:11`
- `analysis/resources.md` ("Unity 2021 format")

Direct evidence says otherwise:

| Source | Value |
|---|---|
| `assets/AssetBundles/common`, offset 0x14 | `2018.4.12f1` |
| `strings libunity.so` | `2018.4.12f1 (59ddc4c59b4f)`, `2018.4/release` |
| `strings libil2cpp.so` | `C:\Unitys\2018.4.12f1\Editor\Data\il2cpp\...` |
| `global-metadata.dat`, bytes 4–7 | metadata version `24` |

This is not cosmetic. Unity 2018.4 uses different serialization, animation, texture, and shader data than 2021.x. Asset extraction, animation reconstruction, and shader replacement all depend on this. The wrong version also explains why the "metadata v24.1" claim looked plausible: the file is v24, but the "1" was not verified.

---

## Finding 8 — Source control and reproducibility are broken (P1)

Current `git status`:

```text
 M CMakeLists.txt
?? external/
?? src/
?? tests/
```

The last four commits are documentation and build-system commits. None of the native source, tests, or vendored headers are committed. A fresh clone cannot build the project because the code is not in Git.

The evidence trail has the same problem:

- `analysis/apktool_out/` is gitignored.
- `analysis/jadx_out/` is gitignored.
- `analysis/il2cpp_dump/` is gitignored.
- the complete Il2CppDumper output is outside the repository,
- there is no script that regenerates the dump or verifies its hashes,
- there is no root `README.md`,
- `CMakeLists.txt` installs files that do not exist.

The master specification says "the final repository should be understandable to another engineer who has never spoken to Nemotron." At present, it is not reproducible even on this machine without the out-of-repo `/home/jules/bin` state.

---

## Finding 9 — Scope and sequencing are the deeper cause (P1)

The metadata shows the size of the problem:

- 3,722 classes,
- 132 top-level `RG*` classes (more including nested types),
- 103,252 script methods,
- 20 numbered enemy AIs plus specials,
- 216 room bundles,
- roughly 150 weapons / 80 enemies / 200 items / 16 bosses estimated by the roadmap.

The roadmap allocates:

- 3 weeks for Phase 3 behavioural specs,
- 3 weeks for native runtime,
- 9 weeks for all core systems,
- 6 weeks for content parity.

That estimate is not credible for a single developer starting from metadata only. More importantly, the sequence is backwards relative to the project's own rules:

- Master spec §3.2: "Reconstruct before reimplementing."
- Master spec §72: "Do NOT mass-implement."
- Actual sequence: wrote 6,220 lines of design docs, then a broad skeleton, before behaviour was extracted.

The result is a large appearance of progress with very little engineering truth.

---

## What Is Actually Salvageable

The situation is recoverable. The following are real assets:

- The APK hash matches `analysis/original-apk.sha256` (`8faff890...0137ea`).
- The game is confirmed Unity 2018.4.12f1 IL2CPP.
- The full metadata dump is present on this machine.
- The ARM64 binary is complete and `script.json`/`il2cpp.h` can drive Ghidra.
- SDL3 3.4.18, Vulkan 1.4.363, Wayland, GCC, CMake, Ninja, and Git all work.
- UnityPy can read and export the AssetBundles.
- `llvm-objdump` can disassemble method bodies now.

The project does not need a new engine, a new language, or a new platform decision. It needs a working feedback loop and an honest evidence trail.

---

## Recommended Recovery Plan

### P0 — Restore the feedback loop (next session)

1. **Fix the build.**
   - Remove `external/vulkan/` and use system `vulkan-headers`.
   - Add `find_package(fmt REQUIRED)` and link `fmt::fmt`.
   - Define `SOUL_KNIGHT_COMMIT_HASH`.
   - Include `<SDL3/SDL_vulkan.h>` and use the SDL 3.2+ signature.
   - Fix the `std::filesystem::path` formatting.
   - Re-run a clean configure/build; do not trust the existing `build/` cache.

2. **Fix the tests.**
   - Remove `DOCTEST_CONFIG_DISABLE`.
   - Fix `World::TypeId` and the `AddComponent` API.
   - Give tests tags or names that match the CTest filters; add one integration smoke test that actually launches the platform layer.
   - Verify `--list-test-cases` is non-empty and CTest fails when a test is intentionally broken.

3. **Make the runtime visible.**
   - Implement the minimum Vulkan path: instance, device, swapchain, clear screen, present.
   - Or, if Vulkan bootstrap is the current risk, use `SDL_Renderer` temporarily for a single clear-screen smoke test, then return to the Vulkan ADR. The important thing is a visible frame and a timed loop, not renderer completeness.

### P0 — Choose and build the evidence pipeline

Pick one, explicitly:

**Option A — Physical ARM Android device (best fidelity).**  
Use ADB + Frida or an IL2CPP inspector to run the reference, log RNG/damage/AI/room generation, and record reproducible experiments. The final product still has no Android runtime; this is analysis tooling only.

**Option B — Emulator/Waydroid with ARM translation (usable, slower).**  
Same observation goal, but requires setup and may be too slow for frame-level work. Document it as an analysis environment, not a runtime target.

**Option C — Ghidra static analysis (no device required).**  
Import `libil2cpp.so`, apply `script.json`/`il2cpp.h`, and reconstruct one subsystem at a time. This is slower per question but works today. Start with:
   - `RGRandom` / Unity `Random` state,
   - `DamageCarrier` / `RoleAttribute` damage path,
   - `MapManagerLevel.CreateMap` room graph,
   - one `EnemyAI##` state machine.

Do not claim a subsystem is understood until it has either a runtime observation or a disassembly-backed behavioural note with addresses.

### P0 — Start the asset pipeline

1. Use UnityPy (or AssetRipper if its licence is acceptable) as a local analysis tool.
2. Export one room (`room/r1_1`), one character (`res/charactor_common`), common UI sprites, and a few audio clips.
3. Record a manifest with source bundle, object name, type, and hash.
4. Keep extracted assets out of Git per the legal boundary; commit only the pipeline script and the manifest.
5. Validate that exported sprites/audio open correctly before building any renderer around them.

### P1 — Reset the documentation to evidence labels

1. Replace speculative fields/mechanics in `ARCHITECTURE.md` with either:
   - actual `dump.cs` fields/methods, or
   - an explicit `HYPOTHESIS` with the experiment that would confirm it.
2. Correct Unity 2018.4.12f1 / metadata v24 everywhere.
3. Replace the invented save schema with the actual `RGSaveManager`/Odin structure, or explicitly mark the native format as a new design rather than a reconstruction.
4. Replace the PCG32 ADR with a Unity-2018-compatible RNG plan. If exact parity is required, PCG32 cannot be used.
5. Mark Gate B as **not met** until behaviour is mapped.

### P1 — Build a vertical slice, not the whole game

The first playable should be deliberately narrow:

- one room (`r1_1`),
- one character (Knight),
- idle/run/hit/death,
- one ranged weapon,
- one enemy AI,
- collision, damage, death, restart,
- HUD with health/armor/energy,
- real sprites from the extracted bundle,
- one music track and a few SFX.

Only after that slice runs should content breadth be attempted. This is the fastest way to validate the architecture and discover what the game actually needs.

### P1 — Commit the native baseline

- Commit `src/`, `tests/`, build fixes, and the reports.
- Add a `tools/analyze-apk.sh`/regeneration script and hashes for derived evidence.
- Add the missing `README.md` and `config/default.toml`.
- Do not commit APKs, extracted proprietary assets, or the full recovered dump.

---

## Decisions Required

| Decision | Recommendation |
|---|---|
| Reference oracle | Physical ARM device if available; otherwise Ghidra static workflow first |
| Scope | Vertical slice before content parity |
| RNG | Unity 2018.4 `Random`-compatible implementation if parity is required; PCG32 only if parity is explicitly abandoned |
| Renderer bootstrap | Keep Vulkan as the target, but permit a temporary SDL clear-screen smoke test to unblock the loop |
| Asset tool | UnityPy for immediate extraction; AssetRipper only if its licence and output quality justify it |
| Documentation | Freeze speculative docs until each claim has an evidence label |

---

## Appendix — Key Evidence Paths

| Evidence | Path |
|---|---|
| APK hash | `analysis/original-apk.sha256` |
| Full metadata dump (outside repo) | `/home/jules/bin/dump.cs`, `/home/jules/bin/script.json`, `/home/jules/bin/il2cpp.h`, `/home/jules/bin/DummyDll/` |
| Partial in-repo dump | `analysis/il2cpp_dump/` |
| Unity version | `analysis/apktool_out/lib/arm64-v8a/libunity.so` strings; bundle header |
| Metadata version | `analysis/apktool_out/assets/bin/Data/Managed/Metadata/global-metadata.dat` bytes 4–7 |
| RNG disassembly | `libil2cpp.so` RVA `0xDD07E8` → `UnityEngine.Random` at `0x1AF8058` |
| Asset bundles | `analysis/apktool_out/assets/AssetBundles/` |
| Build errors | `cmake --build build` output in this session |
| Test false green | `CMakeLists.txt:382`, `./soul-knight-test --list-test-cases` |
| Native skeleton | `src/app/main.cpp:107-115`, `src/engine/rendering/*.hpp` |

---

**Conclusion:** the project is stuck at the transition from metadata to behaviour. The immediate fixes are mechanical (build, tests, Unity version, assets), but the strategic fix is to stop treating class names as behavioural evidence and to establish one real observation/disassembly pipeline before writing more game code.
