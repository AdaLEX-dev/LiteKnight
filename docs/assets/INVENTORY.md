# Asset Inventory

The living inventory is generated from committed scene metadata:

- [texture_inventory.json](../../analysis/assets/texture_inventory.json)
- [asset_inventory.json](../../analysis/assets/asset_inventory.json)

## Current statistics (`0.1.0-dev`)

| Category | Integrated | Observed reference candidates | Status |
|---|---:|---:|---|
| Textures | 14 | 365 | PARTIAL |
| Sprites | 182 | 6408 | PARTIAL |
| Characters | 15 | 15 | PARTIAL: idle/run only |
| Weapons | 4 | 383 weapon GameObjects | PARTIAL: first Gun001 set |
| Projectiles | 3 unique | not counted | PARTIAL |
| Enemies | 1 (`e_slime01`) | 126 enemy IDs | PARTIAL |
| Animations | 30 | not counted | PARTIAL: character ide/run |
| Doors | 4 | 4 in `r1_1` | INTEGRATED |
| Obstacle variants | 12 (8 solid) | recovered `obstacle_list` | INTEGRATED |
| Walls | 140 cells / 420 sprites | room topology | INTEGRATED |
| Effects / items / UI / audio | 0 | unknown | UNRECOVERED / PLACEHOLDER |

The observed reference counts are from local UnityPy scans. They are not a claim
that every candidate is recoverable, licensed, or appropriate for release.

## Missing / not integrated

- 125 observed enemy IDs not integrated
- 379 observed weapon GameObjects not integrated
- attack/charge/hit/death animation states not integrated
- effects, items, pickups, UI assets, audio, environment decorations
- recovered UI font atlas (developer text atlas is a PLACEHOLDER)

## Redistribution

Reference-derived textures and sprites are `local_only`. Project-authored source
and the generated developer font are native code; proprietary pixels are never
committed. See `docs/legal/reference-assets.md`.
