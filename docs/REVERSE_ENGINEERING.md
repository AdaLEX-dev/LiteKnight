# Soul Knight 2.6.8 — Reverse Engineering Log

Generated: 2026-10-30
Session: Initial reconnaissance

> **Correction — 2026-10-31.** The original log called Phase 2 "complete" and recorded Unity 2021.x / metadata v24.1. Both are wrong. The reference is **Unity 2018.4.12f1**, metadata **version 24**. The metadata dump is structural evidence. Gate B is now **PARTIAL** (`docs/GATE_B.md`); core engine primitives and the vertical-slice damage/input/room contracts are reconstructed, but full gameplay/content remains partial. See `analysis/PROJECT_DIAGNOSIS.md`, `analysis/EVIDENCE_AUDIT.md`, and `analysis/re/`.

---

## Session Summary

**Date:** 2026-10-30
**Original phase claim:** 0 → 1 → 2 (Environment → APK Forensics → Engine Reconstruction)
**Corrected status:** Phase 1 artifact inventory is complete. Phase 2 is **PARTIAL** (Gate B PARTIAL). Phase 3 is **PARTIAL/CURRENT**: RNG, timing, input, room prefab, weapon/projectile assets, and damage bridges reconstructed; AI/save/procedural generation/full content remain unresolved.
**APK:** `original/SoulKnight-2.6.8.apk` (SHA-256: 8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea)

---

## Tools Verified

| Tool | Version | Status |
|------|---------|--------|
| CachyOS | Rolling (Arch-based) | ✅ Native |
| Wayland | wayland-1 | ✅ Active |
| GCC | 16.2.1 | ✅ |
| CMake | 4.4.5 | ✅ |
| Ninja | 1.13.2 | ✅ |
| Git | 2.56.0 | ✅ |
| Java | OpenJDK 27.0.1 (JDK) | ✅ |
| apktool | 2.11.0 | ✅ Working |
| jadx | 1.5.6 | ✅ Working (30 errors — obfuscated SDK code) |
| Ghidra | 12.2 PUBLIC | ✅ Installed |
| Il2CppDumper | 6.7.46 (.NET 6) | ✅ Working |
| ADB | 37.0.0 | ✅ Available |
| dotnet SDK | 6.0.428 | ✅ Installed |

---

## APK Forensics (Phase 1) — Complete

### Deliverables Created
- ✅ `analysis/original-apk.sha256` — Immutable artifact hash
- ✅ `analysis/ARTIFACT.md` — Reference artifact record
- ✅ `analysis/APK_INVENTORY.md` — Full APK inventory
- ✅ `analysis/class-map.md` — DEX class map (Java layer)
- ✅ `analysis/native-libraries.md` — Native library analysis
- ✅ `analysis/resources.md` — AssetBundle & resource catalog

### Key Findings
1. **Unity IL2CPP confirmed** — All gameplay in `libil2cpp.so` (52 MB arm64)
2. **Java layer minimal** — Only Unity bootstrap + 10+ ad/analytics SDKs
3. **AssetBundles** — 30+ bundles, 40+ MB content (textures, audio, prefabs, rooms)
4. **Native libs** — libil2cpp.so, libunity.so, libmain.so, + Firebase/GPG SDKs

---

## Engine Reconstruction (Phase 2) — **NOT COMPLETE**

### IL2CPP Metadata Extraction (structural only)
- **Tool:** Il2CppDumper 6.7.46
- **Metadata:** `assets/bin/Data/Managed/Metadata/global-metadata.dat` (10 MB, metadata version 24)
- **Full local output (outside repo):**
  - `/home/jules/bin/dump.cs` (458,499 lines)
  - `/home/jules/bin/script.json` (103,252 method entries)
  - `/home/jules/bin/il2cpp.h` (34 MB)
  - `/home/jules/bin/DummyDll/` (75 assemblies)
  - `/home/jules/bin/stringliteral.json`
- **In-repo output:** `analysis/il2cpp_dump/` is a partial/truncated copy, not the full dump.

### Class Hierarchy Discovered (structural evidence)
**Core Prefix: `RG*` (Rogue/Random Generation)**

