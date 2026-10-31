# Repository Integrity Audit — P0/P1 Repair Pass

**Date:** 2026-10-31
**Baseline before pass:** `4f61c3d` (working tree clean)
**Scope:** documentation truth, source stubs, build/test reproducibility, Git hygiene,
evidence pipelines, RGRandom evidence, damage evidence, asset extraction, vertical-slice
and roadmap gates. No gameplay, reverse-engineering expansion, vertical-slice, or
renderer implementation was started.

Run `git log -1 --format='%h %s'` for the commit that contains this report.

> **Note:** build/test counts and Phase 4 status in this report describe the
> P0/P1 pass at baseline `4f61c3d`. The current baseline is 35 cases / 1367
> assertions, and the Phase 4 native foundation is implemented (Gate C PARTIAL).
> See `docs/TESTING.md` and `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`.

---

## 1. Problems found

### Documentation truth

- `docs/PROJECT_STATE.md` still said Phase 3 had not started and repeated pre-repair
  build claims.
- `docs/PARITY.md` presented inferred class/field names as high-confidence parity and
  left Gate B/Gate C effectively green.
- `docs/ROADMAP.md` said Phase 2 was complete, Phase 3 had not started, and Phase 4
  code existed as a skeleton; gate status contradicted the tree.
- `docs/ARCHITECTURE.md` was labelled a complete reconstruction although it is a class
  inventory plus hypotheses; it carried the wrong engine version and an unsupported
  50 Hz claim at that time. The 50 Hz figure is now confirmed from the serialized
  `TimeManager`; see `analysis/re/timing.md`.
- `docs/REVERSE_ENGINEERING.md` repeated the original "Phase 2 complete / Unity 2021"
  status and stale tool versions.
- `CHANGELOG.md` mixed historical "build broken / tests disabled / no extraction"
  entries with newer repaired-state entries without a clear chronology.
- `docs/CONTRIBUTING.md` documented `-march=native`, an X11 dependency, a stale source
  tree (Vulkan/SpriteBatcher/etc.), and test categories that do not exist.
- `docs/PLATFORM.md`, `docs/RENDERING.md`, `docs/INPUT.md`, `docs/AUDIO.md`, and
  `docs/SAVE_SYSTEM.md` said "DESIGN COMPLETE — ready for implementation" and showed
  snippets as if implemented, including an outdated SDL3 Vulkan call and a thrown
  exception under `-fno-exceptions`.
- `analysis/PROJECT_DIAGNOSIS.md` and `analysis/EVIDENCE_AUDIT.md` were historical but
  not visibly marked as pre-repair history.
- `analysis/resources.md`, `analysis/native-libraries.md`, and `analysis/APK_INVENTORY.md`
  contained stale "ready for next phase" statements and unsupported "2021 era" labels.

### Source and build

- Fabricated/proxy Vulkan headers (`src/platform/wayland/minimal_vulkan.h`,
  `VulkanWrapper.hpp`) and unimplemented renderer declarations
  (`src/engine/rendering/VulkanContext.hpp`, `SpriteBatcher.hpp`) were still in the
  tree and CMake sources.
- `src/platform/audio/SoundHandle.hpp` was a declaration-only stub.
- The SDL event watch was added with one lambda and removed with a different lambda,
  so the watch could not be removed correctly.
- `src/app/main.cpp` forced no video driver and did not reject an X11/XWayland fallback.
- The fixed-step loop called `ShouldSimulate()` but never `ConsumeSimStep()`. Any frame
  over 16.7 ms would hang the process.
- `src/engine/ecs/World.hpp` exposed an unused iterator that treated dense component
  indices as entity indices; it would return null/invalid pairs for sparse sets.
- `FileSystem` used the process working directory instead of `/proc/self/exe` for
  built-in asset lookup.
- `AudioEngine` declared `SoundHandle` methods without definitions and exposed a
  bus-specific `StopAll` API that was not implemented.
- `InputManager` duplicated scancodes in an `unordered_map` (first binding silently
  won) and exposed TOML mapping I/O that returned false without saying so.
- `FrameTimer` used `high_resolution_clock` for simulation timing.
- `CMakeLists.txt` used `-march=native`, DEB packaging assumptions, unused
  VMA/miniaudio global defines, and referenced an undefined `COMMON_WARNING_FLAGS`.
