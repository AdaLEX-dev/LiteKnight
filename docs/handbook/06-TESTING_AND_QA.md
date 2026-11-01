# 06 — Testing and QA

## 1. Test layers

LiteKnight uses several layers because a green unit suite is not acceptance:

| Layer | Tool | What it proves |
|-------|------|----------------|
| Unit | `build/soul-knight-test` (doctest) | Pure logic, state machines, parsing, math, save fields, weapon behavior. |
| CTest | `ctest --test-dir build` | Unit suite + asset audits + geometry + runtime smoke + QA scripts. |
| Geometry probe | `build/soul-knight-geometry --area ...` | Scene topology, floor/solid coverage, named area invariants. |
| Coordinate regression | `tools/qa/living_room_coordinate_regression.py` | Exact supplied Living Room coordinates, nearest object, collision, interaction, BFS walks. |
| Trader regression | `tools/qa/trader_stock_regression.py` | Real binary seed A/B/A stock determinism and difference. |
| Gameplay smoke | `build/soul-knight --gameplay-smoke` | Hub → dungeon → combat → clear → chest → next level → save/load → return hub. |
| Visual review | `--review-area` + screenshot | Visual-only issues that code/geometry tests cannot prove. |

## 2. Unit tests

```bash
./build/soul-knight-test
./build/soul-knight-test --no-breaks
./build/soul-knight-test --test-case="*Rogue starter*" --reporter=console
./build/soul-knight-test --list-test-cases
```

Current state at the beta cut:

```text
238 test cases
161,569 assertions
0 failed
```

The assertion count grows quickly because hub geometry tests iterate every
instance/solid. Do not use raw assertion count as a quality signal; use the
specific test names and coordinate reports.

Important test files:

```text
tests/unit/weapon_presentation_test.cpp    # hard walking/aim/fire rule
tests/unit/weapon_init_ranger_test.cpp     # Rogue starter != pistol
tests/unit/weapon_station_test.cpp         # blueprint/material crafting
tests/unit/item_test.cpp                   # chest rules, seeds, persistence
tests/unit/save_test.cpp                   # save schema round-trip
tests/unit/hub_geometry_test.cpp           # hub collision/sign/board invariants
tests/unit/room_visual_test.cpp            # stair gap/stair wall absence
tests/unit/ui_test.cpp                     # screen/action behavior
tests/unit/room_activation_test.cpp        # room lifecycle
tests/unit/door_test.cpp                   # door blocking/animation
```

### Adding a unit test

1. Add a new `tests/unit/<name>_test.cpp` with doctest.
2. Add the file to the `soul-knight-test` target in `CMakeLists.txt`.
3. Use existing test asset directories or create a temp fixture.
4. Avoid `REQUIRE` when exceptions are disabled; this project uses doctest with
   exceptions disabled. Use `CHECK`, then early-return if continuing would crash.
5. Test the real invariant, not a duplicate of the implementation.

## 3. CTest

```bash
ctest --test-dir build --output-on-failure
ctest --test-dir build -R liteknight --output-on-failure
ctest --test-dir build -R "coordinate|trader|gameplay" --output-on-failure
```

Current CTest coverage includes:

- `soul-knight-unit-tests`;
- `liteknight-asset-audit`;
- `liteknight-hero-room-audit`;
- `liteknight-hub-topology`;
- `liteknight-area-*` for living-room, garden, trader, mech, reforging,
  weapon-station, material-chest, workshop, planting, dungeon/pattern/boss
  areas, special rooms;
- `liteknight-area-coordinate`;
- `liteknight-living-room-coordinate-regression`;
- `liteknight-gameplay-smoke`;
- `liteknight-trader-stock-randomization`;
- `liteknight-weapon-runtime-audit`;
- `liteknight-game-catalogues`;
- `liteknight-reference-gallery`.

CTest tests run from the project root and use the prepared `build/assets` tree.
If you run CTest immediately after a builder edit without
`prepare-assets.sh`, you are testing stale assets.

## 4. Geometry probe

`build/soul-knight-geometry` is a small C++ tool with named areas and free
rectangles.

```bash
./build/soul-knight-geometry --area living-room
./build/soul-knight-geometry --area garden
./build/soul-knight-geometry --area trader
./build/soul-knight-geometry --area mech
./build/soul-knight-geometry --area reforging
./build/soul-knight-geometry --area weapon-station
./build/soul-knight-geometry --area material-chest
./build/soul-knight-geometry --area planting
./build/soul-knight-geometry --area coordinate-regression
./build/soul-knight-geometry --area rogue
./build/soul-knight-geometry --area mission-board
./build/soul-knight-geometry --area portal
./build/soul-knight-geometry --area grave
./build/soul-knight-geometry --area=-20,-2,4,4
```

The `coordinate-regression`, `rogue`, `mission-board`, `portal`, and `grave`
aliases invoke the Python coordinate suite. Named geometry areas inspect floor
coverage and solid-object footprints. A rectangle argument checks that every
point is usable/inside the boundary.

## 5. Living Room coordinate regression

File: `tools/qa/living_room_coordinate_regression.py`.

Run:

```bash
python3 tools/qa/living_room_coordinate_regression.py
```

It reads:

```text
analysis/assets/scenes/hero_room.json
analysis/assets/items/item_assets.json
```

It reports every target in this format:

```text
LIVING_ROOM_COORDINATE_REGRESSION
Target: ...
Reference: (x, y)
Nearest: ...
Expected: ...
Actual: ...
PASS/FAIL
```

Current result at the beta cut:

```text
SUMMARY: 41/41 coordinate checks passed
```

Coverage includes:

- three sign removals (object, collider, marker);
- mission board visual/prefab/collider/interaction;
- Living Room weapon chest marker/definition/random rules;
- c13/c04/c08 table seating planes and depth;
- three-segment stair wall removal and dark outside-world gap;
- portal frame sprites, sorting, interaction, explorer NPC;
- table/object footprint collision at 17.4/27.6 with cardinal/diagonal probes;
- magic NPC → grave relationship and grave visual;
- three reforging tables and the one-block-closer middle approach;
- Rogue body inside beanbag with depth;
- wall cap shortening at 6.6/-23.08;
- invisible wall at 20.084/-10.92 while the legitimate south wall remains;
- oxygen bottle collision reaching the reported upper-body coordinate;
- mech/forge/trader/material/weapon-station markers and solids;
- garden entrance/return and tree-boundary test;
- autonomous walk from five spawn positions to ten destinations.

The suite deliberately accepts a thin floor-edge solid at the removed stair;
that is the intended dark-void boundary, not an artificial wall.

## 6. Trader stock regression

File: `tools/qa/trader_stock_regression.py`.

```bash
python3 tools/qa/trader_stock_regression.py
```

It launches the real `build/soul-knight` binary in hub mode with fixed map seeds
and extracts the `Trader: ... stock=...` log line. It asserts:

- seed A produces valid stock;
- seed B produces valid stock;
- seed A again produces identical stock;
- seeds A and B produce different stock.

Current result:

```text
SUMMARY: 4/4 trader stock checks passed
```

This is a runtime test, not a pure unit test. It requires a working Wayland
session and prepared assets.

## 7. Gameplay smoke

```bash
./build/soul-knight --gameplay-smoke --smoke-seconds 25
```

The smoke stages cover:

1. hub room, pets, recipes;
2. garden plant/harvest;
3. NPC dialogue;
4. trader purchase (orbs deducted, sold state);
5. mech build;
6. hub → dungeon transition;
7. 1-1 initialization/start banner/floor surfaces;
8. combat room entry with closed doors;
9. combat clear and door open;
10. dungeon chest drop;
11. end room;
12. next level (1-2);
13. save → reset → load;
14. return to hub with garden plots.

The smoke exits non-zero on any failed invariant.

## 8. Visual review

Geometry tests cannot prove pixel correctness. Use the named review areas and
screenshot:

```bash
./build/soul-knight --hub --review-area table-characters --smoke-seconds 6 &
sleep 4.5
spectacle -b -n -f -o /tmp/table.png
sleep 2
```

Then crop the application window. On the current machine the app window starts
around `(257, 76)` at 1920x1080 desktop; adjust for your compositor.

For the current coordinate pass, inspect at least:

```text
sign1, mission-board, weapon-chest, table-characters, stair,
portal, sign2, table17, object27, npc-grave, table31, table35,
table39, rogue, sign3, wall-short, invisible-wall, oxygen,
material-chest-ui, weapon-station-locked, mech-ui-live, trader-ui,
trader-world
```

## 9. What visual inspection already proved

From the current pass:

- The mission board is visible and restored as the recovered notice board.
- The portal frame is visible, with the interaction prompt and a nearby NPC.
- The Rogue was hidden under a generic signboard because the transparent
  `nothing` sprite shared Unity path ID 1 with the signboard. Fixing sprite-ID
  collision disambiguation restored the actual Rogue body in the beanbag.
- The table characters now align to the chair seat plane; c13/c04/c08 no longer
  intersect the chair backs.
- The material chest is a dedicated `MATERIALS` screen with icon rows once stock
  exists.
- The mech panel is enlarged and readable.
- The trader offers are three cards; the world trader has three vertical
  pedestal weapons.
- The oxygen bottle collider covers the reported upper-body coordinate.

## 10. What still needs visual acceptance

The coordinate suite is green, but these are visual/manual:

- exact visual “sitting” pose for c13/c04/c08 (no dedicated sitting clip);
- exact vertical pedestal weapon scale/sitting offset;
- exact material icon appearance/readability on all resolutions;
- exact mech panel cropping at other aspect ratios;
- Rogue pose (no dedicated sitting clip; current body is the recovered standing
  body positioned in the beanbag);
- mission board orientation relative to the wall;
- portal frame/NPC facing;
- oxygen tank footprint “feel” (not just AABB coverage).

When you touch any of these, capture a screenshot and add the result to the
coordinate report or release note. Do not claim pixel correctness from geometry.

## 11. Test-writing rules

- Prefer exact coordinates and exact object paths in regression scripts.
- Include a nearest-object/distance line in failure output.
- Test collision from multiple approaches, including diagonal/corner.
- Test both the forbidden state and the legitimate neighbor state (for example,
  invisible wall gone but real wall remains).
- Test persistence across save/load for anything the player can mutate.
- Test different seeds for randomized loot/stock.
- Test “no duplicate/free item” behavior.
- Test no-op/negative cases (insufficient materials, already sold, already open).
- Do not delete a failing test just because the behavior changed; update it to
  the new invariant unless the test itself was wrong.

## 12. Current test status

At the beta cut:

```text
Unit:      238 test cases / 161,569 assertions / 0 failed
Coordinate: 41/41 checks passed
Trader:     4/4 checks passed
Gameplay smoke: passed
```

The full Debug/Release matrix and 108-room/1000-seed sweeps are **not**
guaranteed by this beta. See `08-CURRENT_STATE_AND_ROADMAP.md` for the remaining
verification debt.
