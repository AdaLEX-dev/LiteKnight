# LiteKnight — Project State

**Version:** `0.5.4-dev`
**Last Updated:** 2026-11-01 (0.5.4 reference pass: 168 standard weapons,
main-menu S/L and direct-area launches, hub cleanup/collision audit, orb trader,
audio, auto-open chests, map/enemy/room catalogues)
**Commit:** session head; run `git log -1 --format='%h %s'`
**Phase:** Phase 2 **PARTIAL** (Gate B PARTIAL); Phase 3 **PARTIAL**; Phase 4 **PARTIAL (devroom foundation FUNCTIONAL)**
**Current status:** the native Wayland/Vulkan devroom is playable and is the
primary development environment. It includes the recovered `r1_1` room,
movement/collision/camera, animated doors, obstacles, 15 characters, 168
recovered standard projectile weapons with real textures, deterministic
projectiles, damage, enemy death, player death/reset, live enemy pursuit, a 1920x1080 pixel UI with real
HP/armor/energy HUD, two weapon slots, physical pickups, recovered-font pause/
settings/main-menu screens, character/weapon stations, a recovered Living Room
hub, 360° mouse aiming (Auto Aim fresh default), weapon energy costs, Knight
Dual Wield, a three-room deterministic dungeon graph over actual PatternRooms,
persistent logical HUD layout editor, compact reference-derived HUD minimap,
actual chest and world coin pickups, recovered main-menu/HUD/settings/pause
assets, corrected global text centering and direct developer-area launches, native save/load, an asset laboratory, and a serialized
interactive test/replay harness. Full EnemyAI04 scouting/pathfinding, reference procedural dungeon
generation, production forge/plant economy and the full catalogue remain
incomplete. Gate D remains unmet for a complete game, but the
permanent devroom invariant is maintained.

---

## Current Phase and Gates

| Phase | Status |
|---|---|
| Phase 0 — Environment | ✅ COMPLETE |
| Phase 1 — APK Forensics | ✅ COMPLETE (artifact inventory) |
| Phase 2 — Engine Reconstruction | ⚠️ PARTIAL — Gate B **PARTIAL** |
| Phase 3 — Behavioural Reconstruction | ⚠️ PARTIAL — vertical-slice contract largely known; full-game content UNKNOWN |
| Phase 4 — Native Runtime | ⚠️ PARTIAL — foundation FUNCTIONAL; Gate C **PARTIAL** |
| Phase 5 — First Playable | ⚠️ DEVROOM PLAYABLE; full Gate D NOT MET |
| Phase 6+ — Systems/Content/Fidelity/Release | ⛔ BLOCKED |

Canonical baseline:

- Soul Knight 2.6.8 (`com.ChillyRoom.DungeonShooter`, versionCode 20680)
- Unity **2018.4.12f1**
- IL2CPP metadata **v24**
- APK SHA-256 `8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea`
- CachyOS x86-64
- Wayland only
- Vulkan target
- 1920×1080, 60 FPS target
- native executable
- no Android runtime, Waydroid, emulator, Wine, X11, or XWayland

Canonical reports:

- Gate B: `docs/GATE_B.md` — **PARTIAL**
- Phase 3: `docs/PHASE_3_BEHAVIOURAL_RE.md` — **PARTIAL**
- Phase 4 plan/state: `docs/PHASE_4_NATIVE_RUNTIME.md` — **PARTIAL**
- Phase 4 runtime audit: `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`
- Room topology and traversal: `analysis/re/room_topology.md`
- Machine-readable wall map: `analysis/re/r1_1_wall_map.json`
- Asset pipeline: `docs/ASSET_PIPELINE.md` — **PNG path FUNCTIONAL; conversion/registry PARTIAL**
- Gate B/Phase 3 final audit: `analysis/GATE_B_PHASE_3_FINAL_AUDIT.md`
- Evidence standard: `docs/EVIDENCE_STANDARD.md`

---

## Confirmed in this pass

### Timing

Serialized `TimeManager` in `globalgamemanagers`:

```text
Fixed_Timestep = 0.019999999552965164   // 0.02 s / 50 Hz
Maximum_Allowed_Timestep = 0.33333334
m_TimeScale = 1.0
```

