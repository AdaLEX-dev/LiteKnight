# SOUL KNIGHT 2.6.8 → NATIVE CACHYOS / WAYLAND
## Nemotron Master Project Specification — v1.0

> **Purpose:** Long-running hobby reverse-engineering, preservation, interoperability, and native-port engineering project.
>
> **Canonical target:** CachyOS, x86-64, Wayland, 1920×1080, 60 FPS.
>
> **Final runtime:** Native Linux executable. No Android runtime, emulator, Waydroid, Wine, X11, or XWayland as the intended runtime.

---

# 1. MISSION

Transform an authorized, locally supplied Soul Knight 2.6.8 APK from an immutable reference artifact into an independently maintainable native Linux implementation.

Pipeline:

```text
APK
 ↓
forensics
 ↓
static analysis
 ↓
runtime experiments
 ↓
architecture reconstruction
 ↓
behavioural specification
 ↓
native platform layer
 ↓
engine/game systems
 ↓
content implementation
 ↓
parity testing
 ↓
profiling
 ↓
CachyOS package
```

The APK is **evidence, not architecture**.

Decompiler output is **evidence, not source code**.

Do not blindly transliterate decompiled code. Understand behaviour, write clean native code, and preserve evidence in `analysis/`.

Never claim parity without evidence.

---

# 2. TARGET PLATFORM

Canonical:

- CachyOS
- Arch Linux userspace
- x86-64
- Wayland
- 1920×1080 primary target
- 60 FPS target
- keyboard + mouse
- gamepad where practical
- native Linux filesystem/XDG conventions

Wayland-only means:

- no X11 backend
- no Xlib/XCB dependency as a deliberate platform layer
- no XWayland requirement
- no Wine
- no Android runtime
- no emulator/Waydroid

If a third-party dependency has an unavoidable indirect X11 library dependency, document it; do not design around it.

---

# 3. DESIGN PHILOSOPHY

## 3.1 Evidence over assumptions

Use REA, JADX, Ghidra, Apktool, binary tools, and controlled runtime observations.

Evidence labels:

- `CONFIRMED`
- `HIGH CONFIDENCE`
- `HYPOTHESIS`
- `UNKNOWN`
- `CONTRADICTED`

Never silently upgrade a hypothesis.

## 3.2 Reconstruct before reimplementing

Understand:

- engine
- game loop
- renderer
- input
- audio
- resources
- game state
- combat
- AI
- procedural generation
- persistence
- UI

before mass implementation.

## 3.3 Native means native

The final executable must not launch the Android application or require an Android compatibility layer.

## 3.4 Historical fidelity

Target **2.6.8**, not current Soul Knight.

Do not silently import mechanics, assets, UI, or balance from later versions.

## 3.5 Small, reversible changes

Prefer focused commits and architecture that can be changed without destroying evidence.

## 3.6 Correctness before optimization

First make behaviour correct and measurable. Then optimize based on profiling.

## 3.7 Documentation is a deliverable

The repository must explain what was discovered, why architectural decisions were made, and what remains unknown.

---

# 4. LEGAL / DISTRIBUTION BOUNDARY

The reference APK is a locally supplied artifact.

Do not:

- distribute the original APK
- publish proprietary extracted assets
- publish proprietary recovered source
- attack online infrastructure
- steal credentials
- bypass unrelated security systems
- upload private device data

Public Git history must contain only material appropriate for redistribution.

Never commit:

- APKs
- extracted proprietary dumps
- saves
- credentials
- tokens
- private keys
- machine-specific secrets

---

# 5. REPOSITORY

Canonical structure:

