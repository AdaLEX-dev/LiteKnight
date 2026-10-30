# Soul Knight 2.6.8 — Reverse Engineering Log

Generated: 2026-10-30
Session: Initial reconnaissance

---

## Session Summary

**Date:** 2026-10-30
**Phase:** 0 → 1 → 2 (Environment → APK Forensics → Engine Reconstruction)
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

## Engine Reconstruction (Phase 2) — Complete

### IL2CPP Metadata Extraction
- **Tool:** Il2CppDumper 6.7.46
- **Metadata:** `assets/bin/Data/Managed/Metadata/global-metadata.dat` (10 MB)
- **Output:** `DummyDll/Assembly-CSharp.dll` (4.5 MB), `il2cpp.h` (35 MB struct definitions)

### Class Hierarchy Discovered
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
- ✅ `docs/ARCHITECTURE.md` — Complete system mapping

---

## Evidence Log

| ID | Type | Description | Confidence |
|----|------|-------------|------------|
| RE-001 | CONFIRMED | Unity IL2CPP (libil2cpp.so 52MB, metadata v24.1) | CONFIRMED |
| RE-002 | CONFIRMED | Zero gameplay logic in DEX | CONFIRMED |
| RE-003 | CONFIRMED | RG* namespace for game classes | CONFIRMED |
| RE-004 | HIGH | 20 distinct EnemyAI classes | HIGH CONFIDENCE |
| RE-005 | HIGH | 8 MapManager variants for game modes | HIGH CONFIDENCE |
| RE-005 | HIGH | Deterministic RNG via RGRandom | HIGH CONFIDENCE |
| RE-006 | HIGH | Room serialization via SerializeLinker | HIGH CONFIDENCE |
| RE-007 | HYPOTHESIS | Damage formula: (atk - def) * crit * element | HYPOTHESIS |
| RE-008 | HYPOTHESIS | Room gen: BSP + graph + template placement | HYPOTHESIS |
| RE-009 | UNKNOWN | Exact projectile physics (gravity, bounce) | UNKNOWN |
| RE-010 | UNKNOWN | Save file format (BinaryFormatter?) | UNKNOWN |

---

## Next Actions

1. **Create Technology Decisions** — `docs/TECHNOLOGY_DECISIONS.md`
2. **Create Project State** — `docs/PROJECT_STATE.md`
3. **Create Roadmap** — `docs/ROADMAP.md`
4. **Begin Phase 3** — Gameplay Reconstruction (behavioural specs)
5. **Begin Phase 4** — Native Runtime (Wayland window, game loop)

---

## Blockers

- None — all tooling operational
- Need to decide on C++ vs Rust, Vulkan vs OpenGL, SDL3 vs raw Wayland

---

## Git Status

```
Initial commit pending
Files to commit:
- analysis/ (all deliverables)
- docs/ARCHITECTURE.md
- NEMOTRON_MASTER_PROJECT.md
```

---

**Session End:** Ready for technology selection and native runtime implementation.