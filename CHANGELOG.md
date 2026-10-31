# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.0-dev] - 2026-10-31

### Added
- 1920×1080 logical 16:9 UI/render contract, letterboxing and framebuffer
  diagnostics in the developer overlay (`LogicalViewport`).
- Recovered `pixel_bold` TTF/atlas pipeline (`tools/asset/build_ui_font.py`,
  `BitmapFont`, `TextRenderer`) with metrics, alignment, multiline, shadow and
  pixel snapping. Fallback developer font remains for fresh clones.
- Top-left HP/armor/energy HUD with parity-styled icons and two weapon slots.
- Empty weapon equipment state; weapon rendering is suppressed when the active
  slot is empty.
- Physical weapon pickup prompt (`E PICK UP`), active-slot replacement/drop and
  two-slot switching (`1`/`2`/`Q`).
- Native pause, settings, main menu, character station and weapon station UI.
  No social tab.
- Recovered Living Room/HeroRoom scene: 15 tilemaps (~3,896 tiles), 443 sprites,
  4,260 instances and 91 solid collider bounds; interactive character/forge/
  garden/door markers.
- State-preserving two-room transition between Living Room and `r1_1`.
- Plant/Seed/growth/harvest foundation with deterministic development plots.
- Character stats extraction (HP/armor/energy/speed) and death animation clips
  for c00..c14; character selector displays real sprites and stats.
- Save v2 with unlocked characters, plants, fullscreen/hub flags and v1
  migration.
- Collision/visual/sorting debug bounds (`F4`), hit flash and 60 FPS render
  interpolation over the 50 Hz simulation.

### Changed
- Weapon inventory represents empty slots explicitly; `Weapon::Clear()`.
- Character switching applies recovered per-character stats and speed.
- Hub/room rendering batches raised to 16,384 quads for recovered tile counts.
- Developer overlay uses the recovered pixel font and reports logical/
  framebuffer/scale values.
- Main player flow: `scripts/play.sh` opens main menu -> Living Room; devroom
  remains direct.

### Fixed
- sRGB-authored flat UI colors are converted to linear vertex colors (no grey
  dark panels).
- The old bottom-left debug HUD was replaced by the reference-style top-left HUD.
- Player camera/position render interpolation reduces 50 Hz visual judder.
- Character death uses the recovered death clip when present.

### Testing
- 129 doctest cases / 5,090 assertions.
- Three CTest entries: native unit tests, r1_1 asset audit, HeroRoom audit.
- Interactive scenarios added for UI/font, pickup/no-weapon, two weapons,
  energy/HUD, pause/settings/home, Living Room, character select, animation and
  room transition.
- Vulkan validation, Debug ASan/UBSan, RelWithDebInfo and devroom smoke remain
  in the verification gate.

### Assets / legal
- Recovered UI/title and Hub pixels remain local-only and are not committed.
- Native HUD icons are parity recreations where exact recovered icon mapping is
  still UNKNOWN.
- Reference save format and production hub/forge/plant rules remain UNKNOWN.

## [0.2.0-dev] - 2026-10-31

### Added
- Full local reference catalogue (`analysis/assets/reference_catalogue.json`)
  from all AssetBundles: 365 textures, 6408 sprites, 365 unique weapon IDs,
  125 unique enemy IDs, 2098 clips and 54,891 GameObjects.
- Deterministic `scripts/prepare-assets.sh` and `scripts/audit-assets.sh`;
  APK SHA-256/version verification, catalogue refresh, inventory generation,
  generated-pixel audit and `analysis/assets/audit-report.{json,txt}`.
- Asset laboratory (`--asset-lab`, `F5`): category/item cycling, real previews,
  metadata, PPU/pivot, animation play/pause/step and test actions.
- Serialized interactive tests (`--interactive-test=<name>`) with real
  Wayland/Vulkan window, per-tick input/state recording, commands, asset/scene
  hashes and `test-results/<timestamp>_<name>.json`.
- Named scenarios and `scripts/test-interactive.sh --list|--test|--replay`.
- Deterministic replay; verified zero state differences against recorded
  `combat_live` including enemy spawn, death and reset.
- Live `e_slime01` pursuit/contact behavior with collision sliding, multi-enemy
  spawn/reset, player damage/death and HUD updates.
- Player energy state (recovered c00 max 180), per-shot consumption and native
  HP/armor/energy HUD bars.
- Two independent weapon slots (`1`/`2`) preserving weapon, projectile,
  cooldown and energy behavior.
- Weapon pickup foundation using real weapon sprites (`G` spawn, `E` pickup).
- Versioned native save/load (`F2`/`F3`) with atomic write, backup fallback,
  migrations and XDG path.
- `AssetEnvironment` startup report with actual local paths and counts.

### Changed
- Weapon fire accepts an optional `EnergyState`; unaffordable shots are
  rejected deterministically.
- Character quick-select moved to `Ctrl+1..0`; plain `1`/`2` switch weapons.
- Enemy fixtures moved outside the first-slice aggro radius so the devroom is
  inspectable at spawn.
- Asset inventory now records the maturity ladder and per-category lifecycle.

### Fixed
- Asset-lab weapon activation now equips into the active inventory slot.
- Ctrl+number no longer gets consumed by weapon slot switching.
- HUD/energy context is populated from live gameplay state.