No direct xref to `Time.set_fixedDeltaTime`; the build-time 50 Hz step is the
reference. Render target remains 60 FPS. See `analysis/re/timing.md`.

### Damage bridge

- Player: `RGBulletTrigger.OnTriggerEnter2D` → `GetComponent<RGController>`
  (`0x30E8F80`) → `RGController.GetHurt` vtable `+0x228` (slot 16,
  `0x11795CC`).
- Enemy: same method → `GetComponent<RGEController>` (`0x309A4E8`) →
  `RGEController.GetHurt` vtable `+0x2C8` (slot 26, `0x1108E60`).
- Box: `GetComponent<RGBox>` (`0x31003B0`) → `RGBox.Hit` vtable `+0x168`
  (`0x11675F8`).

`CanDie` resolves `Singleton<RGGameProcess>.get_Inst()` and checks
`RGGameProcess.this_index` (`+0x1C`) against `-1`/`-3`; the earlier
`RGGameInfo.map_random_seed` interpretation is wrong. See `analysis/re/damage.md`.

### RNG

- Core Unity 2018.4 RNG algorithm, wrapper, map seed, and LongMap branch are
  reconstructed. `MapManagerLevel.SetRGRandomSeed` adds a 50% `map_long` increment
  for Normal mode, `this_index` 1..15, LongMap active.
- `seed_state` 1/2 remains unused/UNRESOLVED; single-player paths use 0/3.
- Reference vectors are **NOT AVAILABLE**. See `analysis/re/rgrandom.md`.

### Assets / room / entities

- `r1_1`: 178 transforms, 109 sprites, 12 box colliders, 4 doors, 4 door triggers,
  8 platform effectors, 4 animators; `room_width=15`, `room_height=15`.
  Runtime wall collision comes from `RGRoomX.CreateWall` instantiating
  `MapManagerLevel.wall_list` 1×1 collider prefabs on runtime layer `Edge` (17),
  not from the 12 serialized colliders.
- `MapManagerLevel` places rooms on a 41-unit grid; `RGRoomX.OnSetRGRandomSeed`
  gives 15×15 rooms offsets `(13,13)`. `CreateWall` places horizontal wall rows at
  `y=12/28` and vertical columns at `x=12/28`, skipping the five cells `18..22`
  on every side. The resulting interior is `[12.5,13]..[27.5,28]`; the 44 room
  wall cells and four five-cell openings are recovered.
- Wall prefabs `wall601`/`wall602` (level 1-1 manager, bundle `level-objects`)
  have identical 1×1 non-trigger `BoxCollider2D` at offset `(0,0.5)`, tag
  `Wall`. The prefab is serialized on layer 13 (`Wall`); `RGRoomX.cctor` sets
  `EdgeLayer=17` and generated wall instances are assigned to layer 17 (`Edge`).
  Variant selection is random per cell; collision is identical.
- Four serialized doors have barriers `5×1` (N/S) or `1×5` (E/W), trigger
  volumes `5×1.5` / `1.5×5`, and exact world rectangles recovered in
  `analysis/re/r1_1_wall_map.json`. Door Animator clips set open doors to
  `isTrigger=true`; closed doors restore the full solid collider with a
  one-way inward `PlatformEffector2D` (blocks exit, permits aisle entry).
  `RGDoorTrigger` events fire only while `room.door_open` is true and carry the
  room name and north/south/west/east direction.
- Runtime entrance bits are assigned by `MapManagerLevel.CreateRoom` from
  `room_relation`: `[east,south,west,north]`. The serialized `r1_1` entrance
  `[0,0,0,0]` is the template default; actual map instances differ. The native
  test scene deliberately opens all four sides as a labeled test configuration.
- Player spawn is `MapManager.centerPoint`; `b_point` children populate
  `RGRoomX.floor_list`. `CloseDoor`/`OpenDoor` iterate all four doors;
  `ClearRoom`/`IsClear` use `EnemyMaker`.
- `c00` physical collision is a child `CircleCollider2D` radius 0.4 at
  `(0,0.5)` on layer `Body`; the root `BoxCollider2D` is a trigger interaction
  box. Movement uses `Rigidbody2D.velocity` and Unity Physics2D.
