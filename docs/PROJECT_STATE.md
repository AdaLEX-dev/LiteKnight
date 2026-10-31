# Soul Knight 2.6.8 — Project State

**Last Updated:** 2026-10-31 (diagnostic review)
**Commit:** session head; run `git log -1 --format='%h %s'` (session commits `4a2af27`..head: build baseline, analysis pipelines, evidence docs/ADRs)
**Phase:** Phase 2 is **not** complete; Phase 3 (behaviour reconstruction) has not started
**Current Status:** Evidence and build pipeline need repair before further porting.

> **2026-10-31 DIAGNOSTIC OVERRIDE**
>
> The 2026-10-30 status below is retained as a historical record, but it is contradicted by the artifact and the build. Do not use it as current truth.
>
> Full reports:
> - `analysis/PROJECT_DIAGNOSIS.md`
> - `analysis/EVIDENCE_AUDIT.md`
>
> Confirmed current state:
> - The APK/game is Unity **2018.4.12f1**, metadata **v24** — not Unity 2021.x / v24.1.
> - `cmake --build build` fails (incomplete vendored Vulkan headers, missing `fmt`, missing `SOUL_KNIGHT_COMMIT_HASH`, SDL3 Vulkan API mismatch).
> - Unit tests are disabled by `DOCTEST_CONFIG_DISABLE=0`; enabling them exposes compile errors. CTest's green result is false.
> - The native runtime is a skeleton: no Vulkan context, swapchain, sprite batcher, simulation, or gameplay.
> - Phase 3 behaviour reconstruction has not started: there is no runtime oracle and no Ghidra disassembly workflow. Docs infer behaviour from class names.
> - `RGRandom` delegates to `UnityEngine.Random`; the accepted PCG32 design cannot preserve parity.
> - AssetBundles have not been extracted (`assets/converted`, `assets/generated`, and `data/` are empty), although UnityPy can read them.
> - `src/`, `tests/`, and `external/` are untracked in Git.
>
> **Actual blockers:** build/test feedback loop; reference observation or disassembly pipeline; asset/data extraction; evidence-labelled behavioural specs.
>
> **Next action:** execute the P0 recovery plan in `analysis/PROJECT_DIAGNOSIS.md` — fix build/tests first, then choose the observation pipeline.

---

## 2026-10-31 — Evidence-First Restoration Results

**Phase:** Phase 3 (behaviour reconstruction) is now in progress; Phase 2 is not "complete."

**Completed this session:**
- Corrected the Unity baseline to **2018.4.12f1 / metadata v24** across the key docs.
- Marked Gate B and Gate C **NOT MET**; superseded the PCG32 parity ADR with ADR-0016.
- Added `docs/EVIDENCE_STANDARD.md`.
- Restored a real build/test loop:
  - system Vulkan headers (removed fabricated `external/vulkan` copy),
  - `fmt` discovery/linking, Vulkan loader linking, commit-hash definition,
  - SDL3 Vulkan API fix, filesystem formatting fix,
  - doctest enabled, ECS fixes, CTest filter fixed.
- Verified:
  - clean Debug configure/build,
  - **22 test cases execute**,
  - intentional failure turns CTest red,
  - RelWithDebInfo configure/build/test.
- Added reusable static-analysis tooling:
  - `tools/re/il2cpp_re.py` (metadata → RVA → disassembly → direct xrefs),
  - `tools/re/ghidra/ApplyIl2CppSymbols.java` and `tools/re/ghidra_import.sh`,
  - `tools/re/README.md`.
- Reconstructed `RGRandom` from `libil2cpp.so` and `libunity.so`:
  - `analysis/re/rgrandom.md`,
  - native `UnityRandom` / `RGRandomStream` in `src/engine/math/UnityRandom.*`,
  - 10 new tests; all 22 total pass.
- Started the damage reconstruction:
  - `analysis/re/damage.md` (player armor/HP/can-die path, enemy HP path, projectile trigger pipeline; collision→target bridge unresolved).
- Added reproducible UnityPy extraction:
  - `tools/asset/extract_unity.py`, `tools/asset/extract.sh`, `tools/asset/requirements.txt`,
  - `analysis/assets/extraction_manifest.json` (284 entries),
  - first-pass room/character/enemy/weapon/UI/audio extraction; proprietary bytes remain out of Git.

**Current blockers / unknown areas:**
- No runtime observation oracle (no device/emulator/Frida) — static evidence only.
- Complete projectile-effect → target `GetHurt` bridge not identified.
- Critical-hit and element/status formulas not reconstructed.
- `CanDie` field `+0x1C` semantic conflict needs resolution.
- Map seed virtual call path not fully traced.
- Renderer/Vulkan context still not implemented; vertical slice remains evidence-gated.

**Next actions:**
1. Resolve the projectile→target damage bridge and critical/element rules.
2. Finish RGRandom residual questions (`RGController.rg_random` initialization, `seed_state` 1/2, map-length branch).
3. Start the vertical slice only after the damage/RNG evidence gates in `docs/VERTICAL_SLICE.md` are met.

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

**Key Findings (corrected 2026-10-31):**
- **Unity 2018.4.12f1 IL2CPP (metadata version 24)** — the original 2021.x / v24.1 claim was wrong.
- 100% of discovered gameplay logic is in `libil2cpp.so` (52 MB arm64); this is structural, not behavioural, evidence.
- Java layer: Unity bootstrap + 10+ SDKs only.
- AssetBundles: ~39 MB content + ~70 MB `assets/bin` data (textures, audio, prefabs, rooms).