```text
SoulKnight-Linux/
├── original/
│   ├── README.md
│   └── SoulKnight-2.6.8.apk          # NEVER COMMIT
├── analysis/
│   ├── original-apk.sha256
│   ├── ARTIFACT.md
│   ├── APK_INVENTORY.md
│   ├── SOURCE_MAP.md
│   ├── UNKNOWN_SYSTEMS.md
│   ├── class-map.md
│   ├── native-libraries.md
│   ├── resources.md
│   ├── dex/
│   ├── native/
│   ├── callgraphs/
│   ├── runtime/
│   ├── gameplay/
│   ├── data/
│   └── experiments/
├── docs/
│   ├── ARCHITECTURE.md
│   ├── REVERSE_ENGINEERING.md
│   ├── TECHNOLOGY_DECISIONS.md
│   ├── PORTING.md
│   ├── PLATFORM.md
│   ├── RENDERING.md
│   ├── AUDIO.md
│   ├── INPUT.md
│   ├── SAVE_SYSTEM.md
│   ├── PROCEDURAL_GENERATION.md
│   ├── COMBAT.md
│   ├── AI.md
│   ├── UI.md
│   ├── ASSET_PIPELINE.md
│   ├── TESTING.md
│   ├── PERFORMANCE.md
│   ├── RELEASE.md
│   ├── CONTRIBUTING.md
│   ├── PROJECT_STATE.md
│   ├── ROADMAP.md
│   ├── PARITY.md
│   ├── BUGS.md
│   └── DECISIONS/
├── src/
│   ├── app/
│   ├── platform/
│   │   ├── wayland/
│   │   ├── filesystem/
│   │   ├── input/
│   │   ├── timing/
│   │   └── audio/
│   ├── engine/
│   │   ├── core/
│   │   ├── math/
│   │   ├── ecs/
│   │   ├── rendering/
│   │   ├── animation/
│   │   ├── resources/
│   │   ├── audio/
│   │   └── collision/
│   └── game/
│       ├── player/
│       ├── characters/
│       ├── enemies/
│       ├── weapons/
│       ├── projectiles/
│       ├── items/
│       ├── rooms/
│       ├── generation/
│       ├── combat/
│       ├── status/
│       ├── progression/
│       ├── npc/
│       ├── bosses/
│       ├── ui/
│       └── save/
├── assets/
│   ├── README.md
│   ├── generated/
│   └── converted/
├── data/
│   ├── weapons/
│   ├── enemies/
│   ├── items/
│   ├── characters/
│   ├── rooms/
│   └── localization/
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── regression/
│   ├── gameplay/
│   ├── serialization/
│   ├── procedural/
│   └── performance/
├── tools/
│   ├── configure.sh
│   ├── build.sh
│   ├── run.sh
│   ├── test.sh
│   ├── profile.sh
│   ├── package.sh
│   ├── analyze-apk.sh
│   ├── verify-environment.sh
│   ├── git-release.sh
│   └── changelog.sh
├── .github/workflows/
├── CMakeLists.txt OR Cargo.toml
├── CMakePresets.json
├── .gitignore
├── README.md
├── NEMOTRON.md
├── CHANGELOG.md
├── VERSION
└── LICENSE
```

Do not create every directory just for appearances. Create structure as it becomes useful, while keeping the intended architecture documented.

---

# 6. TECHNOLOGY SELECTION

Before implementation, explicitly decide:

- C++ or Rust
- SDL3 or another Wayland-native window/input abstraction
- OpenGL or Vulkan
- audio backend
- image/texture formats
- configuration format
- save format
- test framework
- logging
- profiling stack

Record decisions in:

`docs/TECHNOLOGY_DECISIONS.md`

and important architectural choices as ADRs.

Decision criteria:

1. Wayland support
2. CachyOS/Arch availability
3. maintainability
4. debugging
5. performance
6. dependency size
7. licensing
8. build reproducibility
9. suitability for a 2D game
10. ease of replacing the dependency later

The simplest robust solution wins.

---

# 7. DEPENDENCY POLICY

Classify dependencies:

- build dependency
- runtime dependency
- development tool
- analysis tool
- optional tool

Likely tools:

- Git
- CMake/Ninja or Cargo
- GCC/Clang
- GDB/LLDB
- Python
- REA
- Ghidra
- JADX
- Apktool
- ADB
- readelf
- objdump
- nm
- strings
- file
- patchelf
- perf
- heaptrack
- RenderDoc where appropriate

Before adding a dependency, document:

- purpose
- alternatives
- license
- runtime cost
- build cost
- Wayland compatibility
- maintenance status
- removal/replacement difficulty

---

# 8. PHASE ROADMAP

## Phase 0 — Environment

Verify:

- CachyOS
- Wayland
- compiler
- build system
- Git
- REA
- Ghidra
- JADX
- Apktool
- ADB
- profiling tools

Deliverable:

`docs/PROJECT_STATE.md`

