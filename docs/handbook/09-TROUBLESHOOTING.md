# 09 — Troubleshooting Cookbook

## 1. The application exits immediately or shows a black window

Check the stdout/log first:

```bash
./build/soul-knight --hub --smoke-seconds 3
```

Look for:

```text
Wayland-only mode
SDL video driver
Runtime scene
Scene loaded
Vulkan
Swapchain
```

Common causes:

- `SDL_VIDEODRIVER` is not `wayland`; the app intentionally rejects X11.
- No Vulkan 1.3 driver/device.
- `build/assets` is missing or stale.
- The requested scene path does not exist.
- The renderer failed to create the swapchain.

If needed:

```bash
echo "$XDG_SESSION_TYPE"
echo "$WAYLAND_DISPLAY"
vulkaninfo --summary
```

The project requires a real Wayland session. Do not “fix” it by enabling X11.

## 2. “Runtime scene ... (FOUND)” but the room looks unchanged

You are looking at a stale runtime copy.

```bash
stat -c '%y %n' analysis/assets/scenes/hero_room.json \
  build/assets/scenes/hero_room/scene.json
./scripts/prepare-assets.sh
cmake --build build -j"$(nproc)"
```

The source of truth is `tools/asset/build_*.py`; never edit
`build/assets/.../scene.json` by hand.

If `prepare-assets.sh` skipped the builder due to its cache check, run the
builder directly:

```bash
tools/asset/.venv/bin/python tools/asset/build_hub_scene.py
tools/asset/.venv/bin/python tools/asset/build_item_assets.py
./scripts/prepare-assets.sh
```

## 3. The coordinate suite passes but the game is wrong

The coordinate suite reads `analysis/assets/scenes/hero_room.json`. The game
reads `build/assets/scenes/hero_room/scene.json`. They can disagree after a
builder edit.

Also remember:

- geometry tests cannot prove sprite appearance;
- the coordinate suite uses a simplified AABB movement model, not the real
  player controller;
- visual review needs a screenshot.

Use `--review-area` and compare the actual window.

## 4. A sprite is wrong / a signboard covers an NPC

Likely Unity path-ID collision. Inspect the scene instance’s `sprite` and the
sprite’s `rect`:

```bash
python3 - <<'PY'
import json
d=json.load(open('analysis/assets/scenes/hero_room.json'))
sp={s['id']:s for s in d['sprites']}
for i in d['instances']:
    if 'c01/img/lighting' in i['path']:
        print(i['sprite'], sp[i['sprite']]['rect'])
PY
```

If two different sprites share `hub_spr_<id>`, the builder must disambiguate with
the content hash logic in `ensure_sprite`. Do not patch the runtime scene.

## 5. The mission board is missing / signs are back

Check the builder’s `removed_prefixes` and the task board filter:

```bash
grep -n "task/board\|roomdecorateslot" tools/asset/build_hub_scene.py
```

The functional `room/objects/task/board` must be present. Decorative
`roomdecorateslot_*` sign slots must be absent. After changing the builder:

```bash
tools/asset/.venv/bin/python tools/asset/build_hub_scene.py
./scripts/prepare-assets.sh
```

## 6. The Living Room chest gives the same weapon every time

Check which definition is used:

```bash
grep -n "hub_weapon_chest\|AddChest" src/app/Application.cpp
python3 - <<'PY'
import json
d=json.load(open('analysis/assets/items/item_assets.json'))
for c in d['chests']:
    if 'hub' in c['id'] or c['id']=='chest00':
        print(c['id'], c.get('appointed_weapon'),
              c.get('weapon_drop_level'))
PY
```

The hub must use `hub_weapon_chest`, not `chest00` (which appoints
`weapon_021`). If `hub_weapon_chest` is missing, rerun
`tools/asset/build_item_assets.py`.

## 7. The Rogue is missing from the beanbag

Look for `characters/c01/img/lighting` in the scene. If its sprite rect is
`21x30` instead of `5x5`, the transparent `nothing` sprite was aliased to the
signboard. Rebuild the hub scene with the current `ensure_sprite`
disambiguation.

Also check that the active player character is not `c01`; the runtime hides the
selected player’s duplicate NPC.

## 8. The weapon moves while walking

This is a hard regression. Check:

- `WeaponRenderer::ComputePresentation` is the only presentation path;
- no call site adds `player_.Position()`/locomotion offsets;
- `WeaponRenderer::Draw` and `DrawAt` both use the presentation;
- the muzzle is gated by `visual_attack_active`.

Run:

```bash
./build/soul-knight-test --test-case="*Walking never moves*"
```

If it passes but the game moves, check for a second draw path in
`Application.cpp` or a synthetic weapon renderer.

## 9. Wrong weapon sound / no Rogue projectile

Check the scene weapon entry:

```bash
python3 - <<'PY'
import json
d=json.load(open('analysis/assets/scenes/hero_room.json'))
for w in d['weapons']:
    if w['id'].startswith('weapon_init_ranger'):
        print(w.get('audio_clip'), w.get('projectile'), w.get('special'))
PY
```

