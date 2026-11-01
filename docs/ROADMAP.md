# Soul Knight 2.6.8 — Roadmap

**Version:** 1.7 (native room topology and door traversal slice, 2026-10-31)
**Target:** CachyOS, x86-64, Wayland, 1920×1080, 60 FPS
**Baseline:** Soul Knight 2.6.8 (APK SHA-256: 8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea)
**Reference engine:** Unity **2018.4.12f1**, IL2CPP metadata **v24**

> **Current status (2026-10-31 Gate B / Phase 3 evidence pass):**
>
> - Phase 0 — COMPLETE
> - Phase 1 — COMPLETE (artifact inventory)
> - Phase 2 — PARTIAL; Gate B **PARTIAL** (see `docs/GATE_B.md`)
> - Phase 3 — PARTIAL/CURRENT (vertical-slice contract largely reconstructed; full content/AI/save unknown; see `docs/PHASE_3_BEHAVIOURAL_RE.md`)
> - Phase 4 — **PARTIAL**; foundation + PNG/Unity Texture2D + Sprite/camera/room composition + idle/run animation + keyboard locomotion + recovered wall/door/aisle collision/traversal FUNCTIONAL, Gate C **PARTIAL** (see `docs/PHASE_4_NATIVE_RUNTIME.md`)
> - Phase 5+ — BLOCKED by Gate B/C
>
> The build/test/evidence pipeline is clean and reproducible. This is not a Phase 4
> completion. The schedule below is a historical estimate, not a commitment.
> Behavioural work follows `analysis/re/`, `docs/EVIDENCE_STANDARD.md`, and
> `tools/re/`.

---

## Phase Overview

| Phase | Name | Target | Gate | Current status |
|-------|------|--------|------|----------------|
| 0 | Environment | Tooling verified | A | ✅ COMPLETE |
| 1 | APK Forensics | Inventory complete | B | ✅ COMPLETE |
| 2 | Engine Reconstruction | Architecture mapped | B | ⚠️ PARTIAL / Gate B PARTIAL |
| 3 | Gameplay Reconstruction | Behavioural specs | B | ⚠️ PARTIAL / CURRENT |
| 4 | Native Runtime | Wayland app launches | C | ⚠️ PARTIAL / Gate C PARTIAL |
| 5 | First Playable | Core loop playable | D | ⛔ BLOCKED |
| 6 | Core Systems | All major systems | E | ⛔ BLOCKED |
| 7 | Content Parity | Full catalog implemented | E | ⛔ BLOCKED |
| 8 | Fidelity | Pixel-perfect behaviour | F | ⛔ BLOCKED |
| 9 | Performance | 60 FPS stable | F | ⛔ BLOCKED |
| 10 | Packaging | CachyOS package | F | ⛔ BLOCKED |
| 11 | Release | v1.0.0 tagged | F | ⛔ BLOCKED |

---

## Detailed Milestones

### Phase 3 — Gameplay Reconstruction (CURRENT)
**Goal:** Behavioural specification for every subsystem

> Deliverable filenames below are planned labels, not files that currently
> exist. Actual evidence documents live in `analysis/re/`. A task is not complete
> because a document exists; it is complete only when its evidence gate is met.

| Task | Evidence document / planned deliverable | Status |
|------|------------------------------------------|--------|
| RNG stream isolation design | `analysis/re/rgrandom.md` | ⚠️ PARTIAL — core/map-seed/LongMap reconstructed; `seed_state` 1/2 and DateTime seed open |
| Damage system (armor, crit, elements, status) | `analysis/re/damage.md` | ⚠️ PARTIAL — player/enemy/box bridges confirmed; crit/element/effect-trigger details open |
| Unknown systems tracking | `analysis/UNKNOWN_SYSTEMS.md` | 🔄 ACTIVE |
| Source map (reference → native) | `analysis/SOURCE_MAP.md` | 🔄 ACTIVE (planning only; no implementation) |
| Player movement & physics | `analysis/re/input_movement.md` | ⚠️ PARTIAL — movement formula/stats reconstructed; no native implementation |
| Player combat (attack, dodge, skill) | `analysis/re/combat_prototype.md` | ⚠️ PARTIAL — first-slice attack/projectile/damage/death implemented; dodge/skill/UI not started |
| Enemy AI (20+ types) | `analysis/re/enemy_ai04.md` (first slice) | ⚠️ PARTIAL — AI04 contact/base path; full AI not started |
| Weapon mechanics (25+ types) | `analysis/re/weapons_projectiles.md` | ⚠️ PARTIAL — weapon_000/Gun001 cadence/spread/speed verified; 24+ weapon types not reconstructed |
| Projectile physics & triggers | `analysis/re/weapons_projectiles.md` | ⚠️ PARTIAL — Bullet01/trigger chain; native swept linear simulation; Rigidbody/rebound open |
| Room generation algorithm | `analysis/re/room_topology.md`, `docs/PROCEDURAL_GENERATION.md` | ⚠️ PARTIAL — `CreateWall`/`CreateAisle`/door geometry recovered; map-level generation and runtime entrance assignment for `r1_1` UNKNOWN |
| Item/buff/chest systems | `docs/ITEMS.md` (planned) | ⬜ NOT STARTED |
| Progression & unlocks | `docs/PROGRESSION.md` (planned) | ⬜ NOT STARTED |
| Save/load system | `analysis/re/save.md` | ⚠️ PARTIAL — PlayerPrefs boundary; payload UNKNOWN |
| Audio triggers & music | `docs/AUDIO.md` (design only) | ⬜ NOT STARTED |
| UI/HUD/Menu flow | `docs/UI.md` (planned) | ⬜ NOT STARTED |

