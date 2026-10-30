# Soul Knight 2.6.8 — Roadmap

**Version:** 1.0
**Target:** CachyOS, x86-64, Wayland, 1920×1080, 60 FPS
**Baseline:** Soul Knight 2.6.8 (APK SHA-256: 8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea)

---

## Phase Overview

| Phase | Name | Target | Gate |
|-------|------|--------|------|
| 0 | Environment | Tooling verified | A |
| 1 | APK Forensics | Inventory complete | B |
| 2 | Engine Reconstruction | Architecture mapped | B |
| 3 | Gameplay Reconstruction | Behavioural specs | B |
| 4 | Native Runtime | Wayland app launches | C |
| 5 | First Playable | Core loop playable | D |
| 6 | Core Systems | All major systems | E |
| 7 | Content Parity | Full catalog implemented | E |
| 8 | Fidelity | Pixel-perfect behaviour | F |
| 9 | Performance | 60 FPS stable | F |
| 10 | Packaging | CachyOS package | F |
| 11 | Release | v1.0.0 tagged | F |

---

## Detailed Milestones

### Phase 3 — Gameplay Reconstruction (Weeks 1-3)
**Goal:** Behavioural specification for every subsystem

| Task | Deliverable | Status |
|------|-------------|--------|
| Player movement & physics | `docs/PLAYER.md` | ⬜ |
| Player combat (attack, dodge, skill) | `docs/COMBAT.md` (player section) | ⬜ |
| Enemy AI (20+ types) | `docs/AI.md` | ⬜ |
| Weapon mechanics (all 25+ types) | `docs/WEAPONS.md` | ⬜ |
| Projectile physics & triggers | `docs/PROJECTILES.md` | ⬜ |
| Damage system (armor, crit, elements, status) | `docs/COMBAT.md` (damage section) | ⬜ |
| Room generation algorithm | `docs/PROCEDURAL_GENERATION.md` | ⬜ |
| Item/buff/chest systems | `docs/ITEMS.md` | ⬜ |
| Progression & unlocks | `docs/PROGRESSION.md` | ⬜ |
| Save/load system | `docs/SAVE_SYSTEM.md` | ⬜ |
| Audio triggers & music | `docs/AUDIO.md` | ⬜ |
| UI/HUD/Menu flow | `docs/UI.md` | ⬜ |
| RNG stream isolation design | `docs/DETERMINISM.md` | ⬜ |
| Unknown systems tracking | `analysis/UNKNOWN_SYSTEMS.md` | ⬜ |
| Source map (ref → native) | `analysis/SOURCE_MAP.md` | ⬜ |

**Gate B:** All major behavioural specs documented with evidence references.

---

### Phase 4 — Native Runtime (Weeks 3-6)
**Goal:** Native Wayland application launches reliably

| Task | Deliverable | Status |
|------|-------------|--------|
| Git repo init + CMake + FetchContent | `CMakeLists.txt`, `tools/*.sh` | ⬜ |
| SDL3 Wayland window (1920×1080, vsync) | `src/platform/wayland/Window.cpp` | ⬜ |
| Vulkan instance + device + swapchain | `src/engine/rendering/VulkanContext.cpp` | ⬜ |
| Frame timing (fixed 60 Hz sim, variable render) | `src/platform/timing/FrameTimer.cpp` | ⬜ |
| Input abstraction (keyboard, mouse, gamepad) | `src/platform/input/InputManager.cpp` | ⬜ |
| miniaudio engine + bus system | `src/platform/audio/AudioEngine.cpp` | ⬜ |
| FileSystem (XDG paths, asset loading) | `src/platform/filesystem/FileSystem.cpp` | ⬜ |
| Logger + profiler integration | `src/engine/core/Log.cpp`, `Perf.cpp` | ⬜ |
| Basic render loop (clear + present) | `src/app/main.cpp` | ⬜ |
| Debug overlay (FPS, frame graph, entity count) | `src/engine/core/DebugOverlay.cpp` | ⬜ |

**Gate C:** Native app launches, shows window, maintains 60 FPS clear screen, handles input, clean exit.

---

### Phase 5 — First Playable (Weeks 6-9)
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
| One weapon (pistol/sword) + projectile | `src/game/weapons/`, `src/game/projectiles/` | ⬜ |
| One enemy (EnemyAI01) + AI | `src/game/enemies/EnemySystem.cpp` | ⬜ |
| Damage application + death | `src/game/combat/CombatSystem.cpp` | ⬜ |
| Player death + restart | `src/game/player/PlayerDeath.cpp` | ⬜ |
| One room load + transition | `src/game/rooms/RoomSystem.cpp` | ⬜ |
| Basic HUD (health, weapon) | `src/game/ui/HUD.cpp` | ⬜ |
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
| **Procedural Gen** | BSP + graph + template placement, deterministic seeds | ⬜ |
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
| 0-2 | 1 week | ✅ DONE |
| 3 | 3 weeks | RE documentation |
| 4 | 3 weeks | Platform + Renderer |
| 5 | 3 weeks | First playable |
| 6 | 9 weeks | Core systems (parallelizable) |
| 7 | 6 weeks | Content implementation |
| 8 | 6 weeks | Fidelity polishing |
| 9 | 4 weeks | Profiling + optimization |
| 10 | 2 weeks | Packaging |
| 11 | 1 week | Release |
| **Total** | **~41 weeks** | **~10 months** |

---

## Risk Register

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| IL2CPP RE incomplete | Missed mechanics | Medium | Phase 3 thoroughness, runtime experiments |
| Vulkan complexity | Delay Phase 4 | Medium | Minimal renderer, copy from working examples |
| Determinism bugs | Desync in gen/combat | High | PCG32 streams, regression tests per system |
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

**Next Review:** After Phase 3 complete (Gate B).
**Roadmap Version:** 1.0 — Update at each gate.