# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Documentation
- Added `analysis/PROJECT_DIAGNOSIS.md`, `analysis/EVIDENCE_AUDIT.md`, and `docs/EVIDENCE_STANDARD.md`.
- Corrected the project state: Unity 2018.4.12f1 (metadata v24), not Unity 2021.x; Gate B and Gate C are NOT MET.
- Superseded the PCG32 parity ADR with ADR-0016 (reference-compatible RNG).
- Added `analysis/re/rgrandom.md`, `analysis/re/damage.md`, `docs/VERTICAL_SLICE.md`, and `analysis/assets/README.md`.

### Added
- Reproducible static-analysis pipeline under `tools/re/` (`il2cpp_re.py`, Ghidra annotation script, documentation).
- Reference-compatible `UnityRandom` / `RGRandomStream` in `src/engine/math/UnityRandom.*`.
- Reproducible UnityPy extraction under `tools/asset/` with pinned dependency and first-pass manifest.
- 22 executing unit tests, including 10 RNG regression tests.

### Fixed
- Build now uses system Vulkan headers; removed the fabricated `external/vulkan` copy.
- Added `fmt` discovery/linking, Vulkan loader linking, and `SOUL_KNIGHT_COMMIT_HASH`.
- Fixed SDL3 Vulkan surface creation and `std::filesystem::path` formatting.
- Enabled doctest, fixed ECS component storage and input injection, and replaced empty CTest filters with a real unit-test entry.
- An intentional test failure now turns CTest red; the restored suite passes.

### Known Issues
- No runtime observation oracle (no device/emulator/Frida); analysis is static-only.
- Projectile-effect → target `GetHurt` bridge, critical hits, and element/status formulas remain unresolved.
- Renderer/Vulkan context is not implemented; Gate C remains NOT MET.
- Vertical slice implementation is blocked on the evidence gates in `docs/VERTICAL_SLICE.md`.
- `analysis/il2cpp_dump/` is partial; the full Il2CppDumper output remains outside Git.
- `src/`, `tests/`, `tools/`, and the new analysis docs require a coherent Git baseline commit.

### Reverse Engineering
- APK forensics complete: manifest, DEX inventory, native libraries, AssetBundles
- IL2CPP metadata extracted from `libil2cpp.so` (original entry claimed Unity 2021 / metadata v24.1; corrected 2026-10-31 to Unity 2018.4.12f1 / metadata v24)
- 200+ `RG*` game classes discovered and catalogued
- Engine architecture documented: Unity IL2CPP, 100% gameplay in native lib
- Technology decisions recorded: 15 ADRs (C++20, SDL3, Vulkan, miniaudio, etc.)
- Platform layer designs: Wayland window, Vulkan renderer, miniaudio, input, save system
- Unknown systems tracker: 15 unresolved mysteries (RNG, damage formula, room gen, AI, etc.)
- Source map: 100+ IL2CPP classes mapped to planned native implementations

---

## [0.0.1-bootstrap] - 2026-10-30

### Added
- Repository bootstrap with canonical structure
- `.gitignore` with build/asset/runtime exclusions
- Immutable reference APK recorded and hashed
- Phase 0 (Environment) and Phase 1 (APK Forensics) deliverables
- Phase 2 (Engine Reconstruction) deliverables

### Reverse Engineering
- APK manifest parsed: package `com.ChillyRoom.DungeonShooter`, version 2.6.8 (20680)
- DEX inventory: 3 DEX files, ~10,446 classes (Unity + SDKs only, zero gameplay)
- Native libraries: `libil2cpp.so` (52 MB), `libunity.so` (17 MB), `libmain.so`, Firebase, GPG
- AssetBundles: 30+ bundles catalogued (40+ MB content)
- IL2CPP metadata: `global-metadata.dat` extracted, 200+ `RG*` classes dumped
- Engine confirmed: Unity IL2CPP, all discovered gameplay code in `libil2cpp.so` (original 2021.x claim corrected 2026-10-31 to 2018.4.12f1)
- Class hierarchy mapped: Player, Enemies (20+ AI types), Weapons (25+), Projectiles (35+), Rooms (8 MapManagers), Items, Progression

### Documentation
- `NEMOTRON_MASTER_PROJECT.md` — Master specification
- `analysis/ARTIFACT.md` — Reference artifact record
- `analysis/APK_INVENTORY.md` — Complete APK inventory
- `analysis/class-map.md` — DEX class map
- `analysis/native-libraries.md` — Native library analysis
- `analysis/resources.md` — AssetBundle catalog
- `analysis/UNKNOWN_SYSTEMS.md` — 15 unresolved mysteries
- `analysis/SOURCE_MAP.md` — Reference → native file mapping
- `docs/ARCHITECTURE.md` — Complete system architecture
- `docs/REVERSE_ENGINEERING.md` — RE session log
- `docs/TECHNOLOGY_DECISIONS.md` — 15 ADRs
- `docs/PLATFORM.md` — Wayland/SDL3/Vulkan platform layer
- `docs/RENDERING.md` — Vulkan sprite batcher design
- `docs/INPUT.md` — Input action mapping
- `docs/AUDIO.md` — miniaudio bus system
- `docs/SAVE_SYSTEM.md` — JSON save with versioning
- `docs/PARITY.md` — Fidelity tracking matrix
- `docs/BUGS.md` — Bug tracker template
- `docs/CONTRIBUTING.md` — Contribution guide
- `docs/PROJECT_STATE.md` — Session state tracker
- `docs/ROADMAP.md` — 11-phase roadmap

---

## Versioning Scheme

- **MAJOR** = Breaking architecture/runtime changes
- **MINOR** = Major capabilities (first playable, core systems, content)
- **PATCH** = Fixes, documentation, small compatibility changes
- **Pre-release**: `-alpha.N`, `-beta.N`, `-rc.N`

---

## Release Tags

| Tag | Date | Description |
|-----|------|-------------|
| `v0.0.1-bootstrap` | 2026-10-30 | Initial project structure + APK forensics + engine reconstruction |