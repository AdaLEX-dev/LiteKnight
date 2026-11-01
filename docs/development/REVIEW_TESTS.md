# 30-Second Manual Review Harness

`scripts/test-explore.sh` launches one real game state for a bounded visual
review. It is intentionally not a screenshot regression test.

```bash
LITEKNIGHT_REVIEW_SECONDS=30 ./scripts/test-explore.sh <area>
```

The script prints a large terminal banner, starts the normal executable, and
after the timer expires the application freezes the simulation, keeps the window
visible, prints `REVIEW REQUIRED`, and waits for ENTER in the terminal.

Environment overrides:

```bash
LITEKNIGHT_REVIEW_SECONDS=30
LITEKNIGHT_BUILD_DIR=/path/to/build
```

Available areas:

| Area | State |
|---|---|
| `main_menu` | normal startup main menu |
| `settings` | main menu with Settings open |
| `pause` | Living Room gameplay with Pause open |
| `living_room` | Living Room gameplay |
| `devroom` | seeded `r1_1_visual` room with the full room graph |
| `r1_1` | seeded `r1_1_visual` room |
| `r1_2` | seeded `r1_2_visual` room |
| `r1_3` | seeded `r1_3_visual` room |
| `ui_design` | Living Room with UI Design Mode active |
| `workshop` | Living Room teleported to the weapon workshop |
| `garden` | Living Room teleported to the planter/garden |
| `cellar` | Living Room teleported to the bottom cellar |
| `chest` | r1_1 with chest00/01/02 review row |
| `weapons` | integrated weapon catalogue selector |
| `fullscreen` | main menu in 1920x1080 fullscreen |

The application also supports the flags directly:

```bash
build/soul-knight --review-area settings --review-seconds 30 --fullscreen
```

`--review-seconds` is clamped to 1..3600. The normal `test-explore.sh` default is
30 seconds; short values are useful for checking the pause/wait path.

## What the reviewer should inspect

- Main menu: title, button stack, centered labels, red swirl/background, Knight
  artwork, left edge, D-ROOM hit target, no social controls.
- Settings: every label/value inside its row, centered headings, toggle values,
  UI Design Mode entry, fullscreen and Auto Aim.
- Living Room / r1_1 / r1_2 / r1_3: player, foreground sorting, doors, chest,
  coins, HUD plate, compact minimap, ability/weapon area, pickup card.
- Fullscreen: no borders/clipping, 1920x1080 drawable/viewport, stable
  presentation.

The run log records the selected swapchain present mode, detected display
refresh, and measured present intervals (`present_avg_ms`, `present_min_ms`,
`present_max_ms`, `present_hz`, and the final `Present pacing` line).
