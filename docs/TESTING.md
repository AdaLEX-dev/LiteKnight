# Testing Strategy

**Status:** current baseline is **129 doctest cases / 5090 assertions**, all passing.
Future tests must be evidence-backed; do not invent expected values.

---

## 1. Current harness

```bash
./tools/configure.sh Debug
./tools/build.sh Debug
./tools/test.sh
ctest --test-dir build --output-on-failure
```

- doctest 2.4.11, CTest entries `soul-knight-unit-tests`, `liteknight-asset-audit` and `liteknight-hero-room-audit`.
- Debug: ASan/UBSan.
- RelWithDebInfo: optimized, no sanitizers.
- CTest fails if the filter selects zero cases (`FAIL_REGULAR_EXPRESSION`).
- Negative control: break an assertion, confirm CTest exits non-zero, restore.

## 1.1 Serialized interactive tests

Interactive tests are not CTest cases because they require a real
Wayland/Vulkan window. Run them explicitly:

```bash
./scripts/test-interactive.sh --test combat_live
./scripts/test-interactive.sh --replay test-results/<file>.json
```

They serialize fixed-tick inputs, key commands and state samples under
`test-results/`. Unit tests cover the replay loader, action mapping, save
format/migrations/atomicity, energy, inventory, pickup and enemy pursuit.
See `docs/development/INTERACTIVE_TESTS.md`.

## 2. Test layers

### Engine

- UnityRandom: seed expansion, Xorshift vectors, int/float range, min==max,
  min>max, global save/restore, lazy seed, independent streams, inside-circle.
- Math: Vec2/Vec3/Mat4/camera.
- Timing: 50 Hz fixed step, accumulator clamp, interpolation.

### Platform (implemented units)

- AppConfig defaults: 1920x1080, 50 Hz simulation, 60 FPS target.
- Wayland-only driver validation; non-Wayland rejected.
- Bootstrap CLI bounds and unknown-option tolerance.
- Window-state resize/minimize/restore/focus/close transitions.
- Safe repeated Application shutdown before initialization.
- FrameTimer consumed-step statistics.

### Renderer (implemented units)

- SpriteBatch layer sort and stable insertion order.
- SpriteBatch quad budget.
- Shader build is a CMake target (`glslc`); SPIR-V is generated in the build tree
  and copied to the executable-relative asset directory.
- Vulkan instance/device/swapchain/present is an **integration/manual** test,
  not a CTest case: it requires a real Wayland compositor and GPU.

### PNG / asset runtime (implemented units)

- libpng `PNG_FORMAT_RGBA` normalization to canonical RGBA8.
- Exact 4x4 RGBA byte/alpha oracle.
- RGB, grayscale, grayscale+alpha, and palette+tRNS normalization.
- Repeated decode determinism.
- Malformed/truncated input failure with cleared `TextureData` and non-empty error.
- Missing-file failure.
- Executable-relative test asset resolution (`FileSystem`).

### Sprite / scene / camera (implemented units)

- SpriteData rect validation, top-left UV conversion, pivot/PPU/border.
- Sprite geometry: center/corner pivot, cropped UVs, rotation, flips, determinism.
- Camera2D world/screen round-trip, aspect ratio, movement, zoom, and Vulkan
  projection center.
- Native SceneLoader: texture decode, sprite metadata, instance fields, camera,
  animation references, missing sprite references, and missing-scene/unknown-animation-target
  errors using committed synthetic scene fixtures.
- The real `r1_1` + `c00` scene is an integration/manual test because it requires
  the local reference bundles and generated proprietary pixels.

### Animation (implemented units)

- AnimationClipLoader: duration, sample rate, loop, update mode, source metadata,
  tracks, malformed input, and missing key fields.
- SpriteTrack: empty/one/multiple keys, exact key boundary, between-key hold,
  end-of-clip hold, duplicate-time determinism.
- AnimationPlayer: Play, Update, current/normalized time, loop count, negative
  delta rejection, non-loop clamp/finish, reset, and sprite evaluation.
- LocomotionAnimator: idle/run transition, reset, current clip/sprite evaluation.
- Synthetic SceneLoader animation fixture resolves sprite IDs and target paths.
- Real `c00` idle and run metadata integration: committed scene/animation metadata
  loads, every key resolves to a real SpriteData ID and texture, and atlas
  rectangles are within texture bounds. Proprietary PNG bytes are not required
  for this metadata test; runtime rendering uses local generated assets.

### Locomotion / collision / camera (implemented units)

- Keyboard movement: cardinal directions, diagonal normalization, zero input,
  opposing keys.
- PlayerLocomotion: recovered speed/speed_rate equation, fixed dt, repeated
  ticks, stationary player.