**Gate B:** Reusable engine/runtime primitives documented with evidence. **Current status: PARTIAL** — see `docs/GATE_B.md`; wall/door gap placement and closed-door collision are now recovered, while runtime entrance assignment for a generated `r1_1`, the event-to-map-manager traversal listener, door animation timing, exact projectile Rigidbody/rebound behavior, and full death/reborn remain open.

---

### Phase 4 — Native Runtime (PARTIAL; Gate C PARTIAL)
**Goal:** Native Wayland application launches reliably

> The full implementation plan is `docs/PHASE_4_NATIVE_RUNTIME.md`; the evidence
> record is `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`. The bootstrap, window,
> Vulkan context/swapchain, sprite renderer, and 50 Hz/60 FPS loop are functional.
> Asset decoding, input-to-simulation, sprite animation, doors, and a first-slice
> combat prototype are implemented; audio, UI, and the full game remain
> unimplemented.

| Task | Deliverable | Status |
|------|-------------|--------|
| Git repo init + CMake + FetchContent | `CMakeLists.txt`, `tools/*.sh` | ✅ |
| SDL3 Wayland window (1920×1080 requested) | `src/platform/wayland/Window.cpp` | ✅ functional/manual |
| Wayland-only enforcement + event lifecycle | `Application`, `WindowState` | ✅ functional |
| Vulkan instance + device + swapchain | `src/engine/rendering/VulkanContext.cpp` | ✅ functional |
| Frame timing (50 Hz sim, 60 FPS render target) | `FrameTimer`, `Application` | ✅ functional |
| Sprite renderer (textured batched quad) | `SpriteRenderer`, `SpriteBatch`, `shaders/` | ✅ bring-up |
| Asset runtime / PNG decode | `src/engine/assets/PngDecoder.*`, `TextureData.hpp`, `Texture2D.cpp` | ✅ PNG → RGBA8 → GPU path; KTX2/registry pending |
| Controlled Unity Texture2D | `tools/asset/extract_unity.py`, `analysis/assets/texture_conversion_manifest.json` | ✅ One real asset converted, deterministic, native round-trip/render verified |
| Unity Sprite metadata / PPU | `SpriteData`, `analysis/assets/sprites.md` | ✅ Rect/pivot/PPU/border/filter/wrap; rectangular tight-crop approximation |
| 2D camera / coordinate system | `Camera2D`, `analysis/assets/coordinate_system.md` | ✅ Y-up world, PPU 16, aspect-fit; exact reference camera size unresolved |
| r1_1 + c00 visual composition | `build_visual_scene.py`, `SceneLoader`, `analysis/assets/scenes/r1_1_visual.json` | ✅ 482 static instances + obstacle field (594 draw batch), 182 sprite assets, 14 textures, rendered Wayland/Vulkan |
| Player simulation/render sync | `PlayerLocomotion`, `PlayerRenderSync`, `Application` | ✅ body follows sim exactly; shadow offset preserved; animation/camera cannot move player |
| Eight-way movement / facing | `MovementInput`, `PlayerLocomotion`, `FacingDirection` | ✅ normalized 8-way, 9.75 speed, horizontal mirror facing, 50 Hz |
| Character test switching | `PlayerCharacterSet`, recovered c00..c14 | ✅ 15 recovered prefabs, 30 ide/run clips, keys 1..0 and [ ] |
| Obstacle variant switching | `ObstacleField`, `obstacle_list` | ✅ 8 solid recovered variants selectable (12 entries in metadata), 49 marker cells, per-variant collision |
| Combat prototype | `Weapon`, `ProjectileSystem`, `Damage`, `EnemySystem`, `Application` | ✅ 168 recovered projectile weapons -> e_slime01 damage/death; player contact/death/reset; native aim/attack bindings |
| Developer devroom / UI | `scripts/dev.sh`, `DevOverlay`, `DebugTextRenderer`, selectors | ✅ permanent launcher, overlay, character/weapon/enemy selectors, spawner |
| Asset inventory / audit | `tools/asset/pipeline.py`, `build_asset_inventory.py`, `audit_assets.py` | ✅ texture/category inventories, metadata validation, redistribution classes |
| Door animation / collision lifecycle | `Door`, `RoomDoorController`, recovered Animator clips | ✅ open/close clips, four states, animation-synchronized blocking, all four orientations |
| Obstacle replacement / wall visuals | `RGObjectSkin`, `obstacle_list`, `wall603` | ✅ 49 markers replaced; 140 wall + 49 obstacle collision cells match visible instances |
| Sprite animation / Animator frames | `AnimationClip`, `AnimationPlayer`, `LocomotionAnimator`, `analysis/assets/animation.md` | ✅ real `char_1_skin_0_ide` and `c00_run`; 4 frames each, 0.5 s loop; fixed-step native sampling |
| Input-driven locomotion / idle-run transitions | `MovementInput`, `PlayerLocomotion`, `AabbCollision`, `Camera2D` | ✅ WASD, fixed 50 Hz, swept AABB, idle/run switching, camera follow |
| Real room geometry / door traversal | `build_visual_scene.py`, `SceneLoader`, `AabbCollision`, `analysis/re/room_topology.md` | ✅ Recovered 44 wall cells + 96 corridor cells, four doors/triggers, one-way closed-door collision; open-door traversal verified |
| Higher-level room transition listener / adjacent room | planned | ⬜ Next bounded investigation; no procedural generation yet |
| Input abstraction (keyboard, mouse, gamepad) | `src/platform/input/InputManager.cpp` | ✅ keyboard/mouse movement, attack, mouse/right-stick aim; debug controls |
| miniaudio engine + bus system | `src/platform/audio/AudioEngine.cpp` | ⚠️ scaffold, not wired |
| FileSystem (XDG paths) | `src/platform/filesystem/FileSystem.cpp` | ⚠️ scaffold |
| Logger + startup diagnostics | `src/engine/core/Log.cpp` | ✅ |
| Basic render loop (clear + present) | `Application`, `VulkanRenderer` | ✅ functional |
| Debug overlay (FPS/frame graph) | planned | ⬜ |