### Phase 2 — Engine Reconstruction (historical claim — **SUPERSEDED**)
**IL2CPP Metadata Extraction (structural only):**
- Il2CppDumper on `libil2cpp.so` + `global-metadata.dat`
- Full local output: `/home/jules/bin/dump.cs`, `/home/jules/bin/script.json`, `/home/jules/bin/il2cpp.h`, `/home/jules/bin/DummyDll/`
- In-repo `analysis/il2cpp_dump/` is a partial/truncated copy, not the full dump
- Metadata contains fields/signatures/RVAs; **it does not contain method bodies or behaviour**

**Architecture Documented (largely speculative):**
- `docs/ARCHITECTURE.md` — class inventory plus inferred mechanics; not a verified reconstruction
- `docs/REVERSE_ENGINEERING.md` — session log with unmarked inferences
- `docs/TECHNOLOGY_DECISIONS.md` — 15 ADRs; ADR-0014 (PCG32) is superseded

**Subsystem Inventory (class discovery, not behavioural confidence):**
| Subsystem | Classes | Discovery confidence | Behaviour verified? |
|-----------|---------|----------------------|---------------------|
| Game Flow | 8 | HIGH | NO |
| Player/Character | 8 | HIGH | NO |
| Enemies/AI | 30+ | HIGH | NO |
| Weapons | 25+ | HIGH | NO |
| Projectiles | 35+ | HIGH | NO |
| Rooms/Generation | 20+ | HIGH | NO |
| Items/Progression | 15+ | HIGH | NO |
| UI/Input | 8 | MEDIUM | NO |
| Audio | 2 | MEDIUM | NO |
| Save System | 2 | MEDIUM | NO |
| Multiplayer | 8 | STRIPPED | N/A |

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

## Blocked (historical claim — **SUPERSEDED**)

- The original "None" claim was false. Actual blockers:
  - build and test feedback loop,
  - reference observation or disassembly pipeline,
  - asset/data extraction,
  - evidence-labelled behavioural specifications.

---

## New Discoveries (2026-10-30 session — corrected)

1. **Unity 2018.4.12f1 IL2CPP, metadata version 24** — confirmed from `libunity.so`, `libil2cpp.so`, bundle headers, and `global-metadata.dat`.
2. **RG* namespace** — game classes use the `RG` prefix; names are structural evidence only.
3. **20 EnemyAI variants** — numbered 01–20 plus specials.
4. **8 MapManager modes** — class inventory only; behaviour not reconstructed.
5. **SerializeLinker** — structural room serialization classes; format not reconstructed.
6. **RGRandom/RGRandomInterface** — `RGRandom` wraps `UnityEngine.Random.State`; it is not a custom PCG32 stream. See `analysis/re/rgrandom.md`.
7. **Weapon hierarchy** — 25+ class derivatives; mechanics not verified.
8. **Projectile hierarchy** — 35+ class derivatives; mechanics not verified.
9. **AssetBundle structure** — ~39 MB bundles plus ~70 MB `assets/bin`; readable with UnityPy.

---

## Changed Assumptions

| Previous | Updated | Evidence |
|----------|---------|----------|
| "Maybe Mono" | **Definitely IL2CPP** | libil2cpp.so 52MB, metadata version 24 |
| "Java has game logic" | **Zero game logic in Java** | Only UnityPlayerActivity + SDKs |
| "Single MapManager" | **Multiple MapManager classes** | Class inventory; exact mode count not verified |
| "Simple weapons" | **Large class hierarchy** | 25+ RGWeapon derivatives; behaviour unknown |
| "MonoBehaviour coroutines" | **Heavy coroutine usage** | `IEnumerator` classes in metadata; behaviour not reconstructed |

---

## Tests (historical claim — **SUPERSEDED**)

The original "Unit: 0 (framework ready)" entry was misleading.

- The suite was wired with `DOCTEST_CONFIG_DISABLE=0`, which defines `DOCTEST_CONFIG_DISABLE` and compiles every test out.
- CTest reported green because zero tests executed.
- Enabling the tests exposed compile errors in `src/engine/ecs/World.hpp` and `tests/unit/ecs_test.cpp`.
- Current rule: do not claim a test pass unless actual test cases execute. See `analysis/PROJECT_DIAGNOSIS.md` and the build/test log in `docs/PROJECT_STATE.md` after the fix.

---

## Build (historical claim — **SUPERSEDED**)

- **Original status:** "Not yet configured / CMakeLists pending"
- **Corrected status:** configured but failing; see `analysis/PROJECT_DIAGNOSIS.md`.
- **Dependencies:** SDL3, Vulkan, miniaudio, toml++, nlohmann/json, doctest; `fmt` was used but not declared, and VMA was referenced but not linked.

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

## Next Actions (2026-10-30 list — **SUPERSEDED**)

> Current priority order is in `analysis/PROJECT_DIAGNOSIS.md` and the 2026-10-31 override above. The list below is retained as history.

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
| IL2CPP Dump (full, outside repo) | `/home/jules/bin/dump.cs`, `/home/jules/bin/script.json`, `/home/jules/bin/il2cpp.h`, `/home/jules/bin/DummyDll/` |
| IL2CPP Dump (partial, in repo) | `analysis/il2cpp_dump/` (not the full dump) |
| APKTool Output | `analysis/apktool_out/` (gitignored) |
| JADX Output | `analysis/jadx_out/` (gitignored) |
| Diagnosis | `analysis/PROJECT_DIAGNOSIS.md` |
| Evidence audit | `analysis/EVIDENCE_AUDIT.md` |

---

**Next Session Goal:** Technology decisions documented → Git initialized → CMake + Platform layer bootstrap.