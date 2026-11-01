# LiteKnight

**Native Linux development executable for the Soul Knight reconstruction.**

LiteKnight is a native C++20/SDL3/Vulkan project built around a permanent,
playable **developer room**. The executable is not a mock-up: movement,
collision, camera, animated doors, obstacle switching, character switching,
weapon fire, projectiles, damage, enemy death, player death, reset, live enemy
pursuit, energy/HUD, two weapon slots, weapon pickup and save/load all run
through the native fixed-step simulation. A 1920x1080 logical pixel UI,
recovered pixel_bold font, pause/settings/main menu, Living Room hub, character
and weapon stations, a three-room dungeon graph, 360-degree mouse aiming,
auto-aim, weapon energy consumption, Knight Dual Wield and native save/load are
part of the same executable. The asset laboratory and serialized interactive
test harness remain available.

- **Version:** `0.5.5-dev`
- **Platform:** Linux, Wayland only, Vulkan 1.3+
- **Reference:** Soul Knight 2.6.8 / Unity 2018.4.12f1 (local reference only)
- **Simulation:** 50 Hz fixed step; 60 FPS render target (FIFO VSync,
  measured present pacing in the run log)
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
assets, and launches the native main menu. It stops before launch if tests fail.

Useful variants:

```bash
./scripts/dev.sh --no-build
./scripts/dev.sh --no-tests
```

### Individual commands

