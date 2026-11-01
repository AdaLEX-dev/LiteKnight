# Save / Load

## Native format

SaveData is a native versioned JSON schema, not the original Odin/Sirenix
serialized save. Current version: **3**.

Version 3 adds:

```text
player.orbs    Living Room orb/gem currency
```

Migration paths:

```text
v0 -> v1 -> v2 -> v3
v1: adds progression/settings defaults
v2: adds coins
v3: adds orbs
```

Future save versions are rejected, never silently overwritten.

## UI

The main menu now has an **S/L** button adjacent to New Game. It opens a
SAVE/LOAD panel for native slot 0:

- summary: character, scene, weapons, HP/armor/energy, coins, orbs, playtime, seed
- SAVE / LOAD / DELETE / BACK
- keyboard and pointer navigation

The summary uses the native slot’s serialized data; fields that cannot be
recovered are not fabricated.

## Persistence still missing

- per-room opened chests and chest loot
- coin/orb pickups on the floor
- materials, seeds, blueprints, forge weapons
- pet selection
- plant growth timers
- map/room progression beyond the current graph