- AABB collision: room bounds, solid wall, wall sliding, no penetration.
- SceneLoader spawn/collision-bounds parsing.
- Camera2D follow: smoothing, deterministic repeat, snap mode, aspect center.
- PlayerRenderSync: c00 body follows the simulated position exactly, shadow
  keeps the recovered `(0,-0.05)` offset, non-player instances stay authored,
  and animation frames plus camera/viewport changes cannot move either render
  transform.
- Direction: eight-way digital input, opposing-axis cancellation, cardinal vs
  diagonal magnitude 9.75/0.195, horizontal-only facing resolution, body flip,
  and run/idle selection for every movement direction.
- Character switching: recovered body sprite and idle/run clip source changes
  while position, render binding, shadow offset, movement and camera persist.
- Obstacle variants: all 12 recovered obstacle list entries in metadata, the
  8 solid selectable variants, marker-cell equality between rendered and
  collision geometry, and cycle wrap. Decorative entries without a collider
  are intentionally excluded from selection.

### Door animation / room visual correspondence (implemented units)

- Recovered r1_1 `door_open`/`door_close`/`door_close2` clips: duration, sample
  rate, key times/values, sprite switches, visibility, trigger timing, and E/W
  vs N/S close differences.
- `Door` state machine: `OPEN`, `CLOSING`, `CLOSED`, `OPENING`; solid/closable
  transition timing; ornament extent; leaf sprite; shadow/ornament visibility.
- `RoomDoorController`: collision feed across all four orientations, one-way
  normals, trigger-time rebuild, and animated south-door blocking/passage.
- r1_1 visual/collision correspondence: 140 wall cells ↔ 420 wall instances,
  49 obstacle markers ↔ 8 selectable solid recovered variants (12 entries in metadata); no placeholder `skin` markers,
  no obstacle sprites baked into static instances, door role bindings, spawn
  free of recovered solids.
- Upper-right artifact regression: every `native_wall/...@x,y` sprite position
  equals `(x,y)`, and no scene instance lies outside the recovered 41×41 map
  cell. This catches the prefab-root-offset bug by source/path identity, not by
  screen coordinates.

### Assets / energy / inventory / save / replay (implemented units)

- Asset audit metadata and generated-pixel presence; audit report generation.
- `EnergyState` clamps, affordability and zero-cost weapons.
- Weapon energy consumption per accepted shot and rejection below cost.
- Two-slot `WeaponInventory` independence, switching, wraparound safety.
- Weapon pickup nearest-target selection, boundary radius and animation.
- Enemy pursuit speed/aggro range, collision sliding, contact cooldown, death.
- Native save round-trip, missing/corrupt files, backup fallback, version 0
  migration, future-version rejection, invalid ranges, delete and atomic write.
- Interactive replay loader: action mapping, commands, corrupt/empty/missing
  files, unknown future action tolerance.

### Combat prototype (implemented units)

- `Weapon_000` attack cycle: first projectile after the recovered 1/30 s
  `Attack` event, one shot every 1/3 s while held, release resets the state, and
  a zero/invalid cycle is rejected.
- Projectile definition: 36 effective speed from BulletInfo, prefab speed 10
  retained as metadata, damage 3, camp 1, through 0, repel 2, lifetime 5 s.
- Projectile simulation: deterministic 0.72-unit first step at 50 Hz, five-second
  expiry, wall and obstacle blocking, and a wall-hit fraction that rejects
  enemies behind the wall while still allowing an enemy in front.
- Damage: armour absorption, armour overflow into HP, defence subtraction,
  lethal/non-lethal HP, same-camp rejection, and dead-state rejection.
- Enemy: hp 12, projectile damage, death, immediate collision/target skip,
  dead-enemy no-retarget, 4.0 s despawn timer, and recovered 3 damage / 1.5 s
  contact cadence.
- Movement: `atk_move_speed = -0.1` reduces effective speed from 9.75 to 9.1
  while attack is held.
- Real scene metadata: player stats 6 hp / 5 armor, weapon, two deterministic
  e_slime01 fixtures, and recovered body/dead/shadow sprites.
- Rendering regression: flipping a sprite mirrors quad geometry around its
  pivot, so an asymmetric player pivot no longer shifts the body relative to the
  shadow when facing changes.

### Developer UI / registries / asset inventory (implemented units)

- Generated developer font atlas: dimensions, RGBA buffer, text width/line height.
- WeaponRegistry: recovered definition load, active selection, cycle/wrap.
- EnemySystem developer spawner: spawn, clear, and reset back to scene fixtures.
- Scene metadata: four weapon definitions with visual/projectile sprites and
  attack timing; player stats; two enemy fixtures.
- Asset inventory generation and metadata audit: unique IDs, sprite bounds,
  pivots/PPU, animation references, weapon/enemy/projectile references,
  redistribution status. Structural errors fail; generated-pixel absence is a
  local-only warning unless `--require-generated` is used.

### Integration / manual smoke

