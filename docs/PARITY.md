# Soul Knight 2.6.8 — Parity Matrix

> **Current truth (2026-10-31 Gate B / Phase 3 pass).** This matrix is evidence-first.
> `PLANNED` means no native implementation exists. `PARTIAL` means some reference
> behaviour is reconstructed, with named gaps. No row is marked `VERIFIED` without
> a reference comparison. The old speculative matrix remains in Git history and is
> audited in `analysis/EVIDENCE_AUDIT.md`.

**Status definitions:**

- `UNKNOWN` — not reconstructed.
- `PARTIAL` — some evidence/implementation, named gaps remain.
- `PLANNED` — design exists; no native implementation.
- `FUNCTIONAL` — native implementation works but is not reference-verified.
- `HIGH-FIDELITY` — reference-verified with known minor differences.
- `VERIFIED` — reference-verified with regression tests.

---

## Evidence-backed matrix

| Feature | Reference Evidence | Native Status | Confidence | Validation |
|---|---|---|---|---|
| Reference identity | Unity 2018.4.12f1, metadata v24, APK hash | N/A | **CONFIRMED** | `analysis/ARTIFACT.md`, `analysis/EVIDENCE_AUDIT.md` |
| Build/test baseline | N/A (tooling) | Debug + RelWithDebInfo, 71/4638 | **VERIFIED (tooling)** | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` |
| RNG core | Unity 2018.4 Xorshift128, LCG seed, 23-bit floats | `UnityRandom`/`RGRandomStream` implemented | **CONFIRMED (static)** | `analysis/re/rgrandom.md`, `tests/unit/unity_random_test.cpp` |
| RNG wrapper / streams | `seed_state`, map seed, `RandomBehaviour`, `RGController` seed | Native wrapper implemented | **PARTIAL** — seed_state 1/2 and DateTime seed exactness open | `analysis/re/rgrandom.md` |
| Fixed timestep | `TimeManager.Fixed_Timestep = 0.02` (50 Hz) | `FrameTimer` + application loop consume 50 Hz fixed steps | **CONFIRMED reference** | `analysis/re/timing.md`, `tests/unit/timing_test.cpp`, smoke log |
| Frame-loop model | Update vs FixedUpdate; render target 60 FPS | Application event → fixed tick → render/present loop; measured 0.834 sim steps/frame | **FUNCTIONAL (native)** | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` |
| Application lifecycle | N/A (native mechanism) | Bootstrap, Wayland-only enforcement, focused event loop, idempotent shutdown | **FUNCTIONAL** | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` |
| Wayland window | Native target: Wayland only | SDL3 window; 1920x1080 requested; compositor-constrained extent handled | **FUNCTIONAL / manual** | smoke log |
| Vulkan device/swapchain | Native target: Vulkan 1.3 | Intel UHD 620 fallback path, 3-image FIFO swapchain, validation-clean | **FUNCTIONAL** | smoke log |
| Sprite rendering | Reference sprites use Unity layer sorting; TagManager layers recovered | GLSL sprite pipeline, RGBA8 textures, alpha, stable layer/order batch, pivot/rotation | **FUNCTIONAL (native), NOT parity** | `docs/RENDERING.md`, `analysis/assets/sprites.md` |
| Unity Sprite metadata | Sprite rect/pivot/PPU/border/filter/wrap serialized; texture rect bottom-origin | `SpriteData` conversion and tests | **HIGH CONFIDENCE / FUNCTIONAL** | `analysis/assets/sprites.md` |
| 2D camera/world | Unity 2D Y-up, PPU=16; exact camera size not recovered | `Camera2D` orthographic center/half-height, aspect-fit, world/screen tests | **FUNCTIONAL; camera size PROJECT DECISION** | `analysis/assets/coordinate_system.md` |
| Room/player visual composition | `r1_1` + `c00` serialized transforms/sprites + recovered wall/obstacle prefabs | 482 static instances + 147 active obstacle sprites, 156 sprite assets, 12 textures; 589-instance draw batch with all doors open; rendered via Wayland/Vulkan | **FUNCTIONAL (visual), NOT gameplay/parity** | `analysis/assets/scenes/r1_1_visual_manifest.json` |
| Unity sprite animation | `c00` Animator `char_0_skin_0`, states `ide` and `run`; four real frames each | `AnimationClip`/`AnimationPlayer`/`LocomotionAnimator`; discrete keys 0/.125/.25/.375 s, 0.5 s loops; fixed-step evaluation | **HIGH CONFIDENCE binding**, function FUNCTIONAL; Normal-update/transition parity UNKNOWN | `analysis/assets/animation.md`, `analysis/assets/scenes/animations/c00_*.json` |
| Keyboard movement | Legacy Unity WASD axes; normalized `MovementInput`; c00 speed 6.5/speed_rate 0.5 | `MovementInput` + `PlayerLocomotion`; eight-way normalized, effective 9.75 units/s at fixed 0.02 s | **CONFIRMED reference path; NATIVE implementation** | `analysis/assets/locomotion.md`, `tests/unit/direction_test.cpp` |
| Directional character animation | No directional clips; `RGController.facing` mirrors `img` 180° around Y | `FacingDirection::{Right,Left}`; body-only flip_x, shadow untouched; ide/run selected by movement presence | **HIGH CONFIDENCE facing; UNKNOWN extra states** | `analysis/assets/animation.md` §7, `tests/unit/direction_test.cpp` |
| Room collision | 15x15 room, 41-unit map cell, four five-cell gaps, 1×1 wall collider offset (0,0.5), one-way closed doors | Native swept AABB consumes 44 room wall cells + 96 corridor wall cells + four one-way door barriers; sliding/no tunneling; player AABB approximates circle | **HIGH CONFIDENCE geometry; native solver APPROXIMATION** | `analysis/re/room_topology.md`, `analysis/assets/locomotion.md`, `tests/unit/scene_test.cpp` |
| Camera follow | Exact reference follow constants UNKNOWN | `Camera2D::Follow`, target player, smoothing 8.0, no limits | **NATIVE DESIGN** | `analysis/assets/locomotion.md` |
| PNG asset runtime | No reference PNG decoder requirement; native asset boundary | libpng → canonical RGBA8 `TextureData` → existing staging upload; deterministic exact-byte tests | **FUNCTIONAL (native), NOT parity** | `docs/ASSET_PIPELINE.md`, `tests/unit/png_test.cpp` |
| Unity Texture2D conversion | Reference bundle `level-objects`/`f903`, Texture2D path ID `-8989119030688471403`, RGBA32 16x24 | UnityPy 1.25.4 → deterministic PNG → native PNG round-trip/render | **FUNCTIONAL (one controlled asset), NOT parity** | `docs/ASSET_PIPELINE.md`, `analysis/assets/texture_conversion_manifest.json` |
| Math/vector conventions | Unity Vector2/3, quaternion, Y-up, 2D physics | Vec2/Vec3/Mat4 helpers exist | **CONFIRMED API**, no parity test | `analysis/re/timing.md`, `analysis/re/input_movement.md` |
| Scene/object lifecycle | MonoBehaviour messages, coroutines, pooling | Planned | **PARTIAL** — destruction/pool details open | `analysis/re/rooms_scene.md` |
| Room `r1_1` | Prefab hierarchy, `room_width=15`, `CreateWall` gaps, 44 wall cells, four serialized doors/triggers, aisle corridors | Native reconstruction test scene consumes recovered wall/door/aisle geometry; all-open entrance is a labeled native test configuration | **CONFIRMED geometry**, runtime entrance/listener/timing PARTIAL | `analysis/re/room_topology.md`, `analysis/re/r1_1_wall_map.json`, `analysis/assets/locomotion.md` |
| Procedural room generation | `MapManagerLevel.CreateMap`, `SerializeLinkerRoom` | Not started | **UNKNOWN** | `docs/PROCEDURAL_GENERATION.md` |
| Collision settings | Physics2D gravity (0,-9.81), Body/Wall/Door/Obstacle/Edge layers; generated walls on Edge(17); c00 child CircleCollider2D r=0.4 offset (0,0.5); Rigidbody2D.velocity | Native swept AABB with recovered static wall/corridor/door AABBs; circle approximated by enclosing 0.8×0.8 box | **CONFIRMED reference path; native APPROXIMATION** | `analysis/re/room_topology.md`, `analysis/assets/locomotion.md`, `tests/unit/scene_test.cpp` |
| Input mapping | Legacy axes, WASD, joystick deadzone 0.19 | Keyboard WASD/arrows wired into `MovementInput`; gamepad axes not consumed | **CONFIRMED reference + native subset** | `analysis/re/input_movement.md`, `tests/unit/locomotion_test.cpp` |
| Player movement | `C01Controller.FixedUpdate` → `SetVelocity`; speed 6.5/speed_rate 0.5 | Fixed 50 Hz `PlayerLocomotion`; effective 9.75 units/s | **HIGH CONFIDENCE reference formula; NATIVE implementation** | `analysis/re/input_movement.md`, `analysis/assets/locomotion.md` |
| Player identity | `c00` prefab uses `C01Controller` + `RoleAttributePlayer` | Native `c00` body/shadow loaded and animated | **CONFIRMED technical**; display name CANDIDATE | `analysis/re/rooms_scene.md` |
| Enemy identity | `e_slime01` uses `EnemyAI04` + `RoleAttribute`; contact `HitUnit(3,1.5)` | Not loaded natively | **CONFIRMED technical**; display name/full AI PARTIAL | `analysis/re/rooms_scene.md`, `analysis/re/enemy_ai04.md` |
| Enemy AI | `EnemyAI01..20`, coroutines; `EnemyAI04` minimal path | Not started | **PARTIAL (AI04)** / UNKNOWN (others) | `analysis/re/enemy_ai04.md` |
| Weapon identity | `weapon_000` = `Gun001`, bullet `bullet_0` | Not loaded natively | **CONFIRMED technical**; display name CANDIDATE | `analysis/re/weapons_projectiles.md` |
| Weapon fire | `Gun001.Attack` → `CreateBullet` → `BulletFactory.TakeBullet`; speed 36, deviation 5, consume 0 | Not implemented | **HIGH CONFIDENCE path**, cadence PARTIAL | `analysis/re/weapons_projectiles.md` |
| Projectile | `Bullet01`/`RGBullet` fields, `destroy_time=5`; BulletInfo speed 36 overrides prefab 10 | Not implemented | **HIGH CONFIDENCE speed**, rebound/through PARTIAL | `analysis/re/weapons_projectiles.md` |
| Damage bridge | Player `+0x228`, enemy `+0x2C8`, box `+0x168` | Not implemented | **CONFIRMED** | `analysis/re/damage.md` |
| Damage formula | `GetHurt`, armor overflow, `CanDie`, enemy HP | Not implemented | **HIGH CONFIDENCE** — crit/element open | `analysis/re/damage.md` |
| Critical hits | `Random.Range(0,100)` vs `DamageTrigger.critical`; observed HP damage not multiplied | Not implemented | **HIGH CONFIDENCE roll**, effect consumers PARTIAL | `analysis/re/damage.md` |
| Elements/status | `emDamageType`, `ShieldIce` factor 0.5 | Not implemented | **PARTIAL** — full status rules unknown | `analysis/re/damage.md` |
| Enemy death | HP <= 0 → `Dead`, collider/delegates | Not implemented | **PARTIAL** — post-death slots open | `analysis/re/damage.md` |
| Player death | flags, collider/weapon disable, Animator; `Reborn`→`DoReborn`; `GameFail`/`TryAgain` | Not implemented | **HIGH CONFIDENCE entry flow**, slots/UI PARTIAL | `analysis/re/damage.md` |
| Save/runtime boundary | `RGSaveManager`, `PlayerSaveData` on Unity `PlayerPrefs` string/int keys; payload unknown | Not implemented | **CONFIRMED PlayerPrefs**, payload UNKNOWN; not required for slice | `analysis/re/save.md` |
| Renderer | No reference renderer reconstruction; built-in RP indicated | Vulkan 1.3 instance/device/swapchain + multi-texture batched sprite renderer, real room/player composition, validation-clean | **FUNCTIONAL (native), NOT parity-verified** | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`, `docs/RENDERING.md` |
| Audio | miniaudio wrapper only; reference triggers unknown | Not implemented | **PLANNED** | `docs/AUDIO.md` |
| UI/HUD | No reconstruction; UI assets extracted | Not implemented | **PLANNED** | `docs/PHASE_4_NATIVE_RUNTIME.md` |

