# LiteKnight

**Native Linux development executable for the Soul Knight reconstruction.**

LiteKnight is a native C++20/SDL3/Vulkan project built around a permanent,
playable **developer room**. The executable is not a mock-up: movement,
collision, camera, animated doors, obstacle switching, character switching,
weapon fire, projectiles, damage, enemy death, player death, reset, live enemy
pursuit, energy/HUD, two weapon slots, weapon pickup and save/load all run
through the native fixed-step simulation. The asset laboratory and serialized
interactive test harness are part of the same executable.

- **Version:** `0.2.0-dev`
- **Platform:** Linux, Wayland only, Vulkan 1.3+
- **Reference:** Soul Knight 2.6.8 / Unity 2018.4.12f1 (local reference only)
- **Simulation:** 50 Hz fixed step; 60 FPS render target
- **License:** native source MIT; reference assets **not** redistributed

LiteKnight is **not** an official Soul Knight build, not affiliated with
ChillyRoom, and not a complete game. See [LEGAL.md](LEGAL.md).

---

## Quick start

### Requirements

- Linux with a Wayland compositor
- Vulkan 1.3-capable GPU/driver
- CMake 3.25+, Ninja, GCC/Clang with C++20
- SDL3 3.2+, Vulkan loader/headers, libpng, fmt, glslc
- Python 3 + `rsync` for local asset preparation
- A locally generated native scene asset tree (see below)

### One command

```bash
./scripts/dev.sh
```

This configures/builds, runs the deterministic tests, prepares the local scene
assets, and launches the native devroom. It stops before launch if tests fail.

Useful variants:

```bash
./scripts/dev.sh --no-build
./scripts/dev.sh --no-tests
```

### Individual commands

```bash
./scripts/build-dev.sh        # configure + build
./scripts/test.sh             # CTest
./scripts/run-devroom.sh      # launch only
./scripts/smoke-devroom.sh    # non-interactive playable check
./scripts/prepare-assets.sh   # full local reference/native preparation
./scripts/audit-assets.sh     # metadata + generated-pixel audit
./scripts/test-interactive.sh --list
```

The built launcher is:

```text
build/bin/liteknight-devroom
```

It is a thin launcher for the tested `build/soul-knight` executable and defaults
to `--devroom`.

---

## Local native assets

The repository deliberately does **not** commit the proprietary Unity bundle
tree, original APK, extracted pixels, or generated PNGs. To run the devroom with
recovered visuals, provide a local reference tree and generate the native scene:

```bash
# With the local reference bundle tree available:
tools/asset/.venv/bin/python tools/asset/build_visual_scene.py
rsync -a --delete assets/generated/visual/r1_1_visual/ \
    build/assets/scenes/r1_1_visual/

# Or let the scripts copy/generate it:
./scripts/prepare-assets.sh
```

`scripts/build-dev.sh` and `scripts/dev.sh` call the preparation step. If no
generated scene or local reference tree exists, they stop with an explicit
message rather than silently launching a broken room.

Metadata, hashes, scene topology, inventory records, and tests are
source-controlled. Generated pixels remain gitignored. See
[assets pipeline](docs/assets/PIPELINE.md) and [legal boundary](docs/legal/).

---

## Current playable features

- Native Wayland/Vulkan window, swapchain, resize/minimize/restore
- 50 Hz deterministic player movement and swept AABB collision
- Recovered 15×15 `r1_1` room, wall/corridor topology, floor, obstacles, doors
- Animated doors with collision synchronization
- All 15 recovered character IDs (`c00..c14`) with recovered body/dead sprites
  and idle/run clips
- Four recovered `Gun001` weapon prefabs with real weapon textures:
  `weapon_000`, `weapon_002`, `weapon_048`, `weapon_287`
- Real recovered projectile sprites and swept collision
- Explicit `e_slime01` fixtures, enemy HP/damage/death, contact damage
- Player HP/armor/damage/death and developer reset
- Native developer overlay, character selector, weapon selector, enemy selector,
  and enemy spawner
- Live `e_slime01` pursuit and contact combat with multi-enemy spawn/reset
- Player HP/armor/energy HUD driven by gameplay state
- Two independent weapon slots (`1`/`2`), per-slot projectiles/cooldowns/energy
- World weapon pickup foundation with recovered weapon sprites (`G`/`E`)
- Asset laboratory (`--asset-lab`, `F5`) with category/item cycling and playback
- Serialized interactive tests and deterministic replay (`scripts/test-interactive.sh`)
- Versioned native save/load at `$XDG_DATA_HOME/liteknight/saves/` (`F2`/`F3`)
- Deterministic tests, Debug ASan/UBSan build, Vulkan validation support

