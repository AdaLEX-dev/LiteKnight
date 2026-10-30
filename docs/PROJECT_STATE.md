# Soul Knight 2.6.8 — Project State

**Last Updated:** 2026-10-30
**Commit:** (initial — pending)
**Phase:** 2 Complete → Entering Phase 3
**Current Status:** Engine reconstruction complete. Ready for gameplay reconstruction and native runtime.

---

## Completed

### Phase 0 — Environment Verification ✅
- CachyOS (Arch-based) confirmed
- Wayland session active (wayland-1)
- GCC 16.2.1, CMake 4.4.5, Ninja 1.13.2, Git 2.56.0
- Java OpenJDK 21, ADB 37.0.0
- apktool 2.11.0, jadx 1.5.6, Ghidra 12.2, Il2CppDumper 6.7.46, dotnet 6.0.428
- All tooling operational

### Phase 1 — APK Forensics ✅
**Deliverables:**
- `analysis/original-apk.sha256` — SHA-256 recorded
- `analysis/ARTIFACT.md` — Reference artifact metadata
- `analysis/APK_INVENTORY.md` — Complete APK inventory
- `analysis/class-map.md` — DEX class map (Java layer only)
- `analysis/native-libraries.md` — Native library analysis (6 libs)
- `analysis/resources.md` — AssetBundle catalog (30+ bundles)

**Key Findings:**
- Unity 2021.x IL2CPP (metadata v24.1)
- 100% gameplay logic in `libil2cpp.so` (52 MB arm64)
- Java layer: Unity bootstrap + 10+ SDKs only
- AssetBundles: 40+ MB content (textures, audio, prefabs, rooms)

### Phase 2 — Engine Reconstruction ✅
**IL2CPP Metadata Extraction:**
- Il2CppDumper on `libil2cpp.so` + `global-metadata.dat`
- Generated: `DummyDll/Assembly-CSharp.dll` (4.5 MB), `il2cpp.h` (35 MB)
- Discovered 200+ `RG*` game classes

**Architecture Documented:**
- `docs/ARCHITECTURE.md` — Complete system mapping
- `docs/REVERSE_ENGINEERING.md` — RE log
- `docs/TECHNOLOGY_DECISIONS.md` — 15 ADRs accepted

**Subsystems Mapped:**
| Subsystem | Classes | Confidence |
|-----------|---------|------------|
| Game Flow | 8 | HIGH |
| Player/Character | 8 | HIGH |
| Enemies/AI | 30+ | HIGH |
| Weapons | 25+ | HIGH |
| Projectiles | 35+ | HIGH |
| Rooms/Generation | 20+ | HIGH |
| Items/Progression | 15+ | HIGH |
| UI/Input | 8 | MEDIUM |
| Audio | 2 | MEDIUM |
| Save System | 2 | MEDIUM |
| Multiplayer | 8 | STRIPPED |

---

## In Progress

### Phase 3 — Gameplay Reconstruction (Next)
- [ ] Behavioural specifications for each subsystem
- [ ] Damage/combat formulas
- [ ] Enemy AI state machines
- [ ] Room generation algorithm
- [ ] RNG stream isolation design
- [ ] Player movement/dodge/attack mechanics
- [ ] Weapon/projectile physics

### Documentation
- [ ] `docs/PLATFORM.md` — Wayland/Vulkan platform layer design
- [ ] `docs/RENDERING.md` — Sprite batcher, atlas, shader design
- [ ] `docs/INPUT.md` — Input action mapping
- [ ] `docs/AUDIO.md` — Bus system, miniaudio integration
- [ ] `docs/SAVE_SYSTEM.md` — JSON schema, migration
- [ ] `docs/PROCEDURAL_GENERATION.md` — Dungeon gen algorithm
- [ ] `docs/COMBAT.md` — Damage, crit, armor, status effects
- [ ] `docs/AI.md` — Enemy AI behaviour trees
- [ ] `docs/UI.md` — HUD, menus, ImGui integration
- [ ] `docs/ASSET_PIPELINE.md` — Extraction → conversion → packaging
- [ ] `analysis/UNKNOWN_SYSTEMS.md` — Track unresolved mysteries
- [ ] `analysis/SOURCE_MAP.md` — Reference → native file mapping

---

## Blocked

- None — all tooling ready, architecture understood

---