Gate: all critical tooling works.

## Phase 1 — APK forensics

Record:

- SHA-256
- package
- version
- signing info
- SDK
- manifest
- DEX count
- ABIs
- native libraries
- permissions
- resources
- assets
- activities/services/providers

Deliverables:

- `analysis/ARTIFACT.md`
- `analysis/APK_INVENTORY.md`
- `analysis/class-map.md`
- `analysis/native-libraries.md`
- `analysis/resources.md`

Gate: reference artifact understood at inventory level.

## Phase 2 — Engine reconstruction

Determine:

- Unity/libGDX/Cocos/custom/etc.
- Java/native split
- renderer
- game loop
- asset system
- audio
- input
- persistence

Deliverable:

`docs/ARCHITECTURE.md`

Gate: major engine boundaries mapped.

## Phase 3 — Gameplay reconstruction

Recover:

- player
- enemies
- weapons
- projectiles
- collision
- rooms
- generation
- items
- progression
- bosses
- UI
- saves

Gate: core behavioural model exists.

## Phase 4 — Native runtime

Implement:

1. Wayland window
2. frame timing
3. renderer bootstrap
4. input
5. asset loading
6. camera
7. basic scene

Gate: native application launches reliably.

## Phase 5 — First playable

Must support:

- player movement
- attack
- enemy
- collision
- damage
- death
- restart
- exit

Gate: genuinely playable native loop.

## Phase 6 — Core systems

Implement:

- weapons
- projectiles
- combat
- AI
- rooms
- procedural generation
- items
- characters
- progression
- audio
- UI
- saving

## Phase 7 — Content parity

Build a parity matrix for every discovered:

- character
- weapon
- enemy
- boss
- item
- room
- status effect
- UI screen

## Phase 8 — Fidelity

Compare:

- timing
- movement
- combat
- animations
- generation
- UI
- sound
- visual effects

## Phase 9 — Performance

Measure:

- frame time
- 1% low
- spikes
- CPU
- GPU
- memory
- load time

Target 16.6667 ms/frame at 60 FPS.

## Phase 10 — Packaging

Produce a reproducible CachyOS package.

## Phase 11 — Release

Only after all release gates pass.

---

# 9. REVERSE ENGINEERING METHOD

Use:

1. inventory
2. identify subsystem
3. find entry points
4. inspect callers/callees
5. inspect state/fields
6. inspect resources
7. runtime experiment
8. document behaviour
9. implement native version
10. regression test

Never paste giant decompiler output into the native source tree.

Instead:

```text
decompiler
 ↓
understanding
 ↓
clean design
 ↓
native implementation
 ↓
test
```

---

# 10. APK FORENSICS

The original APK is immutable.

Calculate and store:

```text
sha256sum original/SoulKnight-2.6.8.apk
```

Record:

- file size
- SHA-256
- package
- version
- manifest
- permissions
- activities
- services
- receivers
- providers
- DEX
- native libraries
- resources
- assets

Do not infer missing information.

---

# 11. JAVA / DEX ANALYSIS

Create a class map containing:

- class
- package
- superclass
- interfaces
- fields
- important methods
- callers
- callees
- subsystem
- confidence
- native replacement
- implementation status

Prioritize:

- application entry
- activity
- game loop
- renderer
- player
- enemies
- weapons
- rooms
- procedural generation
- items
- progression
- save
- UI
- audio

---

# 12. NATIVE BINARY ANALYSIS

For every `.so`, record:

- filename
- ABI
- architecture
- size
- sections
- imports
- exports
- symbols
- JNI
- strings
- likely responsibility

Use Ghidra for deeper analysis.

Prioritize:

- initialization
- JNI
- renderer
- loop
- input
- audio
- filesystem
- serialization
- physics/collision
- AI
- generation

---

# 13. RUNTIME EXPERIMENTS

Where a lawful test environment is available, create reproducible experiments.

Every experiment:

```text
ID:
Question:
Setup:
Action:
Observation:
Expected:
Actual:
Conclusion:
Confidence:
Evidence:
```

Store in:

`analysis/experiments/`

Important observations include:

- startup
- menus
- movement
- attacks
- damage
- death
- room transitions
- item pickup
- shops
- bosses
- saves
- settings
- procedural generation

