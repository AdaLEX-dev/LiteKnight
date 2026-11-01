# 00 — Operating Manual

## 1. What this project is

LiteKnight is a native Linux reconstruction of the Soul Knight 2.6.8 vertical
slice. It is not an emulator, not an Android runtime, and not a mod loader. The
runtime is C++20 with SDL3 for the window/input/audio boundary and Vulkan 1.3 for
rendering. Gameplay data is compiled from metadata and generated scene JSON.

The project has two layers of truth:

1. **Recovered reference facts** — what the Unity 2018.4.12f1 build actually
   contains. These live under `analysis/re/` and `analysis/assets/`.
2. **Native implementation** — what LiteKnight actually does. This lives under
   `src/` and is exercised by `tests/` and `tools/qa/`.

Do not confuse them. A recovered fact does not mean the native runtime implements
it, and a native implementation does not mean the reference behavior is known.

## 2. Environment

Known working environment:

- CachyOS / Arch Linux, x86-64
- Wayland compositor (the application refuses X11/XWayland)
- Vulkan 1.3-capable driver
- CMake 3.25+, Ninja, GCC or Clang with C++20
- SDL3 3.2+
- Vulkan loader and headers, `glslc`
- libpng, fmt
- Python 3 with the local `tools/asset/.venv`
- `rsync` for asset preparation
- `gh` for GitHub release operations

The project uses SDL3 through CMake FetchContent/dependency discovery depending
on the configured tree. See `CMakeLists.txt` and `docs/development/BUILD.md`.

## 3. Canonical directories

| Path | Meaning |
|------|---------|
| `build/` | Current configured RelWithDebInfo build and the canonical `build/soul-knight` executable. |
| `build-debug/` | Debug/ASan/UBSan build if configured. |
| `build-release/` | Older Release build. Its `soul-knight` may be a stale synthetic-scene binary; do not treat it as canonical without checking its timestamp and behavior. |
| `assets/generated/` | Local generated reference pixels. Gitignored. |
| `analysis/apktool_out/` | Local APK extraction. Gitignored/not redistributed. |
| `original/` | Local reference APK. Do not commit or publish. |
| `analysis/assets/` | Committed metadata, scene JSON, manifests, catalogues. |
| `src/` | Native source. |
| `tests/` | Unit and tool tests. |
| `tools/` | Asset builders, audits, QA scripts, reverse-engineering helpers. |
| `docs/` | Human documentation. |

## 4. First-time setup

```bash
git clone https://github.com/AdaLEX-dev/LiteKnight.git
cd LiteKnight
```

Place the local reference APK at:

```text
original/SoulKnight-2.6.8.apk
```

Extract it locally using the project scripts/tools. The expected local tree is
`analysis/apktool_out/`. Do not commit that tree.

Create/refresh the Python asset environment if needed:

```bash
python3 -m venv tools/asset/.venv
tools/asset/.venv/bin/pip install -r tools/asset/requirements.txt
```

Build the native code:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
```

Or use the project wrapper:

```bash
./scripts/build-dev.sh
```

## 5. Everyday commands

```bash
# Configure + build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"

# Prepare local runtime assets from the reference tree
./scripts/prepare-assets.sh

# Run all unit tests
./build/soul-knight-test

# Run CTest
ctest --test-dir build --output-on-failure

# Run the strict Living Room coordinate regression
python3 tools/qa/living_room_coordinate_regression.py

# Run the trader A/B/A stock regression
python3 tools/qa/trader_stock_regression.py

# Launch the Living Room directly
./build/soul-knight --hub

# Launch the main menu
./build/soul-knight --start-gui

# Launch the dungeon devroom
./build/soul-knight --devroom

# Run the automated hub -> dungeon -> clear -> chest -> save/load smoke
./build/soul-knight --gameplay-smoke --smoke-seconds 25

# Run a bounded window/Vulkan smoke
./build/soul-knight --smoke-seconds 3

# Run geometry probes
./build/soul-knight-geometry --area living-room
./build/soul-knight-geometry --area coordinate-regression

# Run one review area
./build/soul-knight --hub --review-area rogue
```

`scripts/dev.sh` is the one-command convenience path: build, test, prepare
assets, launch. Use `--no-build`, `--no-tests`, or the individual scripts when
you need a narrower loop.

## 6. Build configurations

CMake presets exist in `CMakePresets.json`:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug-tests

cmake --preset linux-relwithdebinfo
cmake --build --preset linux-relwithdebinfo
```

The presets all use `build/` except the Clang preset (`build-clang/`). If you
need parallel Debug and Release trees, configure them explicitly with different
`-B` directories; do not assume `build-debug/` matches the current source.

### Debug / sanitizers

The Debug configuration enables validation-layer requests when available.
Useful commands:

```bash
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j"$(nproc)"
ctest --test-dir build-debug --output-on-failure

# Direct ASan run
ASAN_OPTIONS=detect_leaks=1 ./build-debug/soul-knight --hub --smoke-seconds 3
```

If ASan reports a leak in SDL/Vulkan/audio driver code, check whether the same
report appears in a plain `--smoke-seconds 1` run before chasing engine code.

## 7. CLI modes

`AppConfig` is in `src/app/AppConfig.hpp`; parsing is in
`src/app/AppConfig.cpp`. Unknown arguments are ignored.

| Argument | Behavior |
|----------|----------|
| `--devroom` | Load `scenes/r1_1_visual/scene.json` and start in the dungeon devroom. |
| `--hub` | Load `scenes/hero_room/scene.json` directly (Living Room). |
| `--start-gui` | Load the Living Room and start at the native main menu. |
| `--scene <path>` | Load an explicit scene JSON relative to the asset root. |
| `--asset-lab` | Enter the recovered asset laboratory. |
| `--gameplay-smoke` | Run the automated hub/dungeon/chest/save loop. |
| `--smoke-seconds <n>` | Exit after `n` wall-clock seconds. |
| `--map-seed <int>` | Deterministic dungeon graph and trader stock seed input. |
| `--review-area <name>` | Teleport/open a developer review state. |
| `--area <name>` | Alias for `--review-area`. |
| `--fullscreen` | Start fullscreen. |
| `--exercise-window` | Resize/minimize/restore during a smoke test. |
| `--locomotion-debug` | Log fixed-step movement/input/collision state. |
| `--interactive-test=<id>` | Run a serialized interactive test. |
| `--replay <json>` | Replay a recorded input stream. |
| `--texture <path>` | Load a specific texture in test/smoke paths. |

Important review areas for the current pass:

```text
living_room   main_menu   settings   pause   ui_design   fullscreen
workshop      garden      cellar     trader  trader-world
mech          mech-ui     reforging  planting
chest         weapons     r1_1       r1_2       boss-room
sign1         mission-board         weapon-chest
table-characters       stair     portal     sign2
table17       object27    npc-grave  table31    table35
table39       rogue       sign3      wall-short invisible-wall
oxygen        material-chest-ui     weapon-station-ui
weapon-station-locked  trader-ui  mech-ui-live
```

The named coordinate review areas teleport the player and, for station areas,
open the matching UI. The geometry probe's `coordinate-regression` area simply
runs the Python coordinate suite.

## 8. Asset refresh is mandatory after builder edits

If you edit any file in `tools/asset/`:

```bash
./scripts/prepare-assets.sh
cmake --build build -j"$(nproc)"
```

If the builder you changed is gated by a cache check in `prepare-assets.sh`,
run that builder directly, then run `prepare-assets.sh` again. Example:

```bash
tools/asset/.venv/bin/python tools/asset/build_item_assets.py
tools/asset/.venv/bin/python tools/asset/build_hub_scene.py
./scripts/prepare-assets.sh
```

The most common “the fix did nothing” failure is a stale runtime copy under
`build/assets/scenes/...`.

## 9. Code layout

```text
src/app/                 Application lifecycle, CLI, review modes, world flow
src/platform/            Wayland/SDL window, input, audio, timing, filesystem
src/engine/math/         Vec2, Rect, random streams, fixed-step helpers
src/engine/collision/    AABB world and movement resolution
src/engine/locomotion/   Player movement/facing
src/engine/assets/       SceneAsset model, SceneLoader, PNG decoding
src/engine/animation/    Locomotion animation sampling
src/engine/rendering/    Camera, viewport, sprite batch, Vulkan renderer
src/game/player/         Characters, health, energy, aim, abilities
src/game/combat/         Weapon behavior, projectiles, damage
src/game/enemies/        Enemy definitions, instances, AI
src/game/room/           Rooms, doors, obstacles, biomes, dungeon plan
src/game/items/          Chests, coins, item assets, pickups, sellers, boxes
src/game/hub/            Workshop, plants, training stake
src/game/pet/            Pet system
src/game/summon/         Summons
src/game/save/           Versioned native save schema/serializer/manager
src/game/ui/             HUD, menus, station UIs, layout, UI renderer
src/game/weapons/        Registry, inventory, catalogue, pickups, renderer
src/tools/               Standalone gallery/PNG tools
tests/unit/              Doctest unit suite
tests/tools/             Geometry/area probe executable
tools/asset/             Reference extraction and scene/asset builders
tools/qa/                Strict coordinate and runtime QA scripts
```

Dependency direction: `app -> game -> engine -> platform` but in practice the
Application composes all of them. Engine code must not call SDL/Vulkan directly;
`Application` owns the platform/renderer lifecycle. Gameplay code must not own
the window or swapchain.

