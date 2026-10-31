# Asset Inventory

The living inventory is generated from committed scene metadata:

- [texture_inventory.json](../../analysis/assets/texture_inventory.json)
- [asset_inventory.json](../../analysis/assets/asset_inventory.json)

## Current statistics (`0.4.0-dev`)

| Category | Integrated | Observed reference candidates | Highest status |
|---|---:|---:|---|
| Textures | 14 | 365 | INTEGRATED |
| Sprites | 182 | 6408 | INTEGRATED |
| Characters | 15 | 15 | DEVROOM_TESTED (c00 live) |
| Weapons | 4 | 365 unique IDs | DEVROOM_TESTED |
| Projectiles | 3 unique | not counted | DEVROOM_TESTED |
| Enemies | 1 (`e_slime01`) | 125 unique IDs | DEVROOM_TESTED |
| Animations | 45 | 2098 clips | INTEGRATED (idle/run/death subset) |
| Doors | 4 | 4 in `r1_1` | DEVROOM_TESTED |
| Obstacle variants | 12 (8 solid) | recovered `obstacle_list` | DEVROOM_TESTED |
| Walls | 140 cells / 420 sprites | room topology | DEVROOM_TESTED |
| Living Room | 9,236 instances / 434 sprites | HeroRoom level2 + native floor fill | DEVROOM_TESTED |
| Effects / items / UI / audio | 0 reference sprites | discovery names only | UNRECOVERED / native parity UI |

The maturity ladder is `DISCOVERED`, `EXTRACTED`, `TRANSLATED`, `VALIDATED`,
`INTEGRATED`, `DEVROOM_TESTED`. It is recorded per category in
`analysis/assets/asset_inventory.json`.

The observed reference counts are from local UnityPy scans. They are not a claim
that every candidate is recoverable, licensed, or appropriate for release.

## Missing / not integrated

- 124 observed enemy IDs not integrated
- 361 observed unique weapon IDs not integrated
- attack/charge/hit/death animation states not integrated
- effects, items, pickups, UI assets, audio, environment decorations
- recovered UI font atlas (developer text atlas is a PLACEHOLDER)

## Redistribution

Reference-derived textures and sprites are `local_only`. Project-authored source
and the generated developer font are native code; proprietary pixels are never
committed. See `docs/legal/reference-assets.md`.