- `c00` → `C01Controller` + `RoleAttributePlayer` (speed 6.5, hp 6, armor 5, atk 4,
  critical 5, c_index 0).
- `c01` → `C02Controller` (speed 6.5, hp 5, armor 3, atk 5, critical 10, c_index 1).
- `e_slime01` → `EnemyAI04` + `RoleAttribute` (hp 12, speed 3.0, damage 3,
  enemyTag 66, levelScene 20).
- `weapon_000` → `Gun001` (bullet `bullet_0`, speed 36, damage 3, critic 0,
  deviation 5, consume 0, weapon_type 14). Attack path:
  `Gun001.Attack` → `CreateBullet` → `BulletFactory.TakeBullet`.
- `bullet_0` → `Bullet01` (prefab speed 10, destroy_time 5, can_rebound 1,
  camp 1). `RGBullet.UpdateInfo` overwrites speed with the weapon's BulletInfo
  speed 36, so 36 is the effective initial speed; 10 is a prefab default.
- `e_slime01` contact damage: `EnemyAI04.OnTriggerEnter2D` →
  `HitUnit(other, 3, 1.5)`; see `analysis/re/enemy_ai04.md`.
- Save boundary: `PlayerSaveData` uses Unity `PlayerPrefs` string/int keys; exact
  `AboGameData` payload is UNKNOWN and not required for the slice. See
  `analysis/re/save.md`.

Display names (Knight/Rogue/slime/pistol) remain **CANDIDATE**, not proven. See
`analysis/re/rooms_scene.md` and `analysis/re/weapons_projectiles.md`.

### Input / movement

Legacy `Input` axes (WASD + joystick, deadzone 0.19), `MovementInput {dir, weight}`,
`C01Controller.FixedUpdate` → `RGController.SetVelocity` (vtable slot 21,
`0x117A6E4`), speed derived from `RoleAttributePlayer.speed`/`speed_rate`. See
`analysis/re/input_movement.md`.

---

## Phase 4 runtime foundation (new)

Implemented and manually verified on 2026-10-31:

- `Application` lifecycle with explicit ownership and idempotent shutdown.
- SDL3 video/events, `SDL_HINT_VIDEO_DRIVER=wayland`; post-init driver check
  rejects every non-Wayland backend.
- Window state for resize, pixel-size changes, minimize, restore, focus, and
  close; event handling reconciled against SDL window flags.
- Vulkan instance API 1.3, `VK_KHR_surface` + `VK_KHR_wayland_surface`,
  Debug validation layers, physical-device selection, graphics/present queues,
  `B8G8R8A8_SRGB` FIFO 3-image swapchain, render pass, 2 frames in flight.
- GLSL 450 sprite shaders compiled to SPIR-V by a CMake `glslc` target.
- Batched textured quads with UVs, tint, alpha blending, stable layer sorting,
  synthetic PNG fixtures, staging upload, aspect-fit resize.
- 50 Hz fixed simulation consumed from the existing `FrameTimer`; rendering is
  decoupled and targets 60 FPS.
- PNG asset runtime: libpng 1.6 system dependency, canonical RGBA8
  `TextureData`, exact-byte/palette/gray/RGB tests, malformed-input rejection,
  executable-relative synthetic fixtures, decoded PNG rendered through the
  existing staging/`Texture2D`/`SpriteBatch` path.
- **Controlled Unity Texture2D gate PASSED:** one real `level-objects`/`f903`
  Texture2D (`RGBA32`, 16x24, path ID `-8989119030688471403`) converted through
  UnityPy 1.25.4 + Pillow 13.0.0 to canonical RGBA8 and a deterministic PNG;
  provenance recorded in `analysis/assets/texture_conversion_manifest.json`.
  Conversion was deterministic across two runs, native `PngDecoder` round-trip
  matched all 1536 bytes, and the real PNG rendered through the runtime.