---

# 14. GAME LOOP

Prefer a separation resembling:

```text
platform events
      ↓
input snapshot
      ↓
fixed simulation
      ↓
render state
      ↓
render
      ↓
audio
      ↓
present
```

But if the reference uses a materially different model, document it and reproduce its behaviour.

Simulation speed must not accidentally depend on monitor refresh rate.

---

# 15. PLATFORM ARCHITECTURE

Game code must not directly know Wayland structures.

Preferred:

```text
Game
 ↓
Engine abstractions
 ↓
Platform interfaces
 ↓
Wayland/Linux implementation
```

Subsystems:

- `Window`
- `Input`
- `Audio`
- `Filesystem`
- `Clock`
- `Renderer`

---

# 16. RENDERING

Investigate:

- logical resolution
- camera
- sprites
- atlases
- blending
- batching
- shaders
- particles
- UI
- text
- transitions

Build progressively:

1. window
2. graphics context
3. clear
4. frame timing
5. texture
6. sprite
7. camera
8. batching
9. animation
10. particles
11. UI
12. shaders

Do not build a massive general-purpose engine unless evidence requires it.

---

# 17. INPUT

Abstract actions:

```text
MoveUp
MoveDown
MoveLeft
MoveRight
Aim
Attack
Interact
Ability
Dodge
Inventory
Pause
Confirm
Cancel
```

Do not expose Wayland event types to gameplay.

---

# 18. AUDIO

Separate:

- music
- SFX
- UI
- ambience
- loops

Expose:

```text
play
stop
pause
resume
set_volume
set_bus_volume
```

Investigate timing and trigger semantics.

---

# 19. PLAYER

Recover:

- health
- armor
- energy
- speed
- attack modifiers
- abilities
- character class
- inventory
- weapon
- buffs/debuffs
- status effects

Do not hardcode gameplay values into rendering.

---

# 20. COMBAT

Recover and test:

- damage
- critical chance
- critical multiplier
- armor
- resistances
- knockback
- projectile speed
- lifetime
- spread
- fire rate
- cooldown
- range
- elemental effects
- status effects

Every important rule should eventually have a regression test.

---

# 21. WEAPONS

Separate:

`WeaponDefinition`

from:

`WeaponInstance`

Potential definition fields:

- ID
- name
- damage
- fire rate
- projectile count
- spread
- projectile speed
- lifetime
- energy cost
- knockback
- critical chance
- element
- effects
- animation
- sound
- sprite

Do not invent fields unless the behaviour requires them.

---

# 22. ENEMIES

Separate:

- EnemyDefinition
- EnemyInstance
- EnemyAI
- EnemyAttack
- EnemyLoot

Investigate:

- targeting
- navigation
- aggression
- attack selection
- cooldowns
- special attacks
- death
- loot
- boss phases

---

# 23. PROCEDURAL GENERATION

Recover:

- RNG
- seed
- room graph
- templates
- entrances/exits
- enemy placement
- item placement
- biome rules
- boss rules
- reward rules

If deterministic:

```text
generate(seed X)
generate(seed X)
```

must match.

If not deterministic, document why.

Preserve RNG ordering where behaviour depends on it.

---

# 24. SAVE SYSTEM

Native user data:

```text
$XDG_DATA_HOME/soul-knight/
```

fallback:

```text
~/.local/share/soul-knight/
```

Configuration:

```text
$XDG_CONFIG_HOME/soul-knight/
```

fallback:

```text
~/.config/soul-knight/
```

Never save into the repository.

Native saves must be versioned.

Test:

```text
save → load → compare state
```

---

# 25. UI

Separate:

- UI state
- layout
- rendering
- input

Expected screens to investigate:

- title
- character selection
- HUD
- pause
- inventory
- shop
- settings
- results
- progression
- dialogs

Gameplay must not depend on UI pixel coordinates.

---

# 26. ASSET PIPELINE

Pipeline:

```text
reference
 ↓
extraction
 ↓
conversion
 ↓
validation
 ↓
native packaging
```

Never modify reference assets in place.

Generated assets must be reproducible.

Record tools and versions.

---

# 27. DATA-DRIVEN CONTENT

Where appropriate:

```text
WeaponDefinition → WeaponInstance
EnemyDefinition  → EnemyInstance
```

