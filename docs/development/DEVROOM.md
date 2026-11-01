# Permanent Native Devroom

The devroom is the primary development environment. It must always launch after
a successful build and local asset preparation.

## Executable

```text
build/bin/liteknight-devroom
```

The launcher runs the tested `build/soul-knight --devroom` executable. The
`--devroom` flag selects:

```text
scene: scenes/r1_1_visual/scene.json
title: LiteKnight Devroom
```

The normal `soul-knight` executable remains available for synthetic smoke tests
and explicit `--scene` runs.

## Hub topology and physical routes

The Living Room is a single recovered `HeroRoom` scene with physically connected
subareas:

```text
Living Room / hall
├── weapon workshop / research (forge, mech, bookshelf)
├── planter garden (land_root, plant pots, bushes)
├── cellar / bottom room (blueprint table, decorations)
├── magic/protection area
└── battle-door marker
```

The reference scene's default active hierarchy is extracted; inactive event
objects/tilemaps (for example `chinesenewyear_wall_lobby`) are excluded. Wall
tile cells provide collision while the reference floor tilemaps define the
walkable openings. Progression-lock colliders around the cellar and portals are
removed for the explorable native hub. Furniture/BoxCollider2D props retain
their recovered collision where present.

In the dungeon graph, each room still has four recovered physical door objects.
After door configuration, any door with no matching `RoomManager` edge is
sealed: it stays closed and is skipped by `OpenAll`, so a map edge and a
traversable corridor always correspond. Rooms can only be entered/exited through
the doors listed in the generated graph.

The minimap adds native hub subarea nodes for garden/workshop/cellar on top of
the room graph. These are a native visualization of the single-scene subareas,
not separate `MapManagerLevel` rooms.

## Return portal

Dungeon rooms acquire a real return portal from the reference
`transfer_gate_02` AnimationClip (6 sprite frames at 12 fps, 0.5 s loop) and the
actual `magic_protal2` assets. The portal is placed at a clear spawn-relative
floor position and uses the normal interaction prompt:

```text
E  return portal
E again within 3 s to confirm
```

Confirmation returns through `ActivateRoom` to `hero_room` and reloads the
Living Room at its normal spawn; it is not a debug teleport and does not reset
room progress.

## Start

```bash
./scripts/run-devroom.sh
# or
./scripts/dev.sh
```

## Asset preparation

`scripts/prepare-assets.sh`:

1. uses generated assets from `assets/generated/visual/r1_1_visual/` if present;
2. otherwise regenerates them from the local reference tree when available;
3. otherwise stops with instructions rather than launching a broken room.

Generated pixels are gitignored. See `docs/legal/reference-assets.md`.

## Developer controls

| Input | Action |
|---|---|
| `F2` / `F3` | save / load native slot 0 |
| `Esc` | pause / resume (settings backs out to pause first) |
| `Q` | Knight c00 Dual Wield power |
| `F4` | collision/visual/sort/aim debug bounds |
| `T` | cycle through a connected room |
| `H` / `B` | character station / weapon station (hub) |
| `F5` | asset laboratory |
| `F6` | character selector |
| `F7` | weapon selector |
| `F8` | enemy selector |
| `F9` | spawn selected enemy |
| `Shift+F9` | spawn five selected enemies |
| `F11` | toggle bordered fullscreen |
| `F12` | clear enemies |
| `F10` | developer overlay |
| `Tab`, `1`, `2` | next / primary / secondary weapon slot |
| `E` / `G` | interact/open chest/pick up / spawn a weapon pickup |
| `R` | reset player/combat/enemies/doors/obstacles to deterministic initial state |

Auto Aim is the fresh default; Manual remains selectable in Settings. Movement
and aim are independent. Walk through an open room door to change rooms
automatically. Chests open with `E`; coins magnetize and update the HUD wallet.
The test executable starts at the main menu; D-ROOM is the developer entry. `--map-seed <int>` reproduces the same deterministic native room
assignment. Selectors use `A/D` or left/right/up/down, `Enter` to confirm, `Esc`
to return. Legacy controls remain available when a selector is closed.

## 30-second review areas

```bash
./scripts/test-explore.sh living_room
./scripts/test-explore.sh workshop
./scripts/test-explore.sh garden
./scripts/test-explore.sh cellar
./scripts/test-explore.sh chest
./scripts/test-explore.sh weapons
./scripts/test-explore.sh r1_1
```

The review harness freezes the real application after the timer, prints
`REVIEW REQUIRED`, and waits for ENTER.

## Developer overlay

The overlay displays version, build hash, FPS/frame time, fixed ticks, character,
weapon, HP/armor, enemies, projectiles, position, facing, room, renderer and
platform. It is a native overlay, not production UI.

## Asset laboratory and tests

`--asset-lab` (or `F5`) opens the recovered-asset inspector. See
`docs/development/ASSET_LAB.md`.

`scripts/test-interactive.sh --test <name>` opens a real window and serializes
the fixed-tick input/state stream. See `docs/development/INTERACTIVE_TESTS.md`.

## Invariant

```text
source -> build -> tests -> playable Wayland/Vulkan devroom
```

If a feature is incomplete, keep it behind a dev flag, fallback, or isolate it.
Do not merge a permanently broken devroom.
