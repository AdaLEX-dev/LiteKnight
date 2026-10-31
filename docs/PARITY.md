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
| Build/test baseline | N/A (tooling) | Debug + RelWithDebInfo, 31/1335 | **VERIFIED (tooling)** | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` |
| RNG core | Unity 2018.4 Xorshift128, LCG seed, 23-bit floats | `UnityRandom`/`RGRandomStream` implemented | **CONFIRMED (static)** | `analysis/re/rgrandom.md`, `tests/unit/unity_random_test.cpp` |
| RNG wrapper / streams | `seed_state`, map seed, `RandomBehaviour`, `RGController` seed | Native wrapper implemented | **PARTIAL** — seed_state 1/2 and DateTime seed exactness open | `analysis/re/rgrandom.md` |
| Fixed timestep | `TimeManager.Fixed_Timestep = 0.02` (50 Hz) | `FrameTimer` + application loop consume 50 Hz fixed steps | **CONFIRMED reference** | `analysis/re/timing.md`, `tests/unit/timing_test.cpp`, smoke log |
| Frame-loop model | Update vs FixedUpdate; render target 60 FPS | Application event → fixed tick → render/present loop; measured 0.834 sim steps/frame | **FUNCTIONAL (native)** | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` |
| Application lifecycle | N/A (native mechanism) | Bootstrap, Wayland-only enforcement, focused event loop, idempotent shutdown | **FUNCTIONAL** | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` |
| Wayland window | Native target: Wayland only | SDL3 window; 1920x1080 requested; compositor-constrained extent handled | **FUNCTIONAL / manual** | smoke log |
| Vulkan device/swapchain | Native target: Vulkan 1.3 | Intel UHD 620 fallback path, 3-image FIFO swapchain, validation-clean | **FUNCTIONAL** | smoke log |
| Sprite rendering | Reference sprites use Unity layer sorting; numeric mapping not recovered | GLSL sprite pipeline, RGBA8 texture, alpha, stable batch sort | **PARTIAL / bring-up** | `docs/RENDERING.md` |
| Math/vector conventions | Unity Vector2/3, quaternion, Y-up, 2D physics | Vec2/Vec3/Mat4 helpers exist | **CONFIRMED API**, no parity test | `analysis/re/timing.md`, `analysis/re/input_movement.md` |
| Scene/object lifecycle | MonoBehaviour messages, coroutines, pooling | Planned | **PARTIAL** — destruction/pool details open | `analysis/re/rooms_scene.md` |
| Room `r1_1` | Prefab hierarchy, `room_width=15`, runtime wall prefabs on layer Edge, doors/aisles/triggers | Not loaded natively | **CONFIRMED asset/source**, exact gaps PARTIAL | `analysis/re/rooms_scene.md` |
| Procedural room generation | `MapManagerLevel.CreateMap`, `SerializeLinkerRoom` | Not started | **UNKNOWN** | `docs/PROCEDURAL_GENERATION.md` |
| Collision settings | Physics2D gravity (0,-9.81), layers/tags; `RGRoomX.CreateWall` instantiates 1×1 wall colliders | Not implemented | **HIGH CONFIDENCE** — exact gaps open | `analysis/re/rooms_scene.md` |
| Input mapping | Legacy axes, WASD, joystick deadzone 0.19 | Scaffold only | **CONFIRMED reference** | `analysis/re/input_movement.md` |
| Player movement | `C01Controller.FixedUpdate` → `SetVelocity`; speed 6.5/speed_rate 0.5 | Not implemented | **HIGH CONFIDENCE** | `analysis/re/input_movement.md` |
| Player identity | `c00` prefab uses `C01Controller` + `RoleAttributePlayer` | Not loaded natively | **CONFIRMED technical**; display name CANDIDATE | `analysis/re/rooms_scene.md` |
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
| Renderer | No reference renderer reconstruction; built-in RP indicated | Vulkan 1.3 instance/device/swapchain + batched textured quad, validation-clean | **FUNCTIONAL (bring-up), NOT parity-verified** | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`, `docs/RENDERING.md` |
| Audio | miniaudio wrapper only; reference triggers unknown | Not implemented | **PLANNED** | `docs/AUDIO.md` |
| UI/HUD | No reconstruction; UI assets extracted | Not implemented | **PLANNED** | `docs/PHASE_4_NATIVE_RUNTIME.md` |

---

## Parity gates

| Gate | Requirement | Status |
|---|---|---|
| Gate B (Understanding) | Reusable engine/runtime primitives documented with evidence | ⚠️ **PARTIAL** — four blockers in `docs/GATE_B.md` |
| Gate C (Runtime) | Native app launches, renders, inputs | ⚠️ **PARTIAL** — Wayland launch, Vulkan present, resize/focus/minimize verified; input/gameplay not implemented |
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

**Last Updated:** 2026-10-31 (Phase 4 runtime foundation)