Benefits:

- testing
- inspection
- balancing
- serialization
- content tracking

Do not add abstractions purely for aesthetics.

---

# 28. DETERMINISM

Where possible:

- expose seed
- log seed
- test seed
- isolate RNG streams
- preserve important call order

Randomness must not be introduced casually.

---

# 29. TESTING

Testing pyramid:

1. unit
2. subsystem
3. gameplay simulation
4. serialization
5. procedural regression
6. integration
7. headless smoke
8. native Wayland launch
9. reference comparison
10. performance

Test names describe behaviour.

Good:

```text
weapon_fire_consumes_energy
same_seed_generates_same_room_graph
armor_reduces_damage
```

Bad:

```text
test1
thing_test
```

---

# 30. PARITY MATRIX

Maintain `docs/PARITY.md`.

Columns:

```text
Subsystem
Reference behaviour
Native behaviour
Confidence
Test
Status
Notes
```

Status:

- UNKNOWN
- PARTIAL
- FUNCTIONAL
- HIGH-FIDELITY
- VERIFIED

---

# 31. BUG TRACKING

`docs/BUGS.md`

Every bug:

```text
ID
Severity
Reproduction
Expected
Actual
Suspected cause
Evidence
Fix
Regression test
```

Severity:

- P0 crash/data loss
- P1 game-breaking
- P2 major fidelity
- P3 minor
- P4 polish

---

# 32. UNKNOWN SYSTEMS

Maintain `analysis/UNKNOWN_SYSTEMS.md`.

Each unknown:

```text
ID
Description
Evidence
Possible explanations
Next experiment
Priority
Status
```

Never allow unresolved mysteries to disappear.

---

# 33. SOURCE MAP

Maintain `analysis/SOURCE_MAP.md`.

Example:

```text
Reference:
    SomeWeaponManager

Native:
    src/game/weapons/WeaponSystem.*

Status:
    IMPLEMENTED

Evidence:
    analysis/dex/...
```

---

# 34. ARCHITECTURE DECISION RECORDS

Directory:

`docs/DECISIONS/`

Each ADR:

```text
# ADR-NNNN: Title

## Status
Accepted / Proposed / Superseded

## Context

## Problem

## Options

## Decision

## Consequences
```

Important early ADRs:

- project scope
- Wayland-only
- language
- renderer
- window/input backend
- audio
- data format
- save format
- architecture boundaries

---

# 35. PROJECT STATE

`docs/PROJECT_STATE.md` is persistent agent memory.

Update after meaningful work:

```text
Date:
Commit:
Phase:
Current status:

Completed:
- ...

In progress:
- ...

Blocked:
- ...

New discoveries:
- ...

Changed assumptions:
- ...

Tests:
Build:
Performance:

Next actions:
```

Every Nemotron session begins by reading this file.

---

# 36. SESSION START

At session start:

```bash
git status
git log -5 --oneline
```

Then read:

```text
docs/PROJECT_STATE.md
docs/ROADMAP.md
```

Determine:

- phase
- milestone
- blocker
- next action

Do not redo old analysis without reason.

---

# 37. SESSION END

Before ending a substantial session:

- build
- run relevant tests
- update project state
- update docs
- update changelog if appropriate
- make coherent commit if appropriate
- leave working tree understandable

If unfinished, explicitly record what remains.

---

# 38. GIT

Git is mandatory.

Branches:

- `main`
- optional `develop`
- optional feature branches

Feature branches:

```text
feature/rendering
feature/combat
feature/procedural-generation
feature/save-system
feature/audio
feature/ui
feature/wayland
feature/reverse-engineering
```

Do not create branches or commits merely for ceremony.

---

# 39. COMMIT CONVENTION

Use Conventional Commits.

Types:

```text
feat:
fix:
refactor:
perf:
test:
docs:
build:
ci:
chore:
reverse:
```

Examples:

```text
reverse: inventory APK manifest and DEX structure
reverse: identify native rendering library
docs: document reconstructed game loop
feat: add native Wayland window
feat: implement fixed simulation timestep
feat: implement player movement
feat: implement projectile collision
test: add deterministic room generation regression
perf: batch sprite draw calls
fix: decouple simulation speed from refresh rate
```