**Gate C:** Native app launches, shows window, presents frames, handles resize/
minimize/focus, and exits cleanly. **Current status: PARTIAL** — launch/present,
input, locomotion, doors and the first-slice combat loop are verified; full gameplay
is not. The 60 FPS target is measured only on the test machine, not guaranteed.

---

### Phase 5 — First Playable (BLOCKED; Gate D NOT MET)
**Goal:** Genuinely playable native loop

| Task | Deliverable | Status |
|------|-------------|--------|
| Entity/Component system | `src/engine/ecs/` | ⬜ |
| Transform + Sprite components | `src/engine/ecs/Components.cpp` | ⬜ |
| Sprite batcher (instanced, atlas) | `src/engine/rendering/SpriteBatcher.cpp` | ⬜ |
| Camera (ortho, follow, shake) | `src/engine/rendering/Camera.cpp` | ⬜ |
| Player entity + movement + collision | `src/game/player/PlayerSystem.cpp` | ⬜ |
| Input → Player action mapping | `src/game/player/PlayerInput.cpp` | ⬜ |
| Basic collision (AABB / circle) | `src/engine/collision/Collision.cpp` | ⬜ |
| One confirmed weapon + projectile | `src/game/combat/Weapon.cpp`, `ProjectileSystem.cpp` | ✅ weapon_000/bullet_0 first slice |
| One enemy (EnemyAI01) + AI | `src/game/enemy/EnemySystem.cpp` | ⚠️ e_slime01 static fixture + contact/death; AI not started |
| Damage application + death | `src/game/combat/Damage.hpp`, `EnemySystem.cpp` | ✅ first-slice damage/death; crit/elements open |
| Player death + restart | `src/app/Application.cpp` (`R` reset) | ✅ minimum dev reset; production game-over/reborn not started |
| One room load + transition | `src/game/rooms/RoomSystem.cpp` | ⬜ |
| Basic HUD (health, weapon) | `src/game/ui/DevOverlay.cpp` | ⚠️ developer overlay only; production HUD not started |
| Audio: player hit, enemy hit, shoot | `src/game/audio/` | ⬜ |
| Save/load (slot 0) | `src/game/save/SaveSystem.cpp` | ⬜ |

