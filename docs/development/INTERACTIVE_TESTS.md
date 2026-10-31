# Serialized Interactive Tests

**Status:** FUNCTIONAL (real Wayland/Vulkan window, recording, serialization,
named scenarios and replay).

These tests complement headless doctest/CTest cases. They require a real
Wayland compositor and Vulkan device because the purpose is to exercise the
same executable, renderer and asset environment used for development.

## Run

```bash
./scripts/test-interactive.sh --list
./scripts/test-interactive.sh --test combat_live
./scripts/test-interactive.sh --replay test-results/2026-10-31_20-41-55_combat_live.json
```

The process opens a window, prints instructions, records the fixed-tick input
and state stream, and writes:

```text
test-results/<timestamp>_<name>.json
```

`test-results/` is gitignored: recordings contain local runtime state and may
reference local-only asset provenance.

## Finish / abort

- `Enter` finalizes as `PASS`.
- `Esc` aborts and returns a non-zero exit code.
- Closing the window before finalization serializes `ABORTED`.

## Result schema

- test name, LiteKnight version, compile-time commit, timestamp
- platform and renderer
- scene ID and SHA-256
- reference APK SHA-256, asset-inventory hash, scene hash
- instructions and notes
- every fixed-tick `InputSnapshot`
- serialized key commands (menus, spawners, doors, reset, save/load)
- state samples: player position/velocity/camera, animation state/frame,
  character, weapon, room, HP/armor/energy, enemy/projectile counts, death and
  door state
- result

Wall-clock time is test metadata only; gameplay simulation remains 50 Hz fixed
step.

## Named scenarios

```text
character_all  weapon_all  enemy_all      projectile_all
object_all     door_all    combat_live    two_weapon
energy         health      save_load      room_transition (partial)
hub            UNRECOVERED
```

`room_transition` currently records the one-room door limitation rather than
claiming a room transition. `hub` records the missing hub feature; no fabricated
hub is presented as recovered content.

## Replay

Replay feeds the serialized `InputSnapshot` and key-command stream back into the
same fixed simulation:

```bash
./scripts/test-interactive.sh --replay test-results/<file>.json
```

Verified in this milestone: replay reproduced the recorded `combat_live` state
samples with zero differences, including enemy spawn (F11), player death and
R reset.

Known limitation: the replay starts from the default scene/state; it does not
yet serialize arbitrary pre-recording menu/asset state or RNG streams.