- Player/character placeholders contained fabricated non-zero stats that could be
  mistaken for reference values.
- `src/app/main.cpp` still contained `TODO` markers for missing simulation/render
  systems without explicitly saying they are not implemented.

### Evidence pipeline and data

- The asset manifest recorded entries but not the reference APK hash, UnityPy version,
  bundle root, or applied profiles, and output names depended on pre-existing files,
  so reruns were not path-stable.
- RGRandom's `seed_state` 1/2 and map-length branch were open; `RGController.rg_random`
  and the map-seed path needed final confirmation.
- Damage's projectile → target bridge was described as entirely unknown; battle-factor
  and buff enum identities were unresolved in the evidence document.
- `docs/VERTICAL_SLICE.md` gate rows lacked a uniform evidence/missing-evidence/
  responsible-subsystem structure and used an incorrect c01 entry count.

---

## 2. Problems fixed

### Documentation

- Rewrote `docs/PROJECT_STATE.md`, `docs/PARITY.md`, and `docs/ROADMAP.md` as current
  truth with explicit phase/gate status.
- Added correction banners and hypothesis labels to `docs/ARCHITECTURE.md`,
  `docs/REVERSE_ENGINEERING.md`, and the design documents.
- Marked `analysis/PROJECT_DIAGNOSIS.md` and `analysis/EVIDENCE_AUDIT.md` as historical
  pre-repair reports.
- Rebuilt the changelog chronology: historical 2026-10-30 entries are retained, but
  current repaired state is unambiguous.
- Fixed `docs/CONTRIBUTING.md`: removed `-march=native` and `libx11`, documented the
  actual source tree, marked nonexistent test categories as planned, and corrected
  RNG/tick-rate wording.
- Added explicit "design proposal only / not reference evidence" banners to
  `docs/PLATFORM.md`, `docs/RENDERING.md`, `docs/INPUT.md`, `docs/AUDIO.md`, and
  `docs/SAVE_SYSTEM.md`; fixed the SDL3 Vulkan surface signature and removed the
  exception-throwing example.
- Updated `analysis/resources.md`, `analysis/native-libraries.md`, and
  `analysis/APK_INVENTORY.md` to current status and removed unsupported 2021 labels.
- Updated `docs/BUGS.md` with the resolved false-green tests, window-watch, Wayland,
  accumulator-hang, and ECS-iterator defects.
- Updated `analysis/UNKNOWN_SYSTEMS.md` and `analysis/re/rgrandom.md` with the resolved
  map-seed path, `RGController` seeding, and the remaining RGRandom questions.
- Updated `analysis/re/damage.md` with the resolved enum identities, the box target
  bridge, the remaining controller bridge, and the narrowed `CanDie` candidates.
- Updated `docs/VERTICAL_SLICE.md`: every gate now has status, evidence document,
  exact missing evidence, and responsible subsystem; corrected asset counts and gate
  wording.
- Updated ADR-0016/ADR-0017 and `docs/TECHNOLOGY_DECISIONS.md` so they do not imply
  gameplay integration or a future Android/emulator oracle.

### Source and build

- Deleted the fabricated Vulkan headers, unimplemented renderer declarations, and the
  declaration-only `SoundHandle.hpp`.
- Switched to system Vulkan headers and loader; removed `-march=native`, VMA/miniaudio
  implementation defines, DEB packaging metadata, and the undefined warning variable.
- Fixed the SDL event watch add/remove callback identity and SDL3 Vulkan surface call.
- Forced `SDL_HINT_VIDEO_DRIVER=wayland` and added a fail-fast startup check that
  rejects any active driver other than `wayland`.
- Fixed the fixed-step loop by calling `ConsumeSimStep()` and removed the unused
  interpolation variable.
- Removed the broken unused `World` iterator and its dead type-erased `Size()` accessor.
- Fixed `/proc/self/exe` resolution; added file-read/write error and size checks.
- Implemented `SoundHandle` play/stop/query/volume/pitch/pan/loop/seek/uninit methods;
  corrected `StopAll` to an honest engine-wide API; added audio config validation.
- Removed duplicate key bindings and marked TOML mapping load/save as unimplemented.
- Switched frame timing to `steady_clock`.
- Zeroed and clearly labelled the unimplemented player/character stats.
- Replaced misleading `TODO` markers with explicit `NOT IMPLEMENTED` statements.
- Suppressed the project warning set only around the vendored miniaudio header; the
  final Debug and RelWithDebInfo builds are warning-free.

