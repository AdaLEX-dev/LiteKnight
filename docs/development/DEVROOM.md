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
| `P` | pause / resume |
| `F4` | collision/visual/sort debug bounds |
| `T` | Living Room <-> r1_1 transition |
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
| `1` / `2` | primary / secondary weapon slot |
| `E` / `G` | interact/pick up / spawn a weapon pickup |
| `R` | reset player/combat/enemies/doors/obstacles to deterministic initial state |
| `Esc` | close selector/asset lab, back, or quit |

Selectors use `A/D` or left/right/up/down, `Enter` to confirm, `Esc` to return.
Legacy controls remain available when a selector is closed.

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