- **Unity-derived sprite/scene gate PASSED:** the native `r1_1` reconstruction
  test scene renders 109 room sprites plus `c00` body/shadow from the real floor,
  character, and object-common atlases with serialized Sprite rect/pivot/PPU=16,
  Unity sorting layer/order, `Camera2D`, and the fixed multi-texture sprite batch.
  Scene metadata: `analysis/assets/scenes/r1_1_visual.json`. This scene is a
  controlled reverse-engineering reconstruction, **not** a proven canonical
  gameplay level.
- **Unity idle/run-animation gate PASSED:** `c00` Animator controller
  `char_0_skin_0` provides `Base Layer.ide` (`char_1_skin_0_ide`) and `run`
  (`char_1_skin_0_run`), both 0.5 s at 8 Hz, looping, with four real frame
  sprites each from the character atlas. Native `AnimationClip`/
  `AnimationPlayer`/`LocomotionAnimator` select idle/run at fixed 50 Hz.
  Metadata: `analysis/assets/scenes/animations/c00_idle.json` and
  `analysis/assets/scenes/animations/c00_run.json`.
- **Keyboard locomotion gate PASSED:** SDL3 keyboard actions feed normalized
  `MovementInput`; `PlayerLocomotion` applies the recovered
  `speed=6.5`/`speed_rate=0.5` equation (`9.75` effective units/s) only in the
  fixed 0.02 s tick; swept AABB collision and `Camera2D` follow are verified.
  Evidence/design: `analysis/assets/locomotion.md`.
- **Room topology / door traversal gate PASSED (native reconstruction):**
  `RGRoomX.CreateWall` and `RGAisle.CreateAisleWall` are reconstructed; the
  native scene consumes 44 room wall cells, 96 corridor wall cells, four exact
  door barriers/triggers, and the closed-door one-way normals. Open doors are
  passable; closed doors block exit and permit aisle entry; door triggers emit
  direction/room payloads only while `door_open`. The 41×41 map-cell edge is an
  explicit native test boundary. Evidence: `analysis/re/room_topology.md`,
  `analysis/re/r1_1_wall_map.json`.
- **Player simulation/render synchronization gate PASSED:** the rendered c00
  body now follows `PlayerLocomotion` exactly through an explicit
  `PlayerRenderSync` binding; the shadow keeps its recovered `(0,-0.05)` offset.
  Animation and camera follow cannot change player world position. Runtime
  `--locomotion-debug` logs `sim`, `render`, `body`, `shadow` and `camera`; unit
  tests cover animation and camera/viewport changes.
- **Door animation gate PASSED (native reconstruction):** both recovered
  Animator controllers/clips (`door_open`, `door_close`, `door_close2`) are
  exported from the serialized StreamedClip/PPtr curves. `Door` exposes
  `OPEN`/`CLOSING`/`CLOSED`/`OPENING`; `RoomDoorController` drives rendering
  (leaf sprite `door02`/`door03`, moving `d` ornament, shadow visibility) and
  collision from the `m_IsTrigger` curve. **O** toggles OpenDoor/CloseDoor and
  **C** issues CloseDoor. Native collision uses the full barrier during the
  non-trigger interval; transient `m_Size` thinning is a documented
  approximation.
- **Obstacle replacement / wall visual correspondence PASSED:** r1_1's 49
  `RGObjectSkin` markers (all `obj_index=0`) are replaced by `wall603`, matching
  `RGRoomX.CreateRoomObstacle`; the placeholder gray `skin` grid is gone. The
  scene carries 140 wall AABBs with 420 wall sprites, and 49 obstacle AABBs with
  147 obstacle sprites. The test spawn moved to `(20,15)` because the c00
  prefab origin `(20,20)` is inside the recovered obstacle block.
- **Dev-room cleanliness PASSED:** the upper-right stray wall geometry was caused
  by treating the `wall601` prefab asset root local position `(20,20)` as a
  sprite offset. `CreateWall` overrides the root position with the wall-cell
  coordinate; the builder now does the same. A regression test asserts every
  wall instance position equals its `@x,y` cell and no instance escapes the
  41×41 map cell.
- **Eight-way movement / facing PASSED:** all cardinal and diagonal inputs are
  normalized; diagonal speed equals the recovered 9.75 cardinal speed; opposing
  axes cancel. The recovered controllers contain only `ide`/`run` (plus
  dead/hit), with no directional clips. `RGController.facing` mirrors the `img`
  hierarchy 180° around Y for left; native rendering uses body-only horizontal
  flip and retains facing on pure vertical input.