### Asset manifest

- Manifest v2 now records:
  - reference APK SHA-256,
  - UnityPy 1.25.4,
  - bundle root,
  - applied profiles,
  - source bundle path and SHA-256,
  - object path ID, Unity type, name, output path, status, and note.
- Output filenames always include the object path ID, making reruns path-stable.
- Verified from a deleted venv and deleted `assets/generated/first-pass/`: 284 entries,
  0 warnings.

---

## 3. Intentionally unresolved problems

### 3.1 Projectile → controller damage bridge — UNRESOLVED

**Where:** `RGBulletTrigger.OnTriggerEnter2D` (`0x1160324`), enemy/player branch at
`0x1160F78`; shared `GameObject.GetComponent<T>` at `0xFF8A84`, GOT slot `0x309A4E8`;
virtual call through vtable `+0x2C8`.

**What is known:**

- The box branch is HIGH CONFIDENCE: `GetComponent<T>` through GOT slot `0x31003B0`
  followed by virtual `Hit(damage, source_object, camp)` at vtable `+0x168` matches
  `RGBox.Hit` (`0x11675F8`), and the same GOT slot is used by known box-hit paths.
- The enemy/player branch calls a virtual method at vtable `+0x2C8` with
  `(damage, source_object)`.
- No direct `bl` to `RGController.GetHurt` (`0x11795CC`) or
  `RGEController.GetHurt` (`0x1108E60`) exists in `0x1160324..0x1161A80`.

**What is unknown:** the concrete generic `T` for GOT slot `0x309A4E8` and the owner of
vtable `+0x2C8`. The call site does not encode the type; resolving it needs type-info
propagation in Ghidra or an approved native observation trace. Do **not** infer it from
field names.

**Evidence:** `analysis/re/damage.md` §6.3, §7.

### 3.2 Critical hits and elements — UNRESOLVED

`DamageInfo.critic` and `emDamageType` exist in metadata, but no read/application path
has been reconstructed. Exact methods are not yet identified; do not implement a crit
formula or status formula from names.

**Evidence:** `analysis/re/damage.md` §8.

### 3.3 `CanDie()` generic singleton — UNRESOLVED

**Where:** `RoleAttributePlayer.CanDie` (`0xD40718`). It resolves one generic
`Singleton<T>.get_Inst` (`0x1AB448C`) and compares field `+0x1C` to `-1`/`-3`, then
checks `BattleData.IsSandboxEditing`.

**What is known:** the GOT slots used (`0x030A2D88`, `0x030F2358`) are shared generic
helpers and do not identify `T`. The only `Singleton<T>` classes with an `int` at
`+0x1C` are `RGGameInfo` (`map_random_seed`) and `RGGameProcess` (`this_index`).

**What is unknown:** which `T` is instantiated, and therefore which field is compared.
The former claim that it is specifically `RGGameInfo.map_random_seed` is not supported.

**Evidence:** `analysis/re/damage.md` §4.6.

### 3.4 RGRandom `seed_state` 1/2 and map-length branch — UNRESOLVED

**Where:** `RGRandom.seed_state` (`+0x10`), `SetRandomSeed` (`0xDE9370`),
`MapManagerLevel.SetRGRandomSeed`, `emBattleFactor.LongMap` (10).

**What is known:** constructor leaves `seed_state = 0`; `SetRandomSeed` writes `3`
(`GET_SEED`); `MapManager.Start` loads `RGGameInfo.map_random_seed` (`+0x1C`) and calls
virtual `SetRGRandomSeed` (`+0x1E8`); `RGController.AwakeController` (`0x1177318`)
seeds `rg_random` (`+0x130`) with `sample_random_seed + skillIndex`.

**What is unknown:** no static write of `seed_state` 1 (`C_READY`) or 2
(`S_SEND_SEED`) was found. The exact `Range(0,100)`/`LongMap` condition set in
`MapManagerLevel.SetRGRandomSeed` is not fully decoded. Reference float/vector parity is
also unverified because no runtime reference trace exists.

**Evidence:** `analysis/re/rgrandom.md` §7.

### 3.5 Asset identities and conversion — UNRESOLVED

- `c01`, `enemy01`, `e_slime01`, and the generic weapon are **candidates**, not
  proven identities.