**Gate D:** Native launch → move → attack → kill enemy → take damage → die → restart → exit. All working.

---

### Phase 6 — Core Systems (Weeks 9-18)
**Goal:** All major gameplay systems implemented

| System | Tasks | Status |
|--------|-------|--------|
| **Weapons** | All 25+ weapon types, fire modes, ammo, reload, upgrades | ⬜ |
| **Projectiles** | All 35+ bullet types, triggers, effects, pooling | ⬜ |
| **Combat** | Damage formula, armor, crit, elements, status effects, knockback | ⬜ |
| **Enemies** | All 20+ EnemyAI types, boss phases, RGEBossController | ⬜ |
| **Rooms** | All MapManager modes, room templates, doors, transitions | ⬜ |
| **Procedural Gen** | Generation algorithm unreconstructed (BSP/graph/template is a hypothesis) | ⬜ |
| **Items** | Pickups, chests (7 types), buffs, consumables, keys | ⬜ |
| **Characters** | All characters, stats, skills, passives, skins | ⬜ |
| **Progression** | Unlocks, currency, achievements, NG+ | ⬜ |
| **Audio** | Full SFX, music per biome, dynamic mixing | ⬜ |
| **UI** | All menus, HUD, inventory, shop, settings, pause | ⬜ |
| **Save** | Full save/load, version migration, multiple slots | ⬜ |

**Gate E:** All discovered content catalogued and implemented or explicitly marked.

---

### Phase 7 — Content Parity (Weeks 18-24)
**Goal:** Parity matrix complete

| Content Type | Count (Est.) | Status |
|--------------|--------------|--------|
| Characters | ~20 | ⬜ |
| Weapons | ~150 | ⬜ |
| Enemies | ~80 | ⬜ |
| Bosses | ~16 | ⬜ |
| Items | ~200 | ⬜ |
| Rooms | ~200 templates | ⬜ |
| Status Effects | ~30 | ⬜ |
| UI Screens | ~15 | ⬜ |

**Tracking:** `docs/PARITY.md` with columns: Subsystem | Ref Behaviour | Native Behaviour | Confidence | Test | Status | Notes

---

### Phase 8 — Fidelity (Weeks 24-30)
**Goal:** Pixel-perfect behaviour match

| Priority | Subsystem | Method |
|----------|-----------|--------|
| 1 | Timing | Frame-accurate sim, fixed dt |
| 2 | Movement | Velocity, acceleration, dodge frames |
| 3 | Collision | Hitbox sizes, pushout, trigger order |
| 4 | Weapon Mechanics | Fire rate, spread, projectile speed, lifetime |
| 5 | Enemy Mechanics | AI states, attack timing, pathfinding |
| 6 | Generation | Same seed = same dungeon (structure + spawns) |
| 7 | Characters | Stats, skills, passive triggers |
| 8 | Items | Pickup logic, chest loot tables, buff stacking |
| 9 | Progression | Unlock conditions, currency rates |
| 10 | Bosses | Phase transitions, patterns, HP thresholds |
| 11 | UI | Animations, transitions, input navigation |
| 12 | Audio | Trigger timing, volume curves, spatialization |
| 13 | Visual Polish | Particles, screen shake, hit pause, trails |

**Method:** Side-by-side video comparison, frame-stepping, automated regression tests.

---

### Phase 9 — Performance (Weeks 30-34)
**Goal:** Stable 60 FPS @ 1080p

| Metric | Target | Measurement |
|--------|--------|-------------|
| Avg frame time | ≤ 16.67 ms | `perf stat`, internal profiler |
| 1% low | ≤ 16.67 ms | 1000 frame sample |
| Max spike | ≤ 33 ms | No frame > 2x budget |
| CPU (sim) | ≤ 4 ms | Zone profiling |
| CPU (render) | ≤ 6 ms | Zone profiling |
| GPU | ≤ 8 ms | RenderDoc, Vulkan timestamps |
| VRAM | ≤ 512 MB | `vulkaninfo`, heap tracking |
| Heap | ≤ 256 MB | `heaptrack` |
| Load time | ≤ 3 s | Cold start to gameplay |