- **Character / obstacle test controls PASSED:** all 15 recovered c00..c14
  character prefabs and 30 ide/run clips are selectable with 1..0 and [/]; the
  body sprite and clip source change without moving the player, collision, or
  camera. `P` cycles the 8 recovered solid `obstacle_list` variants (4 decorative
  no-collider entries remain in metadata but are not selectable); render and
  collision cells are derived from the same active variant.
- **Permanent devroom infrastructure PASSED:** `build/bin/liteknight-devroom`
  plus `--devroom`; `scripts/dev.sh` builds, tests, prepares local assets and
  launches, refusing to run a known-broken executable. Version `0.1.0-dev` and
  build hash are shown in the developer overlay. Source-controlled asset
  inventories and metadata audits live in `analysis/assets/` and
  `tools/asset/`; reference-derived pixels remain local-only.
- **Developer UI PASSED:** `F6` character selector (all c00..c14 with real
  sprites), `F7` weapon selector (168 recovered standard projectile weapons with real
  weapon textures), `F8` enemy selector, `F9`/`F11`/`F12` spawn one/five/clear,
  and `F10` overlay (version/build/FPS/ticks/player/weapon/enemies/projectiles/
  position/facing/room/platform). Selectors preserve player position, collision,
  camera and combat state. `R` is a full deterministic developer reset.
- **First playable combat prototype PASSED (native reconstruction):** the
  dev room now runs `player -> weapon_000 -> bullet_0 -> wall/obstacle/enemy ->
  damage -> enemy death -> player damage/death -> R reset`. Attack is a native
  test binding (`J`/`Space`/LMB) because the recovered mobile path is
  `RGJoyControl.UpdateBtnAtkClick -> RGController.BtnAtkClick -> SetAttack ->
  AttackStart -> set_atk_b`; aim is a separate native test vector (mouse/right
  stick/keypad) and facing remains movement-driven. Four recovered Gun001
  weapons are selectable: `weapon_000`, `weapon_002`, `weapon_048`,
  `weapon_287`; each preserves its own attack-cycle/event timing and real sprite.
  `weapon_000`'s `w_pistol` loops for 1/3 s with its `Attack` event at 1/30 s;
  the effective projectile is speed 36, damage 3, camp 1, lifetime 5 s.
  `bullet_0` prefab speed 10 is retained as metadata. `e_slime01` is the test
  target: hp 12, contact damage 3 every 1.5 s. Player stats are the recovered
  c00 RoleAttributePlayer (6 hp, 5 armor, camp 1). Enemy colliders are disabled
  immediately on death and the dead sprite remains for the recovered 4.0 s
  `dead_destory_delay_time`; the 0.95 write is friction, not a death delay.
  Player death stops movement/attack and shows the recovered final dead sprite;
  full game-over/reborn UI is out of scope. Evidence:
  `analysis/re/combat_prototype.md`, `analysis/re/weapons_projectiles.md`,
  `analysis/re/damage.md`, `analysis/re/enemy_ai04.md`.
- **Facing visual regression fixed:** sprite flipping now mirrors quad geometry
  around the pivot (not UVs only), keeping the asymmetric c00 pivot body anchored
  over its shadow when facing changes. Regression test:
  `tests/unit/render_test.cpp`.
- Sprite metadata/camera/animation evidence: `analysis/assets/sprites.md`,
  `analysis/assets/coordinate_system.md`, `analysis/assets/animation.md`.
- Manual smoke from `/tmp` (path-independent): PNG decoded, real scene rendered,
  0 validation messages, clean exit.
- Non-Wayland startup fails with exit 1 (`wayland not available`), no X11
  fallback.