Before every commit:

```bash
git status
git diff --check
git diff
```

Then relevant build/tests.

Never commit:

- APK
- proprietary dump
- secrets
- generated garbage
- unrelated changes

---

# 40. GIT TAGS

Meaningful milestones:

```text
v0.0.1-bootstrap
v0.1.0-recon
v0.2.0-runtime
v0.3.0-first-playable
v0.4.0-combat
v0.5.0-content
v0.6.0-fidelity
v0.7.0-performance
v1.0.0
```

Do not tag every commit.

Tags represent completed milestones.

---

# 41. VERSIONING

Maintain:

`VERSION`

Use SemVer:

```text
MAJOR.MINOR.PATCH
```

Pre-release:

```text
-alpha.1
-beta.1
-rc.1
```

MAJOR = breaking architecture/runtime changes.

MINOR = major capabilities.

PATCH = fixes/documentation/small compatibility changes.

---

# 42. CHANGELOG

`CHANGELOG.md` uses:

```text
Added
Changed
Fixed
Removed
Performance
Reverse Engineering
Documentation
Known Issues
```

Start with:

```markdown
# Changelog

## [Unreleased]

### Added
- Initial CachyOS/Wayland project structure.

### Reverse Engineering
- Initial reference artifact inventory.
```

Never fabricate release notes.

Every release entry describes real changes.

---

# 43. RELEASE PROCESS

A release requires:

1. clean working tree
2. build
3. tests
4. Wayland smoke test
5. 1080p test
6. performance measurement
7. VERSION update
8. CHANGELOG update
9. commit
10. tag
11. verify remote
12. optionally push
13. release notes

Never push blindly.

Before first push:

```bash
git remote -v
git status
git log -5 --oneline
```

Never force-push `main`.

No automatic push to unknown remotes.

---

# 44. RELEASE AUTOMATION

`tools/git-release.sh` should:

1. validate tree
2. run tests
3. run build
4. update version
5. prepare changelog
6. commit
7. tag
8. print push commands

Default is dry-run for remote operations.

Only an explicit `--push` may push to an already verified configured remote.

---

# 45. README

README must contain:

- project goal
- target platform
- current status
- build
- run
- test
- analysis workflow
- repository structure
- scope statement
- limitations
- documentation links

Do not advertise unfinished systems as complete.

---

# 46. DOCUMENTATION UPDATE RULE

Whenever a subsystem changes, update relevant:

- architecture
- subsystem documentation
- tests
- project state
- roadmap
- changelog if user-visible

Do not allow documentation to drift.

---

# 47. REFACTORING

Refactor when:

- platform leakage occurs
- subsystem boundaries are wrong
- duplication becomes significant
- testing becomes difficult
- profiling justifies change

Do not refactor for aesthetics while major unknowns remain.

---

# 48. TEMPORARY CODE

Temporary code must be labelled:

```text
TEMP-RE
EXPERIMENTAL
DEBUG-ONLY
```

Eventually:

- remove it
- promote it
- or document it as permanent tooling

---

# 49. PERFORMANCE

Frame budget:

`16.6667 ms`

Track:

- average frame time
- 1% low
- worst spikes
- simulation time
- render time
- CPU time
- GPU time
- memory
- loading time

Every significant optimization records:

```text
Before
Change
After
Measurement
```

Never call something optimized without measurement.

---

# 50. BUILD MODES

Required:

- Debug
- RelWithDebInfo
- Release

Debug:

- diagnostics
- assertions
- debug instrumentation

RelWithDebInfo:

- profiling
- realistic optimization

Release:

- final runtime

---

# 51. CI

CI should verify where practical:

- formatting
- build
- tests
- static analysis
- documentation sanity
- packaging

CI must not require proprietary reference files.

Reference-dependent tests are separate.

---

# 52. SECURITY

Never execute arbitrary extracted APK content during build.

Sanitize extracted paths.

Do not execute unknown scripts from assets.

Do not transmit analysis externally without explicit authorization.

---

# 53. CLEAN-ROOM ENGINEERING PRINCIPLE

Maintain a separation between:

```text
reference observation
        ↓
behavioural specification
        ↓
native implementation
```

The final codebase should be understandable without reading decompiler output.

---

# 54. AUTOMATION