| Category | Classes | Key Types |
|----------|---------|-----------|
| Game Flow | 8 | RGGameProcess, RGGameSceneManager, RGGameInfo, RGGameConst, RGRandom, RGSaveManager, RGNetWorkManager |
| Player | 8 | RGBaseController, RGController, RGPetController, RGMountController, RGCharacterInfo, RGCharGray, RGJoyControl, MovementInput |
| Enemies | 30+ | RGEController, EnemyAI01-20, EnemyAIAlien, EnemyAIChestBattery, EnemyAIFrog, EnemyAIGhost, EnemyAIStake, EnemyAITombStone01/02, EnemyAIWitch, RGEBossController, RGEControllerRace, RGEWeapon, RGEHand |
| Weapons | 25+ | RGWeapon, RGSword, RGLaser, RGShortLaser, RGDrill, RGChain, RGRuinSword, RGBox, RGTBox, RGBatteryController, RGBothPot, RGAutoDestory, RGSBullet01/02, RGSBulletFollow, GunBox*, GunJetPack, GunMagicStaff, GunTelescope |
| Projectiles | 35+ | RGBullet, RGBulletTrigger*, RGArrow*, RGSwordTrigger*, RGDrillTrigger, RGShortLaserTrigger, RGChain, RGShield, RGParticleTrigger, RGTaskBullet, RGBDelay*, RGBT* |
| Rooms/Gen | 20+ | MapManager (8 variants), RGRoomX*, RGDoor*, RGTransferGate*, RGAisle, RGBigRoom, RGMaze, RGStartRoomEvent, SerializeLinker* |
| Items | 15+ | RGItem, RGStateItem, RGBuff, RGChest*, RGCoin, RGHealthPot*, RGArmorPot, RGEnergyPot, RGSecretKey*, ItemObjectProvider |
| UI/Input | 8 | RGJoyControl, RGJoyStick, RGJoyBtn*, RGUIWindow |
| Audio | 2 | RGMusicManager |
| Sandbox | 12 | SandboxConfigData, SandboxMapData, SandboxObject*, MapManagerSandbox |
| Multiplayer | 8 | (Stripped for v1) |

### Architecture Document
- ⚠️ `docs/ARCHITECTURE.md` — structural class inventory plus unverified hypotheses; **not** a complete behavioural mapping

---

## Evidence Log (corrected)

| ID | Type | Description | Confidence |
|----|------|-------------|------------|
| RE-001 | CONFIRMED | Unity 2018.4.12f1 IL2CPP (`libil2cpp.so` 52MB, metadata version 24) | CONFIRMED |
| RE-002 | CONFIRMED | Zero gameplay logic in DEX | CONFIRMED |
| RE-003 | CONFIRMED | RG* namespace for game classes | CONFIRMED |
| RE-004 | CONFIRMED (structural) | 20 distinct EnemyAI class names | HIGH STRUCTURAL |
| RE-005 | CONFIRMED (structural) | Multiple MapManager classes for game modes | HIGH STRUCTURAL |
| RE-006 | CONFIRMED (structural + disassembly) | `RGRandom` wrapper and Unity Xorshift128 engine reconstructed; map-seed path resolved | HIGH |
| RE-007 | CONFIRMED (structural) | Room serialization classes `SerializeLinker*` exist | HIGH STRUCTURAL |
| RE-008 | PARTIAL | Damage bridge confirmed (`RGController +0x228`, `RGEController +0x2C8`, `RGBox +0x168`); HP/armor/CanDie reconstructed; crit/element details open | HIGH (bridge), PARTIAL (formula) |
| RE-009 | UNKNOWN | Room gen algorithm; `r1_1` prefab/seed path confirmed, generation not reconstructed | UNKNOWN |
| RE-010 | PARTIAL | `bullet_0`/`Bullet01` fields, trigger chain, destroy_time; exact velocity/rebound open | PARTIAL |
| RE-011 | UNKNOWN | Save file format (`RGSaveManager`/`AboGameData`); not decoded | UNKNOWN |

---

## Next Actions (historical 2026-10-30 list — superseded)

1. **Create Technology Decisions** — `docs/TECHNOLOGY_DECISIONS.md`
2. **Create Project State** — `docs/PROJECT_STATE.md`
3. **Create Roadmap** — `docs/ROADMAP.md`
4. **Begin Phase 3** — Gameplay Reconstruction (behavioural specs)
5. **Begin Phase 4** — Native Runtime (Wayland window, game loop)

---

## Blockers (current)

- **No runtime oracle**: no Android device/emulator/Waydroid/QEMU/Frida.
- **Build/test baseline is fixed**; static-analysis and asset pipelines exist.
- **Gate B blockers**: wall-collision placement, spawn/door state, weapon fire/projectile velocity, full death/reborn.
- **RGRandom residuals**: `seed_state` 1/2, DateTime seed exactness, runtime reference vectors.
- **Renderer/Vulkan foundation is implemented** (Wayland window, Vulkan device/swapchain, textured-quad present); asset/input/gameplay layers remain incomplete. Gate C is **PARTIAL**.
- **Asset extraction is reproducible**, but native conversion and display-name identity are incomplete.
- Full AI/procedural generation/save/content remain Phase 3 work; see `docs/PHASE_3_BEHAVIOURAL_RE.md` and `docs/PARITY.md`.