### Testing
- 115 test cases / 4983 assertions in RelWithDebInfo and Debug ASan/UBSan.
- Metadata audit: 0 errors, 1 expected local-only warning.
- Interactive results recorded for character/weapon/enemy/energy/health/
  two-weapon/save-load/combat scenarios; replay state comparison exact.

### Assets / legal
- Reference-derived pixels remain `LOCAL_ONLY`, `REFERENCE_DERIVED`,
  `NOT_FOR_REDISTRIBUTION`; no proprietary binaries or pixels are committed.
- Developer text atlas remains PLACEHOLDER UI.
- Room transitions, start GUI and hub remain UNIMPLEMENTED/UNRECOVERED.

## [0.1.0-dev] - 2026-10-31

### Added
- Permanent native developer executable `build/bin/liteknight-devroom` and
  `--devroom` mode.
- One-command workflow: `scripts/dev.sh`, `scripts/build-dev.sh`,
  `scripts/test.sh`, `scripts/run-devroom.sh`, `scripts/smoke-devroom.sh`.
- Developer overlay (`F10`) with version/build, FPS/frame time, fixed ticks,
  character, weapon, HP/armor, enemies, projectiles, position, facing, room,
  renderer and platform.
- Character selector (`F6`) for all recovered `c00..c14` with real body sprites,
  current selection, keyboard navigation and confirmation.
- Weapon selector (`F7`) and real recovered weapon textures. Four Gun001
  weapons are integrated: `weapon_000`, `weapon_002`, `weapon_048`, `weapon_287`.
- Enemy selector (`F8`), spawner (`F9`/`F11`) and clear (`F12`) for recovered
  enemy definitions (`e_slime01` initially).
- `analysis/assets/texture_inventory.json`, `asset_inventory.json`, metadata
  audit tooling and `tools/asset/pipeline.py`.
- Legal boundary documentation, MIT native source license, NOTICE, LEGAL.md and
  docs/legal.
- GitHub remote/CI/release documentation and release notes.

### Changed
- Project/version identity is now LiteKnight `0.1.0-dev`; startup logs and
  window title reflect the native version.
- Weapon rendering is separated into `WeaponRenderer`; recovered weapon visuals
  are drawn from real prefab sprites.
- EnemySystem now exposes deterministic developer spawn/clear/reset semantics.
- Scene metadata now carries four recovered weapon definitions and their
  projectile/visual metadata.

### Fixed
- Player facing no longer shifts the body against its shadow: sprite flips mirror
  quad geometry around the pivot as well as swapping UVs.

### Reverse Engineering
- Recovered the held attack path, weapon Animator cadence and c00 spread formula.
- Corrected the enemy despawn timing: 4.0 s destruction default; 0.95 is friction.

### Assets
- 14 texture records / 182 sprites integrated into the devroom scene.
- 15 characters, 4 weapons, 3 unique projectiles, 1 enemy, 4 doors and 12 obstacle
  variants integrated; reference-derived pixels remain `local_only`.
- Developer 5x7 text atlas is a native PLACEHOLDER pending a licensed/recovered
  UI font.

### Testing
- 87 cases / 4796 assertions in RelWithDebInfo and Debug ASan/UBSan, plus
  Vulkan validation, devroom smoke and real-input manual checks.

### First playable combat prototype — 2026-10-31

- Added a native combat loop in the recovered `r1_1` dev room:
  `player -> weapon_000 -> bullet_0 -> wall/obstacle/enemy -> damage ->
  enemy death -> player damage/death -> R reset`.
- Recovered the held-attack timing from `weapon_000`'s serialized
  `weapon_pistol` Animator: `w_pistol` loops for 1/3 s and its `Attack`
  animation event fires at 1/30 s. Attack input is a native test binding
  (`J`/`Space`/LMB), not reference keyboard parity. Aim is a separate native
  test vector (mouse/right stick); facing remains movement-driven.
- Corrected the spread formula to
  `RoleAttribute.deviation * (weapon.deviation + 1)`. c00's attribute deviation
  is 0, so weapon_000's effective spread is 0°, not ±5°.
- Added `ProjectileSystem`, `Weapon`, `HealthState`, and `EnemySystem`. Effective
  projectile speed is 36 (prefab 10 retained), damage 3, camp 1, lifetime 5 s;
  deterministic swept collision blocks walls/obstacles/doors and rejects targets
  behind a wall hit.
- Added two deterministic `e_slime01` fixtures: hp 12, contact damage 3 every
  1.5 s, dead sprite, immediate collision/target disable, and 4.0 s despawn.
  Corrected the earlier 0.95 s “death delay”: 0.95 is written to `friction`;
  `dead_destory_delay_time` defaults to 4.0 s.
- Added player HP/armor/contact/death and `R` reset. The dead player stops
  movement/attack and renders the recovered final dead sprite; production
  game-over/reborn UI is out of scope.
- Fixed the facing visual regression: sprite flips now mirror quad geometry
  around the pivot as well as swapping UVs, so the asymmetric c00 pivot no
  longer shifts the body relative to its shadow when facing changes.
- Added combat tests for attack cadence, projectile speed/lifetime/collision,
  armour/HP/defence, enemy death/despawn, wall-order target rejection, and pivot
  mirroring. Suite: 84 cases / 4759 assertions in RelWithDebInfo and Debug
  ASan/UBSan; CTest, Vulkan validation, real keyboard/aim combat, death/reset,
  character/obstacle switching, doors, and window exercise verified.

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