Known limitations: full EnemyAI04 scouting/pathfinding, room transitions, start
GUI, hub, forge, plants, production UI/audio/items and most of the recovered
catalogue are not integrated. Room transitions and the hub are not claimed as
working. See [project state](docs/PROJECT_STATE.md) and
[known gaps](docs/ROADMAP.md).

---

## Devroom controls

| Input | Action |
|---|---|
| `WASD` / arrows | move |
| `J`, `Space`, left mouse | attack (native test binding) |
| mouse / right stick / keypad `8,2,4,6` | aim (native test binding; facing stays movement-driven) |
| `F2` / `F3` | save / load native slot 0 |
| `F5` | asset laboratory |
| `F6` | character selector |
| `F7` | weapon selector |
| `F8` | enemy selector |
| `F9` / `F11` / `F12` | spawn one / spawn five / clear enemies |
| `F10` | developer overlay |
| `1` / `2` | primary / secondary weapon slot |
| `E` / `G` | pick up / spawn weapon pickup |
| `Ctrl+1..9`, `Ctrl+0`, `[` / `]` | legacy character shortcuts |
| `P` | cycle solid recovered obstacle variant |
| `O` / `C` | toggle / close animated doors |
| `R` | deterministic dev reset |
| `Esc` | close selector / quit |

Selectors use `A/D` or arrows to navigate and `Enter` to confirm. Character
selection preserves the player position, collision, camera, and combat state.

---

## Tests and verification

```bash
./scripts/test.sh
ctest --test-dir build --output-on-failure
./scripts/smoke-devroom.sh
```

Current baseline: **115 test cases / 4983 assertions** (RelWithDebInfo and Debug
ASan/UBSan), Vulkan validation clean, real Wayland/Vulkan devroom smoke passes,
and interactive replay reproduces recorded combat state exactly. The counts in
`docs/TESTING.md` are authoritative for the exact commit.

The CI workflow builds/tests only source-controlled metadata and native code; it
does not require the proprietary reference bundle tree.

---

## Repository layout

```text
src/engine/       core math, assets, animation, rendering, collision
src/platform/     SDL3/Wayland window, input, filesystem, timing, audio scaffold
src/game/         characters, combat, enemies, player, rooms, UI, weapons
assets/           test assets and generated local asset output (pixels ignored)
analysis/         reverse-engineering evidence, scene metadata, inventories
tools/asset/      extraction, inventory, audit, translation pipeline
tools/re/         IL2CPP/Ghidra helpers
tests/unit/       deterministic native tests
scripts/          build/test/devroom workflow
docs/             architecture, development, assets, legal, releases
```

Development docs:

- [BUILD.md](docs/development/BUILD.md)
- [DEVROOM.md](docs/development/DEVROOM.md)
- [WORKFLOW.md](docs/development/WORKFLOW.md)
- [Asset laboratory](docs/development/ASSET_LAB.md)
- [Serialized interactive tests](docs/development/INTERACTIVE_TESTS.md)
- [DEVROOM architecture](docs/architecture/DEVROOM.md)
- [PIPELINE.md](docs/assets/PIPELINE.md)
- [INVENTORY.md](docs/assets/INVENTORY.md)
- [RELEASE_PROCESS.md](docs/releases/RELEASE_PROCESS.md)

---

## Reverse-engineering and parity

Evidence lives in `analysis/re/`; confidence labels are used deliberately:
CONFIRMED, HIGH CONFIDENCE, HYPOTHESIS, PARTIAL, UNKNOWN, CONTRADICTED. Native
approximations are identified in code and docs. The project does not claim
complete Soul Knight parity.

## Contributing

1. Keep the build/test/devroom workflow green.
2. Use real recovered textures where available; label unavoidable placeholders.
3. Record evidence and confidence for new behaviour.
4. Never commit reference binaries or proprietary pixels.
5. Add tests for deterministic behaviour and document asset provenance.

## Legal

Native source is MIT-licensed. Soul Knight and its assets are property of their
respective owners; this project is independent and not endorsed by ChillyRoom.
See [LICENSE](LICENSE), [NOTICE](NOTICE), [LEGAL.md](LEGAL.md), and
[docs/legal](docs/legal/).
