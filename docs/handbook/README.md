# LiteKnight Successor Handbook

This directory is the top-to-bottom operating manual for the LiteKnight
reconstruction. It is written for the next engineer who has to pick up this
repository without the original conversation, without the original machine, and
without assuming that the older documents are current.

The short version:

- LiteKnight is a native C++20 / SDL3 / Vulkan reconstruction of Soul Knight
  2.6.8 for Linux/Wayland.
- The reference game is **local reference material only**. Do not redistribute
  APK contents, extracted pixels, audio, or generated reference PNGs.
- The repository commits source, metadata, scene topology, tests, and small
  generated metadata files. It does **not** commit the proprietary reference
  tree or generated reference pixels.
- The normal playable executable is `build/soul-knight`.
- The canonical build directory on the current machine is `build/`; older
  `build-release/` binaries may be stale.
- The test suite is not a substitute for checking the actual coordinate,
  interaction, collision, and visual behavior the user asked for.

## Read this in order

| # | Document | What it answers |
|---|----------|-----------------|
| 00 | [00-OPERATING_MANUAL.md](00-OPERATING_MANUAL.md) | How do I build, run, test, debug, and change this project? |
| 01 | [01-GIT-GITHUB-RELEASES.md](01-GIT-GITHUB-RELEASES.md) | How do we do git, GitHub, CI, tags, and beta releases? |
| 02 | [02-CODE_ARCHITECTURE.md](02-CODE_ARCHITECTURE.md) | How is the code organized and how does a frame flow through it? |
| 03 | [03-ASSET_RECOVERY_AND_PIPELINE.md](03-ASSET_RECOVERY_AND_PIPELINE.md) | Where do assets come from and how are scene/metadata files built? |
| 04 | [04-GAMEPLAY_SYSTEMS.md](04-GAMEPLAY_SYSTEMS.md) | How do player, weapons, rooms, hub, items, save, and progression work? |
| 05 | [05-UI_AND_RENDERING.md](05-UI_AND_RENDERING.md) | How do the renderer, UI, HUD, depth sorting, and weapon presentation work? |
| 06 | [06-TESTING_AND_QA.md](06-TESTING_AND_QA.md) | How do unit, CTest, geometry, coordinate, trader, and smoke tests work? |
| 07 | [07-FEATURE_COOKBOOK.md](07-FEATURE_COOKBOOK.md) | Exact recipes for adding a weapon, enemy, room, UI screen, save field, etc. |
| 08 | [08-CURRENT_STATE_AND_ROADMAP.md](08-CURRENT_STATE_AND_ROADMAP.md) | What is done, what is in flight, what is broken, and what to do next? |
| 09 | [09-TROUBLESHOOTING.md](09-TROUBLESHOOTING.md) | Why is the window black, the asset stale, the test failing, or the binary missing? |
| 10 | [10-GLOSSARY_AND_SOURCE_MAP.md](10-GLOSSARY_AND_SOURCE_MAP.md) | Terms, file map, evidence tags, recovered identifiers, and key data files. |
| 11 | [11-COORDINATE_PASS_2026-11-01.md](11-COORDINATE_PASS_2026-11-01.md) | The Living Room coordinate-by-coordinate pass, results, and open visual issues. |

## Non-negotiable project rules

1. **Evidence first.** If a behavior, coordinate, sprite, prefab, or number is
   claimed to be recovered, it must be traceable to the local reference tree or
   to an explicit analysis document. If it is a reconstruction, mark it.
2. **Do not redistribute reference assets.** `original/`, `analysis/apktool_out/`,
   `assets/generated/`, and generated scene PNGs are local-only. See
   `docs/legal/` and `LEGAL.md`.
3. **One authoritative main executable.** `build/soul-knight` is the normal
   game binary. Do not leave a stale `/build/soul knight` or
   `build-release/soul-knight` that appears canonical if it is not.
4. **Fixed simulation.** Gameplay runs at 50 Hz fixed step. Rendering targets
   60 FPS. Do not move gameplay state into render-rate code.
5. **Walking never moves the weapon.** Weapon presentation is centralized in
   `WeaponRenderer::ComputePresentation`. Locomotion must not bob, sway, bounce,
   or translate the weapon. Aiming rotates around the fixed anchor. Firing may
   apply a tiny transient recoil and muzzle flash.
6. **Do not fake assets.** If a sprite is a generic placeholder, do not claim it
   is the correct prefab. Fix the extraction/path collision or mark it blocked.
7. **Do not use a green generic suite as acceptance.** Coordinate-specific,
   interaction-specific, collision-specific, and visual-specific checks matter.
8. **Do not tag a known-broken commit.** A release tag must build and test.

## Current release

The current beta release is `v0.6.0-beta.1`. It is a development beta, not a
finished game. See [docs/releases/v0.6.0-beta.1.md](../releases/v0.6.0-beta.1.md).

## Where the truth lives

- Runtime behavior: `src/`, especially `src/app/Application.cpp`.
- Scene/build truth: `tools/asset/build_*.py` and `analysis/assets/scenes/*.json`.
- Reference recovery evidence: `analysis/re/`, `analysis/assets/`, and
  `docs/EVIDENCE_STANDARD.md`.
- Tests: `tests/unit/`, `tests/tools/`, `tools/qa/`, and `CMakeLists.txt`.
- Historical decisions: `docs/DECISIONS/`, `docs/releases/`, and this handbook.

Older documents such as `docs/PROJECT_STATE.md`, `docs/ROADMAP.md`, and
`docs/development/LIVING_ROOM*.md` are useful history, but several were written
before the current hub/coordinate pass. The handbook and the current source are
the authoritative starting point. Update stale docs when you touch the behavior
they describe.
