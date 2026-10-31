# Soul Knight 2.6.8 — Reverse Engineering Log

Generated: 2026-10-30
Session: Initial reconnaissance

> **Correction — 2026-10-31.** The original log called Phase 2 "complete" and recorded Unity 2021.x / metadata v24.1. Both are wrong. The reference is **Unity 2018.4.12f1**, metadata **version 24**. The metadata dump is structural evidence; behaviour has not been reconstructed. Gate B is **NOT MET**. See `analysis/PROJECT_DIAGNOSIS.md`, `analysis/EVIDENCE_AUDIT.md`, and `analysis/re/`.

---

## Session Summary

**Date:** 2026-10-30
**Original phase claim:** 0 → 1 → 2 (Environment → APK Forensics → Engine Reconstruction)
**Corrected status:** Phase 1 artifact inventory is useful; Phase 2 behavioural reconstruction is **not complete**. Phase 3 has not started.
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
| Java | OpenJDK 21.0.12 | ✅ |
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
| RE-006 | PARTIAL | `RGRandom` wraps `UnityEngine.Random.State`; wrapper semantics under reconstruction | PARTIAL |
| RE-007 | CONFIRMED (structural) | Room serialization classes `SerializeLinker*` exist | HIGH STRUCTURAL |
| RE-008 | HYPOTHESIS | Damage formula: `(atk - def) * crit * element` | UNVERIFIED |
| RE-009 | HYPOTHESIS | Room gen: BSP + graph + template placement | UNVERIFIED |
| RE-010 | UNKNOWN | Exact projectile physics (gravity, bounce) | UNKNOWN |
| RE-011 | UNKNOWN | Save file format (Sirenix/Odin-backed `AboGameData` suspected; not disassembled) | UNKNOWN |

---

## Next Actions

1. **Create Technology Decisions** — `docs/TECHNOLOGY_DECISIONS.md`
2. **Create Project State** — `docs/PROJECT_STATE.md`
3. **Create Roadmap** — `docs/ROADMAP.md`
4. **Begin Phase 3** — Gameplay Reconstruction (behavioural specs)
5. **Begin Phase 4** — Native Runtime (Wayland window, game loop)

---

## Blockers (corrected)

- **No runtime oracle**: no Android device/emulator/Waydroid/QEMU/Frida.
- **No disassembly workflow** at the time of the original session (Ghidra installed but no project).
- **Build and tests were not working**; test results were false-green.
- **Asset/data extraction not started**.
- Behavioural specs are hypotheses, not evidence.

---

## Git Status (historical)

The original session left `src/`, `tests/`, and `external/` untracked. See the current repository state and the build/test migration commits.

---

**Session End (historical):** technology selection and native runtime bootstrap were attempted before behaviour reconstruction. The corrected workflow is evidence-first; see `analysis/PROJECT_DIAGNOSIS.md` and `tools/re/`.

---

## 2026-10-31 — Evidence-First Session

- Corrected Unity version/metadata and Gate B/C status.
- Restored build/test feedback loop; 22 tests execute and an intentional failure turns CTest red.
- Added `tools/re/il2cpp_re.py` for metadata → RVA → disassembly → xrefs.
- Reconstructed `RGRandom` in `analysis/re/rgrandom.md`; native `UnityRandom`/`RGRandomStream` implemented and tested.
- Started `analysis/re/damage.md`; player/enemy entry points and state mutation reconstructed, projectile bridge unresolved.
- Added UnityPy extraction under `tools/asset/`; first-pass manifest has 284 entries and no proprietary bytes committed.
- Next: resolve the projectile→target damage bridge, critical/element rules, and the map-seed virtual path before a vertical slice.