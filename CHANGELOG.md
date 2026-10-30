# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial CachyOS/Wayland project structure per NEMOTRON_MASTER_PROJECT.md
- Repository initialization with canonical directory layout
- Immutable APK artifact recording (SHA-256: 8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea)

### Reverse Engineering
- APK forensics complete: manifest, DEX inventory, native libraries, AssetBundles
- IL2CPP metadata extracted from `libil2cpp.so` (Unity 2021, metadata v24.1)
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
- Engine confirmed: Unity 2021.x IL2CPP, all gameplay in `libil2cpp.so`
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