Not implemented: full AnimatorController transitions/layers/blend trees, other
clips/states, the exact event-to-map-manager traversal listener, room-clear
lifecycle, full enemy AI and production enemy spawning, production player
death/game-over/reborn UI, critical/element/effect triggers,
mounts/one-shot shields/badass modifiers, adjacent-room generation/coordinate
updates, general KTX2/atlas packing, the full asset registry/manifest loader,
recovered UI/font assets (developer text is a PLACEHOLDER), audio, content, or
parity comparison. Native sorting numbers are mapped to recovered
TagManager layers; camera size, camera follow smoothing, the AABB-vs-circle
player collision approximation, linear replay of the recovered streamed keys,
transient door-collider size animation, and fixed-step rather than Normal-update
sampling remain **PROJECT DECISIONS/APPROXIMATIONS** because exact reference
behavior is not recovered.

Evidence and limitations: `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`.

---

## Gate B blockers (must close or explicitly accept)

1. Wall/door gap placement, door collider geometry, door open/closed collision
   states, the recovered door animation clips, and `RGObjectSkin` obstacle
   replacement are now CONFIRMED and implemented. Remaining PARTIAL items:
   the exact entrance bits of any generated `r1_1` instance and the event-to-
   `MapManagerLevel` traversal listener.
2. `Gun001` fire cadence is now recovered from the serialized `w_pistol` state
   and Attack AnimationEvent (1/3 s loop, 1/30 s first shot), and the spread
   field is `RoleAttribute.deviation * (weapon.deviation + 1)` (0° for c00).
   Rigidbody2D projectile integration, rebound/through effect triggers, and the
   exact Unity input button/aim routing remain PARTIAL.
3. Full death/reborn UI sequencing and virtual slot owners (`+0x328`, `+0x3A8`,
   `+0x3E8`) remain PARTIAL. The native slice implements minimum player/enemy
   death + developer reset; reborn-card and production game-over flow are
   deferred. Enemy destruction delay is 4.0 s; 0.95 is friction, not death delay.
4. Full `EnemyAI04`/other AI, all weapons, procedural generation, and content
   identities remain Phase 3 breadth. Minimal AI04 contact behavior is known.
5. Reference save payload/versioning remains UNKNOWN. A native versioned JSON
   save (`src/game/save`, slot 0, atomic write + backup) is implemented; it is
   not claimed to match the original RGSaveManager format.
6. No approved runtime oracle; static analysis only.

---

## Build and Test Verification

Recorded in `analysis/REPOSITORY_INTEGRITY_AUDIT.md` and the final audit. Current
baseline:

- Debug clean configure/build: PASS, 0 warnings
- Debug tests: 132 cases / 5113 assertions, PASS (ASan/UBSan)
- Negative control: CTest FAIL (exit 8), restored PASS
- RelWithDebInfo clean configure/build: PASS, 0 warnings
- RelWithDebInfo tests: 132 / 5113, PASS
- Debug Vulkan validation: enabled, 0 messages; Debug and RelWithDebInfo smoke PASS
- Real keyboard/mouse combat, death/reset, character/obstacle switching, doors,
  and window resize/minimize/restore: PASS
- `git diff --check`: clean
- `git status`: clean after commit

The count changed from 71/4638 to 87/4796 in this development pass because new
deterministic tests cover the weapon Animator cycle, projectile speed/lifetime
and wall/obstacle/enemy collision ordering, armor/HP/defence damage, enemy
death/despawn, the attack movement modifier, combat scene metadata, and pivot-
correct sprite mirroring. No existing test was removed or weakened. The real
scene conversion/smoke remains manual because it requires the local reference
bundles and a Wayland/Vulkan GPU.

---

## Next Action

Phase 4 foundation, PNG, controlled Unity Texture2D, SpriteData, camera, the
`r1_1` native reconstruction test scene, real idle/run animation, keyboard
locomotion, recovered room perimeter/aisle/door geometry, native door traversal,
and the first-slice combat loop are **FUNCTIONAL**. No further combat breadth
(full AI, room clear, production death/reborn, critical/element effects) is
authorized by this milestone. The next bounded investigation remains the
higher-level room transition path: resolve the listener(s) for
`EnterDungeonRoomEvent`/`ExitDungeonRoomEvent`, prove how
`MapManagerLevel.IntoAisle` and room-coordinate updates are invoked, and decide
whether one adjacent room can be reconstructed without starting procedural
generation. Do not start full AI, menus, or the full catalogue without explicit
authorization.
