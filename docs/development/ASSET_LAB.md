# Asset Laboratory

**Status:** FUNCTIONAL for integrated assets; catalogue discovery covers the
full local reference tree. Reference pixels remain `LOCAL_ONLY`,
`REFERENCE_DERIVED`, `NOT_FOR_REDISTRIBUTION`.

## Purpose

The asset laboratory makes a recovered asset inspectable without spawning it
into the live room. It is the visual QA step between translation and gameplay
integration.

```bash
./scripts/prepare-assets.sh --refresh-catalogue
# If the extraction tree is absent and apktool is on PATH:
./scripts/prepare-assets.sh --extract --refresh-catalogue
./build/bin/liteknight-devroom --asset-lab
# or press F5 inside the devroom
```

## Animation lab

Animations now include recovered idle, run and death clips for `c00..c14`
(45 clips). Use `Space` play/pause, `F`/`N` step, `R` restart. The panel shows
frame count, time/duration, sample rate and loop flag. Attack is not a body
SpriteRenderer clip in the recovered c00 controller; the weapon Animator carries
the attack timing.

## Controls

| Input | Action |
|---|---|
| `[` / `]` | previous / next category |
| `,` / `.` or left / right | previous / next item |
| `Space` | play / pause animation playback |
| `N` | single frame step |
| `Enter` | use: equip weapon, switch character, spawn enemy, select obstacle |
| `F5` or `Esc` | close |

Categories currently expose integrated characters, weapons, projectiles,
enemies, obstacles, doors, textures, sprites and animations. Effects, UI and
items/pickups are explicitly shown as `UNRECOVERED`; they are not replaced with
fake content.

## Metadata shown

- source/local-only status
- path/inventory identity
- PPU and pivot
- dimensions for textures
- animation ID/time where applicable
- weapon damage/speed/cycle/projectile/consume
- enemy HP/camp/contact damage/cooldown
- obstacle visual count/solidity

## Complete reference catalogue

`tools/asset/discover_reference.py` scans all local AssetBundles and writes
`analysis/assets/reference_catalogue.json`. It records object names, path IDs,
bundle/CAB provenance, texture/sprite metadata, animation clips and discovered
weapon/enemy IDs. It does not write pixels.

The inventory adds the maturity ladder:

```text
DISCOVERED -> EXTRACTED -> TRANSLATED -> VALIDATED -> INTEGRATED -> DEVROOM_TESTED
```

`analysis/assets/asset_inventory.json` records counts per category.
`analysis/assets/audit-report.json` and `.txt` record validation status,
warnings, errors and the explicit `UNRECOVERABLE`/`UNKNOWN` vocabulary.

## Repository boundary

Native source, translators, manifests, inventories, audit reports and tests are
committed. The APK, extracted Unity trees and generated PNG/RGBA pixels are
local-only. Not being committed does **not** mean unavailable to the local
build: `scripts/prepare-assets.sh` consumes the full local reference tree.

See `docs/legal/reference-assets.md`.