---

## Parity gates

| Gate | Requirement | Status |
|---|---|---|
| Gate B (Understanding) | Reusable engine/runtime primitives documented with evidence | ⚠️ **PARTIAL** — four blockers in `docs/GATE_B.md` |
| Gate C (Runtime) | Native app launches, renders, inputs | ⚠️ **PARTIAL** — Wayland launch, Vulkan present, native `r1_1` reconstruction test scene with real idle/run animation, WASD movement/collision/camera verified; combat/gameplay not implemented |
| Gate D (First Playable) | Core loop playable and FUNCTIONAL | ⛔ **BLOCKED** by Gate B/C and `docs/VERTICAL_SLICE.md` |
| Gate E (Content) | All discovered content PARTIAL+ | ⛔ **BLOCKED** |
| Gate F (Release) | All VERIFIED, tests pass | ⛔ **BLOCKED** |

---

## Tracking rules

- A row may move to `FUNCTIONAL` only when native code exists and tests execute.
- A row may move to `HIGH-FIDELITY`/`VERIFIED` only with reference comparison.
- A class/field/asset name is structural evidence, never behavioural proof.
- Candidate display names stay `CANDIDATE` until a serialized mapping proves them.
- No gate is marked complete to make the roadmap green.

**Last Updated:** 2026-10-31 (native room topology and door traversal slice)