- No serialized character/enemy/weapon definition has been decoded to map them.
- Extraction is implemented; conversion (KTX2/OGG/atlas/JSON data) is not.

**Evidence:** `analysis/assets/extraction_manifest.json`,
`analysis/assets/README.md`, `docs/VERTICAL_SLICE.md`.

### 3.6 Renderer, tick rate, and gameplay gates — NOT MET

- **Superseded (2026-10-31 Phase 4):** a Vulkan instance/device/swapchain and batched sprite renderer now exist. Gate C is **PARTIAL**, not NOT MET. This bullet records the earlier P0/P1 state.
- **Superseded (2026-10-31 late pass):** the serialized Unity `TimeManager` value is
  now decoded as `Fixed_Timestep = 0.019999999552965164` (50 Hz), and the native
  `FrameTimer` has been corrected to match; 60 Hz remains the render target. See
  `analysis/re/timing.md`.
- Movement, collision, enemy AI, weapons, projectiles, rooms, items, UI, audio triggers,
  save/load, and progression are not reconstructed. The vertical slice remains blocked.

### 3.7 Environment limitation

No runtime observation oracle is permitted or available: no Android runtime, Waydroid,
emulator, Wine, or X11/XWayland. Static analysis is the current evidence source. The
full Il2CppDumper output remains outside Git at `/home/jules/bin`; the in-repo
`analysis/il2cpp_dump/` is partial and non-authoritative.

---

## 4. Build and test verification

All commands were run after removing `build/` for each configuration.

### Debug

```bash
./tools/configure.sh Debug
./tools/build.sh Debug
./tools/test.sh
ctest --test-dir build --output-on-failure
```

Results:

- configure: PASS
- build: PASS, **0 compiler warnings**
- doctest: **22 test cases, 1273 assertions, all passing**
- CTest: 1/1 passed

### Negative control

The `Vec2 addition` assertion in `tests/unit/math_test.cpp` was changed from
`Vec2{4.0f, 6.0f}` to `Vec2{4.0f, 7.0f}`, `soul-knight-test` was rebuilt, and CTest was
run:

- CTest exit: **8**
- 22 cases: 21 passed, 1 failed
- 1273 assertions: 1272 passed, 1 failed

The file was restored byte-for-byte (`cmp` verified), rebuilt, and CTest passed again.

### RelWithDebInfo

```bash
rm -rf build
./tools/configure.sh RelWithDebInfo
./tools/build.sh RelWithDebInfo
./tools/test.sh RelWithDebInfo
ctest --test-dir build --output-on-failure
```

Results:

- configure: PASS
- build: PASS, **0 compiler warnings**
- doctest: **22 test cases, 1273 assertions, all passing**
- CTest: 1/1 passed

**Test count did not change.** No test was added, removed, weakened, or skipped during
the pass. The ECS iterator removal did not affect the test count because that API had no
test and no caller.

### Git whitespace

`git diff --check` exits 0.

---

## 5. Evidence pipeline verification

### IL2CPP metadata → RVA → disassembly → xrefs

Commands exercised against the real `libil2cpp.so`:

```bash
tools/re/il2cpp_re.py lookup RGRandom
tools/re/il2cpp_re.py class RGRandom
tools/re/il2cpp_re.py method RGRandom.Range
tools/re/il2cpp_re.py address 0xDD07E8
tools/re/il2cpp_re.py disasm --address 0xDD07E8 --size 0x60
tools/re/il2cpp_re.py xref --address 0xDD07E8
tools/re/il2cpp_re.py export-symbols --out /tmp/opencode/il2cpp_symbols.tsv
```

Observed:

- `RGRandom` class: 3 related classes, 49 methods.
- Fields: `seed_state +0x10`, `seed +0x14`, `this_state +0x18`.
- Two `Range` overloads at `0xDD07E8` and `0xDE93DC`.
- `address 0xDD07E8` resolves to `RGRandom$$Range`.
- Direct xref scan: **414** call sites.
- Symbol export: **103252** symbols.

### Ghidra import/annotation

```bash
tools/re/ghidra_import.sh /tmp/opencode/ghidra_smoke SoulKnightSmoke
```

Result: exit 0; AArch64 ELF imported; `ApplyIl2CppSymbols` applied **96904** symbols and
skipped **6348** (duplicate/unsupported labels). The optional Ghidra path is working.
The primary llvm-objdump path does not require Ghidra.