## 10. Coding conventions

- C++20, 4-space indentation, 80-column-ish style in existing files.
- Namespaces follow the directory: `soulknight::game::items`,
  `soulknight::engine::assets`, etc.
- Prefer explicit types and `std::` over `using namespace`.
- Keep deterministic simulation in `FixedUpdate`; rendering-only work belongs in
  `BuildRenderBatch`/UI draw code.
- Do not introduce nondeterminism into gameplay without a seed or explicit
  rationale. `RGRandomStream` is the recovered deterministic RNG.
- Use `LOG_INFO_APP`, `LOG_WARN_APP`, `LOG_ERROR_APP`, `LOG_FATAL_APP` rather
  than `printf`.
- Booleans that appear in logs must be formatted with `BooleanText()` in
  `Application.cpp` where that convention exists.
- UI rectangles are logical 1920x1080 coordinates. The viewport maps them.
- Asset/scene builders should write deterministic JSON (sorted keys where the
  existing builder does).
- New source files must be added to the correct static library in
  `CMakeLists.txt`.
- New tests must be added to `tests/unit` and the `soul-knight-test` target, and
  new QA scripts should be registered as CTest tests when practical.

## 11. How to make a change safely

1. **Reproduce** the behavior with a bounded command. For hub work, use
   `--hub --review-area <target>` or the geometry probe.
2. **Locate the data** in `analysis/assets/scenes/hero_room.json`, the builder,
   or the runtime code. Do not edit `build/assets` by hand.
3. **Fix the source of truth.** For scene geometry that is
   `tools/asset/build_hub_scene.py`, followed by `prepare-assets.sh`.
4. **Add or update a regression test** at the coordinate/behavior level, not just
   a generic unit test.
5. **Run**:
   ```bash
   ./build/soul-knight-test --test-case="<relevant pattern>"
   python3 tools/qa/living_room_coordinate_regression.py
   ctest --test-dir build -R "liteknight-(living-room-coordinate-regression|gameplay-smoke)" --output-on-failure
   ```
6. **Inspect visually** with `--review-area` and a screenshot when the issue is
   visual. Geometry tests do not prove pixel correctness.
7. **Commit** only after assets are refreshed and tests are green.

## 12. Debugging the running application

- Logs are written to `$XDG_CACHE_HOME/soul-knight/logs` (normally
  `~/.cache/soul-knight/logs`) and also to stdout.
- The startup log prints asset resolution, viewport contract, camera, scene
  load, characters, obstacles, combat configuration, item assets, audio, and
  save path.
- `--locomotion-debug` adds fixed-step movement/collision logs.
- `--smoke-seconds` gives a bounded run; use it before interactive testing.
- `--review-area` is the fastest way to reach a specific object/station.
- For window/viewport issues, look for `Window geometry event`,
  `Viewport contract`, `Swapchain`, and `Viewport mouse mapping` lines.
- For scene issues, look for `Runtime scene`, `Scene loaded`, `Instances`,
  `Sprites`, and `Level surfaces`.
- For save issues, look for `Native save slot`, `Saved native game state`, and
  `Save failed`.

## 13. Debugging tests

```bash
# All tests, no breaks
./build/soul-knight-test --no-breaks

# One test case pattern
./build/soul-knight-test --test-case="*Rogue starter*" --reporter=console

# List test names
./build/soul-knight-test --list-test-cases

# CTest with output
ctest --test-dir build --output-on-failure -R <regex>

# Geometry area
./build/soul-knight-geometry --area rogue
```

If a unit test builds but fails with old expectations, inspect whether the
behavior intentionally changed. Update the test to the new invariant, do not
delete the test. If the test is genuinely obsolete, explain why in the commit.

## 14. Release checklist

See [01-GIT-GITHUB-RELEASES.md](01-GIT-GITHUB-RELEASES.md). Minimum:

```bash
./scripts/prepare-assets.sh
cmake --build build -j"$(nproc)"
./build/soul-knight-test
python3 tools/qa/living_room_coordinate_regression.py
python3 tools/qa/trader_stock_regression.py
ctest --test-dir build --output-on-failure
git status
```

Do not tag if the tree is dirty or if the canonical binary is not the one you
tested.

## 15. What “done” means here

A task is done when:

- the source of truth (builder/runtime/data) is changed, not the copied runtime
  asset;
- the behavior is reproducible from a clean asset preparation;
- a coordinate/behavior-specific test exists or a manual visual report is
  attached;
- the release build still compiles;
- the change is committed with a message that states the user-visible outcome.

A green generic unit suite is not enough. The previous Living Room passes
demonstrated that thousands of assertions can coexist with wrong coordinates.