## New Discoveries (This Session)

1. **Unity IL2CPP v24.1** — Confirmed via metadata version
2. **RG* namespace** — All game classes use `RG` prefix (Rogue/Random Generation)
3. **20 EnemyAI variants** — Numbered 01-20 + special (Alien, Witch, Ghost, etc.)
4. **8 MapManager modes** — Level, Endless, Defence, BR, Race, Sandbox, Soccer, Survive
5. **SerializeLinker** — Room serialization format (SBRoomInfo, SBGravityInfo, SBColor)
5. **RGRandom/RGRandomInterface** — Deterministic RNG abstraction
6. **Weapon hierarchy** — 25+ derivatives from RGWeapon
7. **Projectile hierarchy** — 35+ derivatives from RGBullet/DamageCarrier
8. **AssetBundle structure** — 30+ bundles, KTX2/ASTC ready

---

## Changed Assumptions

| Previous | Updated | Evidence |
|----------|---------|----------|
| "Maybe Mono" | **Definitely IL2CPP** | libil2cpp.so 52MB, metadata v24.1 |
| "Java has game logic" | **Zero game logic in Java** | Only UnityPlayerActivity + SDKs |
| "Single MapManager" | **8 specialized MapManagers** | MapManagerLevel, Endless, Defence, BR, Race, Sandbox, Soccer, Survive |
| "Simple weapons" | **Complex weapon hierarchy** | 25+ RGWeapon derivatives with unique behaviors |
| "MonoBehaviour coroutines" | **Heavy coroutine usage** | 100+ `IEnumerator` state machines in il2cpp.h |

---

## Tests

- **Unit:** 0 (framework ready: doctest)
- **Integration:** 0
- **Regression:** 0
- **Performance:** 0

---

## Build

- **Status:** Not yet configured
- **CMakeLists.txt:** Pending
- **Dependencies:** All decided (SDL3, Vulkan, miniaudio, VMA, toml++, nlohmann/json, doctest)

---

## Performance

- **Target:** 16.67 ms/frame (60 FPS @ 1080p)
- **Current:** N/A (no native code yet)
- **Budget Allocation (Planned):**
  - Simulation: ~4 ms
  - Render: ~6 ms
  - Audio: ~1 ms
  - Input/OS: ~1 ms
  - Margin: ~4 ms

---

## Next Actions (Priority Order)

1. **Create `docs/PROJECT_STATE.md`** (this file) ✅
2. **Create `docs/ROADMAP.md`** with detailed milestones
3. **Initialize Git repository** with `.gitignore`, baseline commit
4. **Create `tools/configure.sh`, `tools/build.sh`, `tools/run.sh`**
5. **Write `CMakeLists.txt`** with FetchContent for all deps
6. **Implement Platform Layer** (Phase 4):
   - Wayland window (SDL3)
   - Vulkan context + swapchain
   - Frame timing (fixed timestep)
   - Input abstraction
   - miniaudio initialization
7. **Implement Renderer** (Phase 4):
   - Sprite batcher (instanced draws)
   - Texture atlas (KTX2 array)
   - Shader pipeline (vertex + fragment)
   - Camera (orthographic, zoom, shake)
8. **Begin Gameplay Systems** (Phase 5):
   - Entity/Component system
   - Player movement + collision
   - One enemy + AI
   - One weapon + projectile
   - Room loading + transition

---

## Evidence References

| Document | Location |
|----------|----------|
| APK Hash | `analysis/original-apk.sha256` |
| Artifact Record | `analysis/ARTIFACT.md` |
| APK Inventory | `analysis/APK_INVENTORY.md` |
| Class Map (DEX) | `analysis/class-map.md` |
| Native Libraries | `analysis/native-libraries.md` |
| Resources/Assets | `analysis/resources.md` |
| Architecture | `docs/ARCHITECTURE.md` |
| RE Log | `docs/REVERSE_ENGINEERING.md` |
| Technology Decisions | `docs/TECHNOLOGY_DECISIONS.md` |
| IL2CPP Dump | `analysis/il2cpp_dump/` (DummyDll, il2cpp.h) |
| APKTool Output | `analysis/apktool_out/` |
| JADX Output | `analysis/jadx_out/` |

---

**Next Session Goal:** Technology decisions documented → Git initialized → CMake + Platform layer bootstrap.