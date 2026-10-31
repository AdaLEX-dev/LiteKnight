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
| `F6` | character selector |
| `F7` | weapon selector |
| `F8` | enemy selector |
| `F9` | spawn selected enemy |
| `F11` | spawn five selected enemies |
| `F12` | clear enemies |
| `F10` | developer overlay |
| `R` | reset player/combat/enemies/doors/obstacles to deterministic initial state |
| `Esc` | close selector or quit |

Selectors use `A/D` or left/right/up/down, `Enter` to confirm, `Esc` to return.
Legacy controls remain available when a selector is closed.

## Developer overlay

The overlay displays version, build hash, FPS/frame time, fixed ticks, character,
weapon, HP/armor, enemies, projectiles, position, facing, room, renderer and
platform. It is a native overlay, not production UI.

## Invariant

```text
source -> build -> tests -> playable Wayland/Vulkan devroom
```

If a feature is incomplete, keep it behind a dev flag, fallback, or isolate it.
Do not merge a permanently broken devroom.
