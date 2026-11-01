# 11 — Living Room Coordinate Pass (2026-11-01)

This is the record of the strict coordinate regression pass. The authoritative
runner is:

```bash
python3 tools/qa/living_room_coordinate_regression.py
```

It reads the generated Living Room scene and item metadata. It prints a
nearest/reference/expected/actual/PASS block for every target. At this beta cut:

```text
SUMMARY: 41/41 coordinate checks passed
```

This is **not** a claim that every visual pixel is correct. The visual-only
caveats are listed at the end.

## 1. Supplied coordinate results

| # | Coordinate | Target | Result | Evidence |
|---|------------|--------|--------|----------|
| 1 | `(-12.630, 6.080)` | sign removal | **PASS** | 0 signboard objects, 0 nearby solids, 0 markers. |
| 2 | `(-7.065, 5.130)` | mission board | **PASS** | 3 recovered task/board parts, marker with 47 task lines, top `hub_spr_1493`, bottom `hub_spr_1414`, colliders present. |
| 3 | `(-2.643, 5.410)` | weapon chest | **PASS** | static copy removed; marker at `(-2.89, 6.31)`; `hub_weapon_chest` random definition; chest00 visuals. |
| 4 | `(-0.280, 0.940)` | table characters | **PASS** | c13/c04/c08 feet on chair seat plane (`seat_delta=-0.00`), character layer 5 > chair 4. |
| 5 | `(-10.405, -9.080)` | remove 3 walls | **PASS** | 0 repair visuals, 0 artificial 3x3 block, 0 thick solids, 0 floor tiles. Thin void edge is intentional. |
| 6 | `(13.724, -14.920)` | portal frame + NPC | **PASS** | `magic_protal1` top/base restored, portal marker, talkable explorer at distance 3.64. |
| 7 | `(21.034, -21.840)` | sign removal | **PASS** | 0 statue-slot sign objects, 0 nearby solids, 0 markers. |
| 8 | `(17.400, -26.440)` | table collision | **PASS** | `hub_spr_2229` object, footprint `[11.75,-28.05,21.25,-26.15]`, cardinal/diagonal probes blocked. |
| 9 | `(27.600, -26.440)` | object collision | **PASS** | `hub_spr_2240` object, footprint `[23.85,-28.23,33.15,-26.33]`, cardinal/diagonal probes blocked. |
| 10 | `(31.345, -20.100)` | NPC + grave | **PASS** | magic NPC marker 1.08 from grave reference, body present/facing grave, guardian sprite `hub_spr_1047` at 2.57. |
| 11 | `(32.515, -16.910)` | grave reference | **PASS** | recovered guardian/grave visual nearest at 2.57. |
| 12 | `(31.012, -3.760)` | table collision | **PASS** | `hub_spr_2179`, per-table footprint, cardinal/diagonal probes blocked. |
| 13 | `(35.037, -3.131)` | reduced table collision | **PASS** | `hub_spr_121`, top `-4.03`, north approach stops at center `-3.63` (one block closer). |
| 14 | `(39.717, -3.326)` | table collision | **PASS** | `hub_spr_2195`, per-table footprint, cardinal/diagonal probes blocked. |
| 15 | `(10.454, -2.440)` | Rogue + beanbag | **PASS** | `characters/c01/img/body` at `(9.93,-1.64)`, sofa `(9.77,-1.00)`, body inside sofa bounds, layer 5 vs 4. |
| 16 | `(13.790, 5.610)` | sign removal | **PASS** | 0 safe-slot sign objects, 0 nearby solids, 0 markers. |
| 17 | `(6.600, -23.080)` | shorten wall | **PASS** | cap instance count 0, east walk to 8.60, return to 6.60, south wall blocked. |
| 18 | `(20.084, -10.920)` | invisible wall | **PASS** | 0 nearby solids, horizontal/north crossing clear, legitimate south wall remains blocked at y≈-12.10. |
| 19 | `(23.625, -4.740)` | oxygen tanks | **PASS** | `hub_spr_2172` bottle; footprint `(21.89,-6.97,25.01,-4.30)` reaches the reported upper body; approaches blocked. |

## 2. Station and area results

