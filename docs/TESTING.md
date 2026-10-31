# Testing Strategy

**Status:** current baseline is **55 doctest cases / 1838 assertions**, all passing.
Future tests must be evidence-backed; do not invent expected values.

---

## 1. Current harness

```bash
./tools/configure.sh Debug
./tools/build.sh Debug
./tools/test.sh
ctest --test-dir build --output-on-failure
```

- doctest 2.4.11, CTest entry `soul-knight-unit-tests`.
- Debug: ASan/UBSan.
- RelWithDebInfo: optimized, no sanitizers.
- CTest fails if the filter selects zero cases (`FAIL_REGULAR_EXPRESSION`).
- Negative control: break an assertion, confirm CTest exits non-zero, restore.

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
gitignored. This scene is a reverse-engineering test reconstruction, not a proven
canonical gameplay level. Run with `--locomotion-debug` to log player position,
input, velocity, animation state/frame, camera, collision blocking, door state,
and door-trigger enter/exit events. Press **O** to toggle all four recovered doors
closed/open. Closed doors use the recovered one-way inward barrier; opening them
makes both directions passable. Deterministic tests for all four doors, wall
sliding, and the recovered coordinates are in `tests/unit/scene_test.cpp`.

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
- Manifest path-ID/hash stability: extraction tooling exists; native registry not.
- Room JSON node/collider/spawn parse: not implemented.
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