---

## Git Status (historical)

The original session left `src/`, `tests/`, and `external/` untracked. See the current repository state and the build/test migration commits.

---

**Session End (historical):** technology selection and native runtime bootstrap were attempted before behaviour reconstruction. The corrected workflow is evidence-first; see `analysis/PROJECT_DIAGNOSIS.md` and `tools/re/`.

---

## 2026-10-31 — Evidence-First Session

- Corrected Unity version/metadata and Gate B/C status.
- Restored build/test feedback loop; 22 tests executed at that stage and an intentional failure turns CTest red (now 31).
- Added `tools/re/il2cpp_re.py` for metadata → RVA → disassembly → xrefs.
- Reconstructed `RGRandom` in `analysis/re/rgrandom.md`; native `UnityRandom`/`RGRandomStream` implemented and tested.
- Added UnityPy extraction under `tools/asset/`; first-pass manifest has 284 entries and no proprietary bytes committed.

## 2026-10-31 — Gate B / Phase 3 Evidence Pass

- Confirmed fixed timestep **0.02 s (50 Hz)** from serialized `TimeManager`
  (`analysis/re/timing.md`).
- Resolved damage dispatch: player `+0x228` (`RGController.GetHurt`), enemy
  `+0x2C8` (`RGEController.GetHurt`), box `+0x168` (`RGBox.Hit`);
  `CanDie` checks `RGGameProcess.this_index` (`analysis/re/damage.md`).
- Resolved RGRandom map seed and `MapManagerLevel` LongMap branch; `seed_state`
  1/2 remains unused (`analysis/re/rgrandom.md`).
- Reconstructed input/movement contract (`analysis/re/input_movement.md`).
- Reconstructed `r1_1` room hierarchy/collider representation and asset identities
  (`analysis/re/rooms_scene.md`).
- Reconstructed `weapon_000`/`Gun001` and `bullet_0`/`Bullet01` evidence
  (`analysis/re/weapons_projectiles.md`).
- Wrote Gate B, Phase 3, and Phase 4 planning reports.
- Gate B remains **PARTIAL** due to wall-collision placement, spawn/door state,
  weapon fire/projectile velocity, and full death/reborn. Phase 4 planning is
  complete but no runtime code was started.

## 2026-10-31 — Final Gate B / Phase 3 Evidence Sprint

- Resolved the runtime wall source (`RGRoomX.CreateWall` → `MapManagerLevel.wall_list`,
  layer `Edge`), player spawn (`MapManager.centerPoint`), and door/clear entry points.
- Resolved `Gun001.Attack → CreateBullet → BulletFactory.TakeBullet`, effective
  speed 36, deviation 5, and `consume=0`; cooldown remains PARTIAL.
- Resolved `EnemyAI04` contact `HitUnit(3,1.5)` and the minimal `e_slime01` contract.
- Resolved death/reborn/game-over/restart entry flow; exact virtual slots/UI remain PARTIAL.
- Resolved the save boundary as Unity `PlayerPrefs`; payload remains UNKNOWN.
- Corrected native `FrameTimer` to the confirmed 50 Hz step and added a regression
  test: **23 cases / 1276 assertions at that time** (now 35/1367 after the PNG asset stage).
- Gate B and Phase 3 remain **PARTIAL**; first-slice approximations are explicitly
  scoped in `docs/GATE_B.md` and `analysis/GATE_B_PHASE_3_FINAL_AUDIT.md`.

## 2026-10-31 — Phase 4 Native Runtime Foundation

- Added `Application` lifecycle, Wayland-only SDL3 window, resumable window-state
  model, measured 50 Hz/60 FPS loop, and idempotent shutdown.
- Added Vulkan 1.3 instance/device/queues/swapchain/render pass/frame resources,
  Debug validation, and compositor-aware resize/recreation.
- Added GLSL sprite shaders, dynamic batched quads, RGBA8 staging texture upload,
  alpha blending, stable layer sorting, and aspect-fit resize.
- Manual 10 s smoke: exit 0, 0 validation messages, 602 presented frames,
  506 fixed ticks, 16.665 ms average `FrameTimer` frame.
- Tests at that stage: **31 cases / 1335 assertions** (now 35/1367 after PNG).
- Gate C is **PARTIAL**; asset decoding, input-to-simulation, audio, UI, and
  gameplay remain unimplemented. See `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`.