**Optimization Log:** Every change records Before/Change/After/Measurement.

---

### Phase 10 — Packaging (Weeks 34-36)
**Goal:** Reproducible CachyOS package

| Task | Status |
|------|--------|
| PKGBUILD for CachyOS AUR | ⬜ |
| Reproducible build (fixed deps, no timestamps) | ⬜ |
| Desktop entry + icon | ⬜ |
| Man page | ⬜ |
| Systemd user service (optional) | ⬜ |
| Install to `$XDG_DATA_HOME/soul-knight/` | ⬜ |
| Uninstall clean | ⬜ |

---

### Phase 11 — Release (Week 36+)
**Goal:** v1.0.0 tagged

| Gate | Requirement |
|------|-------------|
| Clean tree | `git status` clean |
| Build | `tools/build.sh Release` passes |
| Tests | `tools/test.sh` all pass |
| Wayland smoke | `tools/run.sh` launches, plays 5 min |
| 1080p test | Native 1920×1080, no scaling issues |
| Performance | Phase 9 metrics met |
| Docs | All `docs/*.md` current |
| Changelog | `CHANGELOG.md` updated |
| Version | `VERSION` = 1.0.0 |
| Commit | `release: v1.0.0` |
| Tag | `v1.0.0` |
| Verify remote | `git remote -v` correct |
| Push | Explicit `git push origin main --tags` |

---

## Dependency Graph

```
Phase 0,1,2 ──────────────────┐
                               ▼
Phase 3 (Gameplay Specs) ◄─────┤
                               │
Phase 4 (Native Runtime) ──────┤
                               │
Phase 5 (First Playable) ◄─────┤
                               │
Phase 6 (Core Systems) ────────┤
                               │
Phase 7 (Content Parity) ◄─────┤
                               │
Phase 8 (Fidelity) ────────────┤
                               │
Phase 9 (Performance) ─────────┤
                               │
Phase 10 (Packaging) ──────────┤
                               │
Phase 11 (Release) ────────────┘
```

---

## Resource Allocation (Solo Developer)

| Phase | Est. Time | Focus |
|-------|-----------|-------|
| 0-1 | 1 week | ✅ DONE (historical) |
| 2 | — | ⚠️ PARTIAL / Gate B PARTIAL |
| 3 | — | ⚠️ PARTIAL/CURRENT (estimate suspended) |
| 4 | — | ⚠️ PARTIAL / Gate C PARTIAL |
| 5 | — | ⛔ BLOCKED |
| 6 | 9 weeks | Core systems (parallelizable) |
| 7 | 6 weeks | Content implementation |
| 8 | 6 weeks | Fidelity polishing |
| 9 | 4 weeks | Profiling + optimization |
| 10 | 2 weeks | Packaging |
| 11 | 1 week | Release |
| **Total** | **—** | **estimate suspended: Phase 4/5 have not started and Phase 3 is evidence-gated** |

---

## Risk Register

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| IL2CPP RE incomplete | Missed mechanics | Medium | Phase 3 thoroughness, static analysis, approved native observation |
| Vulkan complexity | Delay Phase 4 | Medium | Minimal renderer, copy from working examples |
| Determinism bugs | Desync in gen/combat | High | Reference-compatible RNG streams, regression tests per system |
| Asset pipeline | Content missing | Medium | AssetRipper automation, validation step |
| Performance | < 60 FPS | Medium | Profile early, budget per system |
| Scope creep | Never release | High | Strict gate enforcement, no new features post-Gate E |
| Burnout | Project abandonment | Medium | Weekly milestones, visible progress, playable builds |

---

## Success Criteria for v1.0.0

- [ ] Native CachyOS executable (no Android runtime)
- [ ] Wayland-native (no X11/XWayland)
- [ ] Stable 60 FPS @ 1920×1080
- [ ] Keyboard + mouse + gamepad
- [ ] All characters, weapons, enemies, bosses from 2.6.8
- [ ] All rooms, generation modes, items, progression
- [ ] Full save/load with versioning
- [ ] Audio (music + SFX + UI)
- [ ] UI (menus, HUD, settings, pause)
- [ ] Regression test suite passing
- [ ] Performance metrics documented
- [ ] Reproducible CachyOS package
- [ ] Complete documentation
- [ ] CHANGELOG.md + v1.0.0 tag

---

**Next Review:** After Gate B blockers are closed or explicitly accepted.
**Roadmap Version:** 1.6 — Update at each gate.