If a manual operation happens three times, consider automating it.

Good automation targets:

- APK inventory
- builds
- tests
- profiling
- documentation checks
- changelog preparation
- release validation
- Git sanity checks

Automation must not hide destructive actions.

---

# 55. PROGRESSION GATES

## Gate A — Environment

All critical tooling works.

## Gate B — Understanding

Engine, loop, renderer, and major subsystems mapped.

## Gate C — Native Runtime

Wayland window, renderer, input, update loop, assets.

## Gate D — Gameplay

Player, weapon, enemy, collision, death/restart, room.

## Gate E — Content

Major discovered content catalogued and implemented or explicitly marked.

## Gate F — Release

Build, tests, Wayland smoke test, performance, docs, changelog, tag.

---

# 56. FIRST PLAYABLE

Definition:

- native Wayland launch
- visible game scene
- player movement
- attack
- enemy
- collision
- damage
- enemy death
- player death
- restart
- clean exit

It does not mean full parity.

---

# 57. CONTENT TRACKING

Track every discovered:

- character
- weapon
- enemy
- boss
- item
- room
- status effect
- UI screen

Each has:

```text
discovered
implemented
tested
verified
```

---

# 58. DEBUG MODE

Provide developer diagnostics where useful:

- FPS
- frame-time graph
- entity count
- room ID
- RNG seed
- collision shapes
- AI state
- player state
- current weapon
- resource statistics

Debug functionality must be disabled or removable from release builds.

---

# 59. LOGGING

Use levels:

```text
TRACE
DEBUG
INFO
WARN
ERROR
FATAL
```

Startup should report:

- version
- commit
- build type
- renderer
- Wayland backend
- resolution
- audio backend
- save path

Never swallow initialization errors.

Subsystem prefixes:

```text
[APP]
[WAYLAND]
[RENDER]
[AUDIO]
[INPUT]
[GAME]
[AI]
[SAVE]
[ASSET]
[RE]
[PERF]
```

---

# 60. CRASH DIAGNOSTICS

Debug builds should provide:

- stack trace where practical
- build version
- commit
- platform
- renderer
- relevant subsystem state

Do not upload reports automatically.

---

# 61. MEMORY / RESOURCE MANAGEMENT

C++:

- RAII
- smart pointers
- explicit ownership

Rust:

- ownership/borrowing
- minimal justified `unsafe`

Centralize resource lifetime:

- textures
- shaders
- sounds
- fonts

Avoid repeated loads.

---

# 62. THREADING

Do not add threads without a measurable reason.

Possible model:

- main/game thread
- render thread only if required
- audio backend
- background asset loading

Threading changes require correctness testing.

---

# 63. GAMEPLAY IMPLEMENTATION ORDER

Recommended first playable:

1. Wayland window
2. renderer
3. frame timing
4. input
5. assets
6. camera
7. player state
8. movement
9. collision
10. weapon
11. projectile
12. enemy
13. damage
14. death
15. room
16. room transition
17. basic UI
18. audio
19. save/restart

Then fidelity work.

---

# 64. FIDELITY ORDER

Prioritize:

1. timing
2. movement
3. collision
4. weapon mechanics
5. enemy mechanics
6. generation
7. characters
8. items
9. progression
10. bosses
11. UI
12. audio
13. visual polish

---

# 65. VISUAL FIDELITY

Compare:

- sprite dimensions
- pivots
- animation timing
- camera movement
- particles
- effects
- UI scale
- fonts
- blending
- transitions

Do not improve original behaviour unless explicitly decided.

---

# 66. DEBUGGING METHOD

For bugs:

1. reproduce
2. minimize
3. observe
4. locate boundary
5. inspect state
6. compare reference
7. write regression test
8. fix
9. rerun

Do not repeatedly patch symptoms without understanding the state transition.

---

# 67. MASTER AGENT BEHAVIOUR

Nemotron is the lead engineering agent.

It should:

- inspect before changing
- maintain project state
- reason from evidence
- implement incrementally
- build
- test
- document
- commit coherent milestones

It should NOT:

- fabricate analysis
- pretend tools succeeded
- mass-port before understanding architecture
- delete evidence
- silently change scope
- add X11
- add Android runtime
- modernize the 2.6.8 target
- claim parity without tests
- push to an unverified remote
- commit proprietary reference files