```bash
./tools/smoke_phase4.sh
# or: ./build/soul-knight --smoke-seconds 10
```

Requirements: real Wayland session, SDL3, Vulkan 1.3, working GPU. The script
checks driver, Wayland enforcement, validation status, swapchain/renderer markers,
clean smoke completion, and rejects validation/renderer errors. It is intentionally
not registered with CTest.

### Native `r1_1` reconstruction test scene (manual)

Requires the local reference bundle tree and generated texture assets (never
committed):

```bash
tools/asset/.venv/bin/python tools/asset/build_visual_scene.py
cp -a assets/generated/visual/r1_1_visual build/assets/scenes/r1_1_visual
PROJECT_ROOT="$(git rev-parse --show-toplevel)"
cd /tmp
"$PROJECT_ROOT/build/soul-knight" \
    --scene scenes/r1_1_visual/scene.json --smoke-seconds 7
```

The committed metadata lives in `analysis/assets/scenes/r1_1_visual.json` and
`analysis/assets/scenes/animations/`; generated PNG/scene bytes remain
gitignored. The scene now has 482 static instances (420 walls + 60 doors +
2 player), 182 sprite assets, 14 textures, 30 recovered character clips,
49 obstacle markers with 12 recovered variants (8 solid selectable), one
weapon/projectile pair, and two e_slime01 fixtures. Its current render batch is
594 sprites (static room + obstacles + two enemies + equipped weapon + active projectiles). This
scene is a reverse-engineering test reconstruction, not a proven canonical
gameplay level.
Run with `--locomotion-debug` to log `sim`/`render`/`body`/`shadow`/camera
positions, input, velocity, animation state/frame, facing, active character,
active obstacle variant, HP/armor/dead state, enemy HP/active state, active
projectiles, collision blocking, animated door state, and door-trigger
enter/exit events.

Controls:

```text
1..9, 0   select recovered character c00..c09
[ / ]     cycle all 15 recovered character prefabs
P         cycle recovered obstacle variants
J / Space / LMB   attack (native test binding)
Mouse / right stick / keypad 8,2,4,6   aim vector (facing remains movement-driven)
F6 / F7 / F8      character / weapon / enemy selector
F9 / F11 / F12    spawn enemy / spawn x5 / clear enemies
F10               developer overlay
R         full deterministic developer reset (player, HP/armor, enemies, doors,
          obstacles, weapon, character, projectiles)
O         toggle OpenDoor()/CloseDoor()
C         CloseDoor() on all four doors
```

Character switching changes only the active body sprite/clip pair. Obstacle
switching keeps the 49 marker cells and derives both sprites and collision from
the same recovered variant. Doors replay the recovered
`door_open`/`door_close`/`door_close2` timing; closed doors use the recovered
one-way inward barrier, and opening collision becomes passable at the recovered
`m_IsTrigger` key (0.133333 s). The test spawn is `(20,15)` because `(20,20)` is
inside the recovered obstacle block. Deterministic tests are in
`tests/unit/door_test.cpp`, `tests/unit/player_render_sync_test.cpp`,
`tests/unit/direction_test.cpp`, `tests/unit/character_switch_test.cpp`,
`tests/unit/obstacle_field_test.cpp`, `tests/unit/room_visual_test.cpp`,
`tests/unit/combat_test.cpp`, `tests/unit/devroom_test.cpp`, and `tests/unit/scene_test.cpp`. The suite is
129 cases / 5090 assertions in both RelWithDebInfo and Debug ASan/UBSan builds.

### Assets / energy / inventory / save / replay (implemented units)

- Asset audit metadata and generated-pixel presence; audit report generation.
- `EnergyState` clamps, affordability and zero-cost weapons.
- Weapon energy consumption per accepted shot and rejection below cost.
- Two-slot `WeaponInventory` independence, switching, wraparound safety.
- Weapon pickup nearest-target selection, boundary radius and animation.
- Enemy pursuit speed/aggro range, collision sliding, contact cooldown, death.
- Native save round-trip, missing/corrupt files, backup fallback, version 0
  migration, future-version rejection, invalid ranges, delete and atomic write.
- Interactive replay loader: action mapping, commands, corrupt/empty/missing
  files, unknown future action tolerance.

### Combat manual verification (2026-10-31)

Run from `/tmp` against the generated scene:

```bash
build/soul-knight --scene scenes/r1_1_visual/scene.json --locomotion-debug \
    --smoke-seconds 12
```

Observed with real SDL input through `/dev/uinput` (`ydotool`) against the focused
Wayland window:

- Hold attack: five `Attack:` rows at roughly 1/3 s spacing, each spawning one
  `bullet_0` at speed 36 / damage 3; the right-hand `e_slime01` reached 6/12,
  3/12, then 0/12 and `dead=true`.