### Asset extraction from a clean environment

Commands:

```bash
rm -rf tools/asset/.venv assets/generated/first-pass
tools/asset/extract.sh extract --profile first-pass
```

Result:

- UnityPy **1.25.4** installed from the pinned `tools/asset/requirements.txt`.
- **284** manifest entries, **0** warnings.
- Profiles: `room-r1-1` (102), `character-c01-candidate` (133),
  `enemy-enemy01-slime-candidate` (7), `enemy-e-slime01` (1),
  `weapon-candidate` (5), `ui-sprites` (30), `audio-sample` (6).
- Four distinct source bundle SHA-256 values recorded.
- Every entry has a source bundle, source hash, path ID, Unity type, name, output path,
  and status.
- Extracted bytes remain under gitignored `assets/generated/`.

---

## 6. Git verification

Before committing the repair pass:

- `git status`: only intended source/doc/tool changes plus five deleted stub headers.
- `git diff --check`: clean.
- `git log --oneline --decorate -20`: no history rewrite; baseline `4f61c3d` intact.
- `git ls-files`: 100 tracked entries before the deletion commit; the repaired tree
  tracks 96 files including this audit.
- No tracked `.apk`, `.so`, `.dll`, `.exe`, archive, extracted image/audio, build output,
  virtualenv, Il2CppDumper output, secret, key, or environment file.
- `git check-ignore` confirms `original/*.apk`, `assets/generated/`, `build/`, the
  tool venv, and `analysis/apktool_out/` are ignored.
- `analysis/assets/extraction_manifest.json` is intentionally tracked; it contains
  metadata and hashes, not asset bytes.
- The five stub headers are deleted and their CMake references removed.
- The repair pass is committed as a coherent change set; the working tree is clean
  afterwards.

---

## 7. Current phase and gates

| Phase | Status |
|---|---|
| Phase 0 — Environment | ✅ COMPLETE |
| Phase 1 — APK Forensics | ✅ COMPLETE (artifact inventory) |
| Phase 2 — Engine Reconstruction | ⚠️ PARTIAL — Gate B **NOT MET** |
| Phase 3 — Behavioural Reconstruction | 🔄 **CURRENT** — RNG core reconstructed; damage partial |
| Phase 4 — Native Runtime | ⚠️ PARTIAL — foundation implemented; Gate C **PARTIAL** (superseded note above) |
| Phase 5 — First Playable | ⛔ BLOCKED |
| Phase 6+ — Systems/Content/Fidelity/Release | ⛔ BLOCKED |

Canonical baseline remains Soul Knight 2.6.8, Unity 2018.4.12f1, IL2CPP metadata v24,
CachyOS x86-64, Wayland only, Vulkan target, 1920×1080, 60 FPS target, native
executable. No Android runtime, Waydroid, emulator, Wine, X11, or XWayland is part of
the project.

What is genuinely done:

- build/test/evidence tooling is reproducible,
- RGRandom core is reconstructed and tested,
- damage state mutation is partly reconstructed,
- box projectile target bridge is HIGH CONFIDENCE,
- asset extraction is reproducible.

What was not done at this audit baseline (superseded where noted):

- no renderer/swapchain (superseded: Phase 4 foundation),
- no gameplay loop,
- no verified player/enemy/weapon/room identities,
- no projectile → controller damage bridge,
- no crit/element rules,
- no resolved reference tick rate (superseded: 50 Hz confirmed),
- no vertical slice.

---

## 8. Exact recommended next action

Continue **Phase 3 static evidence only**: resolve the enemy/player damage dispatch at
`RGBulletTrigger.OnTriggerEnter2D` `0x1160F78`.

Concretely:

1. Use the working Ghidra import and type propagation to resolve the generic `T` for
   `GameObject.GetComponent<T>` at GOT slot `0x309A4E8` and the method at vtable `+0x2C8`.
2. Decide whether it is `RGController.GetHurt` (`0x11795CC`) or
   `RGEController.GetHurt` (`0x1108E60`), or a different virtual method, using the
   method signature and `this` type, not field names.
3. Record the exact instruction sequence and confidence in `analysis/re/damage.md`.
4. Only after that, decode critical-hit and element/status application.

Do not start Phase 4, the vertical slice, or renderer work until Gate B and the
vertical-slice evidence gates pass. Do not weaken the evidence standard to make a gate
green.