---

# 68. SESSION START PROCEDURE

Run:

```bash
git status
git log -5 --oneline
```

Read:

```text
docs/PROJECT_STATE.md
docs/ROADMAP.md
docs/BUGS.md
docs/PARITY.md
```

Then:

1. determine current phase
2. inspect latest changes
3. identify highest priority blocker
4. gather evidence
5. make a small plan
6. implement
7. build
8. test
9. document
10. commit when coherent

---

# 69. SESSION END PROCEDURE

Before ending a major session:

```text
build
test
git diff --check
```

Then:

- update PROJECT_STATE
- update relevant docs
- update CHANGELOG when appropriate
- commit coherent work
- record blockers
- record exact next action

---

# 70. INITIAL GIT HISTORY

Recommended actual history:

```text
chore: initialize native CachyOS project structure
reverse: record immutable APK artifact metadata
reverse: inventory APK manifest and package structure
reverse: map DEX architecture
reverse: inventory native libraries
reverse: document rendering and engine architecture
```

Do not create empty commits merely to satisfy this list.

---

# 71. INITIAL TAGS

After real bootstrap:

```text
v0.0.1-bootstrap
```

After complete reconnaissance:

```text
v0.1.0-recon
```

Only create a tag when its milestone is genuinely complete.

---

# 72. PHASE 0 IMMEDIATE ACTIONS

Do NOT mass-implement.

First:

1. inspect repository
2. verify CachyOS
3. verify Wayland
4. verify compiler
5. verify build tools
6. verify Git
7. verify Gemini/agent environment
8. verify REA
9. verify REA integration
10. verify Ghidra
11. verify JADX
12. verify Apktool
13. verify ADB
14. locate APK
15. hash APK
16. initialize Git if necessary
17. create `.gitignore`
18. create baseline docs
19. inventory APK
20. identify likely engine
21. identify Java/native split
22. identify renderer
23. identify major subsystems

Stop and report only after actual inspection.

---

# 73. PHASE 1 REQUIRED DELIVERABLES

Must eventually exist:

```text
analysis/original-apk.sha256
analysis/ARTIFACT.md
analysis/APK_INVENTORY.md
analysis/class-map.md
analysis/native-libraries.md
analysis/resources.md
docs/ARCHITECTURE.md
docs/REVERSE_ENGINEERING.md
docs/PROJECT_STATE.md
docs/ROADMAP.md
docs/TECHNOLOGY_DECISIONS.md
docs/PLATFORM.md
```

These must contain actual findings.

---

# 74. ACCEPTANCE FOR v1.0.0

Required:

- native CachyOS executable
- native Wayland runtime
- no Android runtime
- stable launch
- 1920×1080
- stable 60 FPS target
- keyboard
- mouse
- gamepad where practical
- player
- combat
- weapons
- enemies
- rooms
- generation
- items
- progression
- characters
- bosses
- UI
- audio
- saving/loading
- major mechanics
- regression tests
- performance measurements
- reproducible build
- documentation
- changelog
- release tag

Anything missing must be explicitly documented rather than hidden.

---

# 75. THE CORE RULE

**Never optimize for the appearance of progress. Optimize for accumulated engineering truth.**

Every discovery should make the next step easier.

Every implementation should be testable.

Every major decision should be documented.

Every important bug should become a regression test.

Every release should be reproducible.

Every Git commit should tell the truth about what changed.

The final repository should be understandable to another engineer who has never
spoken to Nemotron.

---

# 76. FIRST INSTRUCTION TO NEMOTRON

Read this entire file.

Then execute Phase 0.

Do not start mass implementation.

Do not guess the engine.

Do not guess the architecture.

Do not create fake analysis.

Inspect the real environment and real APK first.

Your first meaningful report must state:

```text
Platform:
Wayland:
Compiler:
Build system:
Git:
REA:
Ghidra:
JADX:
Apktool:
ADB:

APK path:
APK SHA-256:
Package:
Version:

Likely engine:
Java/native split:
Renderer:
Major subsystems discovered:

Current phase:
Current blocker:
Next action:
```

Then continue autonomously through the roadmap while respecting all gates,
documentation requirements, Git discipline, and Wayland/CachyOS-only scope.