Expected: `fx_sword1`, projectile `bullet_107` with a real sprite, special
`rogue_throw`. If missing, check `SceneLoader.cpp`’s Rogue hybrid enrichment and
the scene JSON. Run:

```bash
./build/soul-knight-test --test-case="*Rogue starter*"
```

## 10. Workshop gives a free weapon / blueprint state is wrong

Check:

- `TryInteract` for `weapon_station` opens `OpenWorkshop`;
- `UIAction::CraftWeapon` checks blueprint and materials before
  `ConsumeForCraft`;
- `Workshop::CanCraft` returns false for locked blueprint or missing materials;
- `GameUI::DrawWorkshop` shows `BLUEPRINT`/`NEED TO COLLECT...`.

Run:

```bash
./build/soul-knight-test --test-case="*Weapon station*,*Blueprint*"
```

The 17 locked IDs are in `Workshop.cpp`. Do not add a free fallback.

## 11. Materials or blueprints disappear after reload

The save serializer must read the fields, not just write them. Check:

```bash
grep -n "unlocked_blueprints\|materials" src/game/save/SaveSerializer.cpp
```

Both write and read paths must exist. Run:

```bash
./build/soul-knight-test --test-case="*Save round-trips*,*Blueprint state*"
```

## 12. Trader stock never changes

Check the order of seed initialization in `Application::Initialize` and
`ConfigureTraderOffers`. The seed must exist before the first stock roll.

Run:

```bash
python3 tools/qa/trader_stock_regression.py
```

Expected: seed A, seed B different; seed A repeated identical. If you see
`seed=0`, the seed initialization order is wrong.

## 13. Garden is not enterable

Run:

```bash
python3 tools/qa/living_room_coordinate_regression.py
./build/soul-knight-geometry --area garden
```

If the BFS says reachable but the player is blocked, check:

- tree/planter collision boxes;
- house/room boundary;
- a stale runtime scene;
- the actual player collider vs the grid model.

Do not delete all garden collision. The intended entrance is open; the tree
boundary outside it must remain solid.

## 14. Table/character/furniture collision feels wrong

Inspect the actual solid boxes:

```bash
python3 - <<'PY'
import json
d=json.load(open('analysis/assets/scenes/hero_room.json'))
for a in d['collision']['solids']:
    if 30 <= a[0] <= 40 and -7 <= a[1] <= -2:
        print(a)
PY
```

Remember:

- forge tables use per-table footprints;
- object-portal long tables use sprite width + 2-unit base;
- oxygen bottles use recovered polygon AABB;
- chairs/tables are not full-sprite rectangles;
- a thin floor-edge solid at the stair is intentional.

## 15. UI text/controls look wrong

- Check the logical viewport (`Viewport contract` log).
- Use the same rect helper for draw and hit-test.
- Use `TextFitted` for long labels.
- The document design resolution is 1920x1080.
- The font may be missing (`has_pixel_font` false).
- The mech/workshop/trader screens use logical UI coordinates, not world units.

## 16. Build fails after adding a file

Add the `.cpp` to the correct `add_library`/`add_executable` list in
`CMakeLists.txt`. Re-run CMake:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
```

Do not add source files only to the IDE/editor project.

## 17. Unit test fails after an intentional behavior change

Read the failure. If the old test asserted the old behavior, update the test to
the new invariant and keep a meaningful assertion. Do not delete the test.

Example from the beta pass:

- old furnace test expected the player to stop at `y > -3.05`; the new reduced
  footprint stops at `y ≈ -3.63`, one block closer. The test now asserts
  `-3.70 < y < -3.55`.
- old stair test expected 9 wall tiles and a 3x3 block; the new intended behavior
  is zero repair tiles and no block. The test now asserts that.

## 18. Git push/release problems

- `gh auth status` must show a valid account with `repo`.
- `git remote -v` must be the canonical URL.
- `git status -sb` must be clean before tagging.
- If the tag exists, do not move it; increment the beta number.
- If a release is wrong, delete the GitHub release and tag, then cut a new beta.
- Never force-push `main` without owner approval.

## 19. Stale executable confusion

Canonical:

```text
build/soul-knight
```

Older/generated:

```text
build-release/soul-knight   # may be an old synthetic-scene binary
build-debug/soul-knight     # only valid if built from current tree
/build/soul knight          # not present in this tree
```

Before reporting a crash, run:

```bash
stat -c '%y %s %n' build/soul-knight build-release/soul-knight 2>/dev/null
file build/soul-knight
./build/soul-knight --help | head
```

Use the canonical binary. Do not debug a stale binary as if it were current.

## 20. “It works in the test but not in the game”

The classic causes, in order:

1. stale `build/assets`;
2. a test using `analysis/assets` while the game uses `build/assets`;
3. a test using a synthetic fixture with different data;
4. a visual-only issue that geometry cannot see;
5. the game using a different active character/seed/save;
6. a code path not exercised by the test (e.g., marker interaction vs runtime
   proximity).

Reproduce with `--review-area` and the exact `--map-seed`, then compare logs and
screenshots.