- An upward shot into the recovered central obstacle logged
  `Projectiles: world_hits=1 lifetime_expired=0 active=0`, confirming the
  projectile stopped on recovered room geometry rather than passing through.
- The dead enemy's own sprite remained for 4.0 s, then `active=false`; collision
  and retargeting stopped immediately at death.
- Contact with the left slime produced `Player damaged` rows at 1.5 s spacing:
  armor 5 -> 2, HP 6 -> 5, HP 2, then HP 0 / dead.
- The dead player did not produce any `Attack:` rows; `R` logged
  `Combat reset`, restored HP 6 / armor 5 / spawn `(20,15)` / both enemies, and
  attack worked again.
- After moving left (facing left), keypad-right aim plus attack logged
  `aim=(1.000,0.000)` while the same tick reported `facing=left`; the right
  enemy took damage. This confirms movement, facing and aim are independent.
- Character switching (c00 -> c01 -> c00), obstacle switching (`wall603` ->
  `box04`), door close/open, and camera/window exercise all remained functional.
- Every sampled `locomotion:` row had
  `sim == render == body` and shadow offset `(0,-0.05)`; projectiles and enemies
  are drawn from their simulation positions.
- No upper-right stray wall geometry returned.

The recovered room is smaller than the 5 s projectile lifetime in every
direction, so wall/obstacle collision and the 5 s expiry are additionally covered
by deterministic unit tests rather than waiting for an in-room expiry.

### Developer devroom UI (manual)

```bash
./scripts/dev.sh
```

Observed on the real Wayland/Vulkan devroom:

- `F10` shows the overlay with LiteKnight/version/build, FPS/frame time, fixed
  ticks, active character/weapon, HP/armor, enemy/projectile counts, position,
  facing, room, renderer and platform.
- `F6` shows all 15 recovered character sprites with C00..C14 labels and current
  selection. `D` + `Enter` selected c01 and preserved player/camera/collision.
- `F7` lists the four recovered weapons with their real sprites and stats. `D` +
  `Enter` equipped `weapon_002`; subsequent attack logs use its projectile and
  cadence.
- `F8` opens the enemy selector. `F9` spawned one `e_slime01`, `F11` spawned
  five, and `F12` cleared all dynamic enemies. `R` restored the scene fixtures.
- The generated developer text atlas is a documented PLACEHOLDER; all character,
  weapon and enemy imagery in the selectors is recovered native texture data.

### Unity Texture2D conversion / native round-trip (manual)

Requires the local reference APK bundle tree (never committed):

```bash
tools/asset/extract.sh convert-texture \
    --bundle level-objects \
    --path-id -8989119030688471403 \
    --name f903

build/soul-knight-png-roundtrip \
    assets/generated/texture-conversion/f903_-8989119030688471403/f903_-8989119030688471403.png \
    assets/generated/texture-conversion/f903_-8989119030688471403/f903_-8989119030688471403.rgba8 \
    16 24
```

The converter records hashes in
`analysis/assets/texture_conversion_manifest.json`; generated bytes stay under
gitignored `assets/generated/`. Determinism is checked by running the conversion
twice and comparing PNG/raw/manifest SHA-256. The native tool uses the production
`PngDecoder` and compares bytes exactly. This is an integration/manual test because
the reference bundle is not part of the repository.

### Assets (partially implemented)

- PNG decode dimensions/format/bytes: implemented and tested.
- Unity Sprite rect/pivot/PPU/UV and real animation keys: implemented and tested.
- Room JSON node/collider/spawn/door-animation parse: implemented and tested.
- Manifest path-ID/hash stability: extraction tooling exists; native registry not.
- Missing asset → clear error: tested for PNG and animation metadata.
- No proprietary bytes tracked: checked in the repository hygiene pass.

### Simulation

- Movement: keyboard vector, diagonal normalization, fixed-step collision stop.
- Collision: swept room/wall AABB, sliding, no penetration; door triggers and
  exact generated wall gaps remain future work.
- Projectile: spawn, lifetime, hit, despawn.
- Damage: armor overflow, shield, enemy HP, box hit, death.
- Death/restart: flags, duplicate-callback prevention, world reset.

### Integration

- `r1_1`: spawn → move → shoot → hit enemy → enemy death → player damage → death →
  restart.
- Headless/offscreen mode where possible.

### Regression

- Reference values: fixed timestep 0.02, prefab stats, collider bounds, damage
  constants, RNG vectors labelled as reconstructed (not runtime-verified).
- No tests based on candidate display names or invented gameplay.
- Deterministic tests only where evidence supports determinism.

## 3. Rules

- Never weaken a test to make a gate green.
- Never add a test that asserts a hypothesis as fact; label it or omit it.
- Every new subsystem gets at least one failing-capable test.
- Keep the count visible; explain any change in the audit.
- Sanitizer failures are failures, not warnings.
