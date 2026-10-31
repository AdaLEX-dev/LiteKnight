# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Clean dev room, eight-way facing, character/obstacle test switching — 2026-10-31

- Removed the mysterious upper-right wall geometry. Root cause: the native scene
  builder added the `wall601` prefab asset root local position `(20,20)` as a
  sprite offset even though `CreateWall` overrides it with the wall-cell
  coordinate. The builder now ignores root-prefab positions; a source/path
  regression test asserts wall sprite positions equal their cells and no
  instance lies outside the 41×41 map cell.
- Formalized eight-way normalized movement, opposing-axis cancellation, and
  equal cardinal/diagonal speed (9.75 effective) at the fixed 50 Hz step.
- Recovered the directional facing model: no directional character clips exist;
  `RGController.facing` mirrors the `img` hierarchy 180° around Y. Native
  rendering uses body-only horizontal flip, retains facing on pure vertical
  input, and never flips the shadow.
- Extracted all 15 recovered character prefabs (`c00..c14`) and their 30
  `ide`/`run` clips. Dev controls: 1..0 select, `[`/`]` cycle. Switching changes
  only body sprite/clip source; position, collision, and camera persist.
- Added runtime obstacle variant switching over the 8 solid recovered
  `MapManagerLevel.obstacle_list` entries (`P` cycles; 12 entries remain in
  metadata). Render and collision cells are generated from the same variant.
- Added direction, player-render, character-switch, obstacle-variant, and
  wall-artifact tests. Suite: 71 cases / 4638 assertions.

### Player render sync / animated doors / obstacle replacement — 2026-10-31

- Fixed the stationary-Knight bug: `Application::BuildRenderBatch` now resolves
  c00 body/shadow positions from `PlayerLocomotion` through an explicit
  `PlayerRenderSync` binding. The shadow keeps its recovered `(0,-0.05)` offset;
  animation and camera follow cannot move the player world transform.
- Added `sim`/`render`/`body`/`shadow`/`camera` output to
  `--locomotion-debug`.
- Recovered the serialized `door_open`, `door_close` and `door_close2` Animator
  clips, including transform, renderer visibility, sprite PPtr, and collider
  curves.
- Added `game/room/Door` with `OPEN`/`CLOSING`/`CLOSED`/`OPENING` states and
  `RoomDoorController`; door rendering and collision now follow clip timing.
  **O** toggles OpenDoor/CloseDoor and **C** closes all doors.
- Replaced r1_1's 49 `RGObjectSkin` placeholder markers with the recovered
  `wall603` obstacle representation and 49 obstacle AABBs. The wall visual and
  collision cell sets are now tested for exact correspondence.
- Moved the native test spawn from `(20,20)` to `(20,15)` because the recovered
  central obstacle block occupies the c00 prefab origin.
- Added player-render, door-state/collision, and wall/obstacle visual
  correspondence tests. Suite: 63 cases / 3913 assertions.

### Gate B / Phase 3 Evidence Pass — 2026-10-31

- Confirmed the reference fixed timestep is **0.02 s (50 Hz)** from the serialized
  `TimeManager`; render target remains 60 FPS.
- Resolved the projectile damage bridge: player `RGController.GetHurt` (`+0x228`),
  enemy `RGEController.GetHurt` (`+0x2C8`), box `RGBox.Hit` (`+0x168`).
- Resolved `RoleAttributePlayer.CanDie` to `RGGameProcess.this_index` (`+0x1C`);
  removed the incorrect `RGGameInfo.map_random_seed` interpretation.
- Resolved `RGRandom` map seed and `MapManagerLevel` LongMap branch; documented the
  remaining `seed_state` 1/2 and reference-vector gaps.
- Added `analysis/re/timing.md`, `analysis/re/input_movement.md`,
  `analysis/re/rooms_scene.md`, and `analysis/re/weapons_projectiles.md`.
- Added `docs/GATE_B.md`, `docs/PHASE_3_BEHAVIOURAL_RE.md`, and
  `docs/PHASE_4_NATIVE_RUNTIME.md`.
- Updated parity, roadmap, project state, vertical slice, source map, unknown
  systems, architecture, platform, renderer, input, audio, procedural generation,
  testing, and performance documents.
- Gate B and Phase 3 remain **PARTIAL**; no native runtime/gameplay code was added.


### Documentation
- Added `analysis/PROJECT_DIAGNOSIS.md`, `analysis/EVIDENCE_AUDIT.md`, and `docs/EVIDENCE_STANDARD.md`.
- Corrected the project state at the time of the integrity pass: Unity 2018.4.12f1 (metadata v24), not Unity 2021.x; Gate B was NOT MET then and is now **PARTIAL**; Gate C remains NOT MET.
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
- Integrity pass removed unimplemented renderer/audio placeholders, fixed the window event watch, forced Wayland, removed `-march=native`, and corrected the CachyOS packaging assumption.

### Changed
- Current project state, roadmap, parity, architecture, RE log, and bug tracker now distinguish current truth from historical claims.
- `docs/PARITY.md` now contains only evidence-backed rows; the old speculative matrix is preserved in Git history/evidence audit.
- `docs/VERTICAL_SLICE.md` gates now list exact missing evidence and responsible subsystems.

### Known Issues
- No runtime observation oracle (no device/emulator/Frida); analysis is static-only.
- Projectile → controller/box damage bridge is resolved; effect-trigger critical/element details and full death/reborn branches remain partial.
- RGRandom `seed_state` 1/2 and DateTime seed exactness remain unresolved; the `MapManagerLevel` LongMap branch is resolved.
- Renderer/Vulkan context is not implemented; Gate C remains NOT MET.
- Vertical slice implementation is blocked on the evidence gates in `docs/VERTICAL_SLICE.md`.
- `analysis/il2cpp_dump/` is partial; the full Il2CppDumper output remains outside Git.
- Asset extraction is reproducible; `c00`/`c01`/`e_slime01`/`weapon_000`/`bullet_0` technical identities are confirmed, but display names remain candidate.

### Reverse Engineering
- APK forensics complete: manifest, DEX inventory, native libraries, AssetBundles
- IL2CPP metadata extracted from `libil2cpp.so` (original entry claimed Unity 2021 / metadata v24.1; corrected 2026-10-31 to Unity 2018.4.12f1 / metadata v24)
- Structural class inventory documented; this is not a complete behavioural reconstruction
- Technology decisions recorded: 15 ADRs (C++20, SDL3, Vulkan, miniaudio, etc.), with ADR-0014 superseded by ADR-0016
- Platform layer design documents exist; renderer implementation does not
- Unknown systems tracker: 15 unresolved mysteries (RNG, damage formula, room gen, AI, etc.)
- Source map: planned native mappings only; no gameplay system is implemented

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