```bash
./scripts/build-dev.sh        # configure + build
./scripts/test.sh             # CTest
./scripts/run-devroom.sh      # launch devroom only
./scripts/play.sh             # main menu -> Living Room
./scripts/smoke-devroom.sh    # non-interactive playable check
./scripts/prepare-assets.sh   # full local reference/native preparation
./scripts/audit-assets.sh     # metadata + generated-pixel audit
./scripts/test-interactive.sh --list
./scripts/play-area.sh main_menu         # main menu until normal exit
./scripts/play-area.sh living_room       # Living Room until normal exit
./scripts/play-area.sh workshop          # hub workshop area
./scripts/play-area.sh garden            # planter garden
./scripts/play-area.sh chest             # three recovered chest types
./scripts/play-area.sh weapons           # weapon catalogue selector
./scripts/smoke-hub.sh                   # non-interactive hub invariant smoke
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
tools/asset/.venv/bin/python tools/asset/build_item_assets.py
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
- All 108 recovered PatternRooms (15×15, 15×21, 21×15, 21×21) plus eight
  special rooms (start, transfer gate, chest, weapon provider, statue, mineral,
  mercenary, mount) and the Living Room hub
- Deterministic `--map-seed` assignment of actual rooms over a recovered
  5×5 MapManagerLevel relation grid with real door entrances
  graph; same seed produces the same room graph
- Animated doors with collision synchronization and automatic walk-through
  transitions at the matching destination entrance
- Room-clear and per-room enemy/clear state preservation across visits
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
- Two independent weapon slots (`1`/`2`/`Tab`), per-slot projectiles/cooldowns
- Recovered chest00 and RGCoin world items; E opens a chest, coin_0/1/2 use
  serialized values 5/3/1, coins magnet/collect into the HUD wallet
- Persistent logical HUD layout (`$XDG_CONFIG_HOME/liteknight/ui_layout.json`)
  with Settings -> UI Design Mode, mouse drag, save and reset
- Compact always-visible minimap reconstructed from the actual `UIMap`
  hierarchy: tinted reference cells/connectors, recovered home/boss/special/
  chest/cage icons, `ui_4` current marker and a white `D` devroom cell; no
  oversized fullscreen map
- Recovered UI construction: main-menu `biaoti 01/02` backdrop/swirl, actual
  button/panel/ribbon frames, `ui_15` HUD plate and `ui_12` bar fills, actual
  skill lightning (`ui_joy_3`), pickup card (`ui_191`), pause/settings assets
- Weapon catalogue: 302 entries loaded at runtime, with 210 Tier A
  projectile/melee weapons fully spawnable and 14 melee / 78 unique entries
  explicitly classified
- Recovered weapon animation curves, muzzle flash, per-weapon audio, baseline
  loadouts for all 15 heroes, and Knight dual wield
- Nine chest prefabs, room chest/coin persistence, breakable RGBox boxes and
  RGEnergy bubbles
- Recovered pets (follow/attack/menu/save), Living Room NPC speech, the
  EnemyAIStake punching bag, workshop material recipes and crafting, and the
  exact `map_info`/`show_signpost` room UI animations
- Chest catalogue: 13 reference classes / 86 GameObjects audited; chest00/01/02
  integrated with actual visuals, 1.0 s open animation and reference-derived
  weapon loot
- Living Room default active hierarchy, physically reachable workshop/garden/
  cellar and the actual transfer_gate_02 return portal
- Directional body-facing separation, stabilized in-hand weapon rendering and
  blue Knight ability charge fill
- Direct developer-area launcher (`scripts/play-area.sh`) for main
  menu, settings, pause, Living Room, r1_1/r1_2/r1_3, UI Design Mode and
  fullscreen
- Main menu defaults for the test executable, LIGHT KNIGHT wordmark and D-ROOM
  developer entry
- Recovered per-attack energy costs: `weapon_000=0`, `weapon_002=1`,
  `weapon_048=1`, `weapon_287=1`; insufficient energy rejects the attack
- 360° mouse aim independent of WASD movement, aim-driven body flip with
  hysteresis, and a Manual/Auto Aim setting persisted in save v2
- Auto Aim selects the nearest alive visible target with wall line-of-sight;
  the weapon returns to the held pose when not firing (HYPOTHESIS)
- Knight c00 Dual Wield on `Q`: 5.0 s active, recovered 10.0 s cooldown,
  off-hand weapon presentation/fire and HUD cooldown state
- World weapon pickup foundation with recovered weapon sprites (`G`/`E`)
- Asset laboratory (`--asset-lab`, `F5`) with category/item cycling and playback
- Serialized interactive tests and deterministic replay (`scripts/test-interactive.sh`)
- Versioned native save/load at `$XDG_DATA_HOME/liteknight/saves/` (`F2`/`F3`)
- 1920x1080 logical 16:9 coordinate contract with framebuffer letterboxing
- Recovered `pixel_bold` bitmap font rasterized at 28 px, with measured/centered
  text, shadow/outline and pixel snapping
- Top-left HP/armor/energy HUD using recovered heart/shield/crystal sprites,
  recovered weapon slots, pause and currency icons
- Pause, Settings, Main Menu and character/weapon station screens using recovered
  button frames/icons; recovered title logo
- Recovered Living Room (`level2`/HeroRoom) with tilemap floor, NPCs, furniture,
  collision, character station, forge station, garden plots and hub door
- Per-room state-preserving transitions between the Living Room and three
  recovered dungeon rooms
- Bordered fullscreen (`F11`) with integer-scale preference and 16:9 letterbox
- Empty equipment state, compact E pickup card with damage/energy stats, two
  slots and `Tab` switching
- Confirmed passive energy regeneration: +1 every 2.0 s while below maximum
- Room-clear flow: doors close on entry, open when all room enemies are dead
- Idle/run/death character clips with render interpolation and hit flash
- Versioned save v2 with hub/settings/progression fields
- Deterministic tests, Debug ASan/UBSan build, Vulkan validation support

Known limitations: full EnemyAI04 scouting/pathfinding, the reference
`MapManagerLevel.CreateMap/CreateRoom` algorithm, production forge recipes/plant
economy, full audio/effects/items and most of the recovered catalogue are not
integrated. The native room assignment is deterministic but is not a
reconstruction of reference procedural generation; 3 of 108 discovered
PatternRooms are integrated. Auto-aim target priority and the body-flip
threshold are documented HYPOTHESES. UI panel frames remain native
approximations where exact sprite mapping is UNKNOWN. See
[project state](docs/PROJECT_STATE.md) and [known gaps](docs/ROADMAP.md). Chest/coin state is session-scoped; the wallet is
saved, but opened-chest and remaining-world-coin state requires save v3.

---

## Devroom controls

| Input | Action |
|---|---|
| `WASD` / arrows | move |
| `J`, `Space`, left mouse | attack (native test binding) |
| mouse / right stick / keypad `8,2,4,6` | 360° aim; body flips from aim, not movement |
| `Q` | Knight c00 Dual Wield power (5 s active / 10 s cooldown) |
| `Tab`, `1`, `2` | switch weapon slot / select primary / select secondary |
| `F2` / `F3` | save / load native slot 0 |
| `Esc` | pause/resume; settings closes back to pause first |
| `F4` | collision/visual/sort debug bounds |
| `T` | developer cycle through a connected room |
| `N` | teleport to current room entrance |
| walk through an open door | automatic physical room transition |
| `H` / `B` | character station / weapon station (hub) |
| `F5` | asset laboratory |
| `F6` | character selector |
| `F7` | weapon selector |
| `F8` | enemy selector |
| `F9` / `Shift+F9` / `F12` | spawn one / spawn five / clear enemies |
| `F10` | developer overlay (logical/framebuffer/scale) |
| `F11` | toggle bordered fullscreen |
| `E` / `G` | interact/open chest/pick up / spawn weapon pickup |
| `Ctrl+1..9`, `Ctrl+0`, `[` / `]` | legacy character shortcuts |
| `Y` | cycle solid recovered obstacle variant |
| `O` / `C` | toggle / close animated doors |
| `R` | deterministic dev reset |

Settings -> `UI DESIGN MODE` opens the mouse layout editor: drag HUD elements,
`S` saves, `R` resets defaults, `ESC` saves and returns. Layout coordinates are
logical 1920x1080 and persist in `$XDG_CONFIG_HOME/liteknight/ui_layout.json`.
`--map-seed <int>` reproduces the native room assignment. Selectors use `A/D` or
arrows to navigate and `Enter` to confirm. Character selection preserves the
player position, collision, camera, and combat state.

---

## Tests and verification

```bash
./scripts/test.sh
ctest --test-dir build --output-on-failure
./scripts/smoke-devroom.sh
```

Current baseline: **132 test cases / 5113 assertions** (RelWithDebInfo and Debug
ASan/UBSan), three CTest entries including the Living Room audit, Vulkan
validation clean, and interactive replay reproduces recorded combat state
exactly. The counts in `docs/TESTING.md` are authoritative for the exact commit.

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
- [Native pixel UI](docs/development/UI.md)
- [UI reconstruction evidence](docs/development/UI_RECONSTRUCTION.md)
- [Weapon catalogue](docs/development/WEAPON_CATALOGUE.md)
- [Weapon system](docs/development/WEAPON_SYSTEM.md)
- [Weapon attachment](docs/development/WEAPON_ATTACHMENT.md)
- [Chest system](docs/development/CHEST_SYSTEM.md)
- [Room types and stage labels](docs/development/ROOM_TYPES.md)
- [Map generation](docs/development/MAP_GENERATION.md)
- [Biomes](docs/development/BIOMES.md)
- [Living Room hub](docs/development/HUB.md)
- [Character animation](docs/development/CHARACTER_ANIMATION.md)
- [Knight ability UI](docs/development/ABILITY_UI.md)
- [Trader and orbs](docs/development/TRADER_SYSTEM.md)
- [Audio](docs/development/AUDIO_SYSTEM.md)
- [Enemy catalogue](docs/development/ENEMY_CATALOGUE.md)
- [Save/load](docs/development/SAVE_LOAD.md)
- [Compact minimap](docs/development/MAP.md)
- [Living Room topology audit](../analysis/assets/hub_topology.md)
- [Living Room](docs/development/LIVING_ROOM.md)
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