| Target | Result | Evidence |
|--------|--------|----------|
| mech boundary | **PASS** | `mech_station` marker + 2 solids. |
| trader station | **PASS** | `trader_station` marker + 4 solids. |
| reforging station | **PASS** | `forge_station` marker + 6 solids. |
| material chest | **PASS** | marker + 2 solids. |
| weapon station | **PASS** | marker + 4 solids. |
| garden entrance | **PASS** | lobby→garden and garden→lobby BFS both reachable. |
| garden tree boundary | **PASS** | restricted y=6 crossing blocked. |
| autonomous walk from lobby | **PASS** | all 10 destinations reachable. |
| autonomous walk from portal | **PASS** | all 10 destinations reachable. |
| autonomous walk from workshop | **PASS** | all 10 destinations reachable. |
| autonomous walk from garden | **PASS** | all 10 destinations reachable. |
| autonomous walk from beanbag | **PASS** | all 10 destinations reachable. |

The autonomous walk currently uses a grid AABB model derived from the scene
solids, not the real player controller. Treat it as a topology/collision test,
not a full runtime autopilot.

## 3. Runtime/unit regressions in this pass

| Test | Result |
|------|--------|
| `soul-knight-test` (238 cases / 161,569 assertions) | **PASS** |
| `liteknight-living-room-coordinate-regression` CTest | **PASS** |
| `liteknight-trader-stock-randomization` CTest | **PASS** |
| `liteknight-gameplay-smoke` CTest | **PASS** |
| Rogue starter weapon unit test | **PASS** |
| Weapon presentation walking/aim/fire unit test | **PASS** |
| Weapon station blueprint/material unit tests | **PASS** |
| Multi-seed hub chest loot unit test | **PASS** |
| Hub chest save/restore unit test | **PASS** |
| Save materials/blueprints round-trip | **PASS** |

## 4. Important fixes found by this pass

### Rogue hidden under a signboard

The Rogue’s `img/lighting` renderer references the transparent sprite named
`nothing` (5x5). The hub builder named sprites only by Unity path ID, and a
different sprite with the same path ID (a 21x30 signboard) won the alias. The
Rogue was drawn, but a signboard was drawn on top of him. The builder now
compares texture/rect/pivot and appends a content hash for collisions. This
restored the Rogue and also removed an unnoticed signboard from the fishbowl/
magic-well empty decoration slots.

### Hub chest was an appointed duplicate weapon

`chest00` has an appointed `weapon_021`, so using it for the Living Room chest
would always produce the same free weapon. The hub now uses
`hub_weapon_chest`: chest00 visuals/collider, `weapon_drop_level=-2` (random
level from current chest level), no appointed weapon. Multi-seed loot and
save/restore tests cover it.

### Materials/blueprints were written but not read

`SaveSerializer` wrote `materials` but had no read path for it, and
`unlocked_blueprints` was missing entirely. The save test now round-trips both.
This was a real persistence break.

### Trader stock could be generated before the seed existed

The first stock roll happened before `trader_stock_seed_` was initialized in one
startup path, producing `seed=0` and identical stock. Seed initialization now
precedes the first `ConfigureTraderOffers` call. The A/B/A runtime script passes.

### Table characters

c13/c04/c08 are snapped to the chair seat plane (`chair Y + 0.40`) and kept at
character sorting layer 5 above chair layer 4. The current pose has no dedicated
sitting animation clip; this is a transform/depth fix, not a new animation.

### Stair wall

The old builder added 9 wall tiles and a 3x3 solid over the removed cellar stair.
Both are gone. The remaining thin floor-edge solid at the opening is the
intended dark-void boundary. The room_visual test was updated to assert the new
behavior.

## 5. Visual acceptance still required

The following are **not** proven by the coordinate suite:

- exact sitting pose of c13/c04/c08 (no recovered sitting clip);
- exact Rogue pose in the beanbag (recovered standing body, no sitting clip);
- exact vertical pedestal weapon scale/sitting offset;
- material icon readability at different resolutions;
- mech panel cropping at non-16:9 aspects;
- mission board orientation relative to the wall;
- portal NPC facing;
- oxygen AABB “feel” vs a full polygon physics body;
- trader UI card orientation vs world pedestal orientation.

Capture screenshots with `--review-area` and record them in the release note or
the next coordinate pass.

## 6. How to reproduce

```bash
./scripts/prepare-assets.sh
cmake --build build -j"$(nproc)"
python3 tools/qa/living_room_coordinate_regression.py
python3 tools/qa/trader_stock_regression.py
./build/soul-knight-test
ctest --test-dir build -R "liteknight-(living-room-coordinate-regression|trader-stock-randomization|gameplay-smoke)" --output-on-failure
```

The coordinate report must stay in the loop for any future Living Room change.
