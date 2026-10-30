# Soul Knight 2.6.8 — Source Map

**Purpose:** Map reference (IL2CPP/DEX) classes to native implementation files.

**Format:**
```
Reference:
    Namespace.ClassName

Native:
    src/<module>/<SystemName>.*

Status:
    PLANNED | IN_PROGRESS | IMPLEMENTED | TESTED | VERIFIED

Evidence:
    analysis/<path/to/evidence>
```

---

## Core Game Flow

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGGameProcess` | `src/game/core/GameProcess.cpp` | PLANNED | `analysis/class-map.md` |
| `RGGameSceneManager` | `src/game/core/SceneManager.cpp` | PLANNED | `analysis/class-map.md` |
| `RGGameInfo` | `src/game/core/GameInfo.cpp` | PLANNED | `analysis/class-map.md` |
| `RGGameConst` | `data/constants/game_constants.toml` | PLANNED | `analysis/class-map.md` |
| `RGRandom` / `RGRandomInterface` | `src/engine/math/Random.cpp` | PLANNED | `analysis/UNKNOWN_SYSTEMS.md#UNK-001` |
| `RGSaveManager` | `src/game/save/SaveManager.cpp` | PLANNED | `docs/SAVE_SYSTEM.md` |
| `RGNetWorkManager` | (stripped) | N/A | `analysis/class-map.md` |

---

## Player System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGBaseController` | `src/game/player/ControllerBase.cpp` | PLANNED | `analysis/class-map.md` |
| `RGController` | `src/game/player/PlayerController.cpp` | PLANNED | `analysis/class-map.md` |
| `RGCharacterInfo` | `data/characters/*.toml` | PLANNED | `analysis/class-map.md` |
| `RGCharGray` | `src/game/player/CharacterVisuals.cpp` | PLANNED | `analysis/class-map.md` |
| `RGPetController` | `src/game/player/PetController.cpp` | PLANNED | `analysis/class-map.md` |
| `RGMountController` | `src/game/player/MountController.cpp` | PLANNED | `analysis/class-map.md` |
| `RGJoyControl` / `RGJoyStick` / `RGJoyBtn*` | `src/platform/input/InputManager.cpp` | PLANNED | `docs/INPUT.md` |
| `MovementInput` | `src/game/player/MovementInput.cpp` | PLANNED | `analysis/class-map.md` |

---

## Enemy System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGEController` | `src/game/enemies/EnemyController.cpp` | PLANNED | `analysis/class-map.md` |
| `EnemyAI01` – `EnemyAI20` | `src/game/enemies/ai/EnemyAI##.cpp` | PLANNED | `analysis/class-map.md` |
| `EnemyAIAlien` | `src/game/enemies/ai/EnemyAI_Alien.cpp` | PLANNED | `analysis/class-map.md` |
| `EnemyAIChestBattery` | `src/game/enemies/ai/EnemyAI_ChestBattery.cpp` | PLANNED | `analysis/class-map.md` |
| `EnemyAIFrog` | `src/game/enemies/ai/EnemyAI_Frog.cpp` | PLANNED | `analysis/class-map.md` |
| `EnemyAIGhost` | `src/game/enemies/ai/EnemyAI_Ghost.cpp` | PLANNED | `analysis/class-map.md` |
| `EnemyAIStake` | `src/game/enemies/ai/EnemyAI_Stake.cpp` | PLANNED | `analysis/class-map.md` |
| `EnemyAITombStone01/02` | `src/game/enemies/ai/EnemyAI_TombStone.cpp` | PLANNED | `analysis/class-map.md` |
| `EnemyAIWitch` | `src/game/enemies/ai/EnemyAI_Witch.cpp` | PLANNED | `analysis/class-map.md` |
| `RGEBossController` | `src/game/bosses/BossController.cpp` | PLANNED | `analysis/class-map.md` |
| `RGEWeapon` / `RGEHand` / `RGELaser*` | `src/game/enemies/EnemyWeapons.cpp` | PLANNED | `analysis/class-map.md` |
| `EnemyDummy` | `src/game/enemies/EnemyDummy.cpp` | PLANNED | `analysis/class-map.md` |
| `EnemySoccer` | (stripped) | N/A | `analysis/class-map.md` |

---

## Weapon System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGWeapon` | `src/game/weapons/WeaponBase.cpp` | PLANNED | `analysis/class-map.md` |
| `RGSword` / `RGSwordBuffTrigger` | `src/game/weapons/melee/Sword.cpp` | PLANNED | `analysis/class-map.md` |
| `RGLaser` / `RGLaserBuff` | `src/game/weapons/beam/Laser.cpp` | PLANNED | `analysis/class-map.md` |
| `RGShortLaser` | `src/game/weapons/beam/ShortLaser.cpp` | PLANNED | `analysis/class-map.md` |
| `RGDrill` / `RGDrillTrigger` | `src/game/weapons/melee/Drill.cpp` | PLANNED | `analysis/class-map.md` |
| `RGChain` | `src/game/weapons/special/Chain.cpp` | PLANNED | `analysis/class-map.md` |
| `RGRuinSword` | `src/game/weapons/melee/RuinSword.cpp` | PLANNED | `analysis/class-map.md` |
| `RGBox` / `RGTBox` / `RGBatteryController` | `src/game/weapons/deployable/Box.cpp` | PLANNED | `analysis/class-map.md` |
| `RGBothPot` / `RGAutoDestory` | `src/game/weapons/thrown/Pot.cpp` | PLANNED | `analysis/class-map.md` |
| `RGSBullet01` / `RGSBullet02` / `RGSBulletFollow` | `src/game/weapons/special/SpecialBullets.cpp` | PLANNED | `analysis/class-map.md` |
| `RGTaskBullet` | `src/game/weapons/special/TaskBullet.cpp` | PLANNED | `analysis/class-map.md` |
| `GunBox*` / `GunJetPack` / `GunMagicStaff` / `GunTelescope` | `src/game/weapons/gunbox/*.cpp` | PLANNED | `analysis/class-map.md` |
| `RGAim` | `src/game/weapons/AimComponent.cpp` | PLANNED | `analysis/class-map.md` |
| `RGDash` | `src/game/player/DashComponent.cpp` | PLANNED | `analysis/class-map.md` |

---

## Projectile System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGBullet` / `DamageCarrier` | `src/game/projectiles/ProjectileBase.cpp` | PLANNED | `analysis/class-map.md` |
| `RGBulletTrigger*` | `src/game/projectiles/triggers/*.cpp` | PLANNED | `analysis/class-map.md` |
| `RGBDelayCreate` / `RGBDelayDivision` | `src/game/projectiles/DelayedProjectile.cpp` | PLANNED | `analysis/class-map.md` |
| `RGBMoveToSpawn` | `src/game/projectiles/HomingProjectile.cpp` | PLANNED | `analysis/class-map.md` |
| `RGBTCreate` / `RGBTDelayCreate` / `RGBTDivision` | `src/game/projectiles/TriggerProjectile.cpp` | PLANNED | `analysis/class-map.md` |
| `RGBTEnergy` / `RGBTExplode` / `RGBTExplodeThrow` | `src/game/projectiles/ExplosiveProjectile.cpp` | PLANNED | `analysis/class-map.md` |
| `RGBTHeart` / `RGBTMultiExplode` / `RGBTRebound` / `RGBTSpawn` | `src/game/projectiles/SpecialProjectile.cpp` | PLANNED | `analysis/class-map.md` |
| `RGArrowTrigger*` / `RGArrowBuffTrigger*` | `src/game/projectiles/ArrowProjectile.cpp` | PLANNED | `analysis/class-map.md` |
| `RGSwordTrigger*` / `RGSwordSlash` | `src/game/projectiles/MeleeArc.cpp` | PLANNED | `analysis/class-map.md` |
| `RGDrillTrigger` / `RGShortLaserTrigger` | `src/game/projectiles/BeamProjectile.cpp` | PLANNED | `analysis/class-map.md` |
| `RGChain` / `RGShield` / `RGParticleTrigger` | `src/game/projectiles/EffectProjectile.cpp` | PLANNED | `analysis/class-map.md` |

---

## Room / Generation System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `MapManager` (base) | `src/game/generation/MapManager.cpp` | PLANNED | `analysis/class-map.md` |
| `MapManagerLevel` | `src/game/generation/MapManagerLevel.cpp` | PLANNED | `analysis/class-map.md` |
| `MapManagerEndless` | `src/game/generation/MapManagerEndless.cpp` | PLANNED | `analysis/class-map.md` |
| `MapManagerDefence` | `src/game/generation/MapManagerDefence.cpp` | PLANNED | `analysis/class-map.md` |
| `MapManagerBR` | (stripped) | N/A | `analysis/class-map.md` |
| `MapManagerRace` | (stripped) | N/A | `analysis/class-map.md` |
| `MapManagerSandbox` | `src/game/generation/MapManagerSandbox.cpp` | PLANNED | `analysis/class-map.md` |
| `MapManagerSoccer` | (stripped) | N/A | `analysis/class-map.md` |
| `MapManagerSurvive` | `src/game/generation/MapManagerSurvive.cpp` | PLANNED | `analysis/class-map.md` |
| `MapManagerPattern` | `src/game/generation/MapManagerPattern.cpp` | PLANNED | `analysis/class-map.md` |
| `RGRoomX` / `RGRoomXEndless` / `RGRoomXZombie` | `src/game/rooms/RoomTemplate.cpp` | PLANNED | `analysis/class-map.md` |
| `RGAisle` / `RGBigRoom` / `RGMaze` | `src/game/rooms/RoomTypes.cpp` | PLANNED | `analysis/class-map.md` |
| `RGDoor` / `RGDoorTrigger` | `src/game/rooms/Door.cpp` | PLANNED | `analysis/class-map.md` |
| `RGTransferGate` / `RGTransferGateEndless` | `src/game/rooms/TransferGate.cpp` | PLANNED | `analysis/class-map.md` |
| `RGStartRoomEvent` | `src/game/rooms/StartRoom.cpp` | PLANNED | `analysis/class-map.md` |
| `SerializeLinker*` | `src/game/generation/SerializeLinker.cpp` | PLANNED | `analysis/class-map.md` |
| `RandomTile` | `src/game/generation/RandomTile.cpp` | PLANNED | `analysis/class-map.md` |

---

## Item / Progression System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGItem` / `RGStateItem` | `src/game/items/ItemBase.cpp` | PLANNED | `analysis/class-map.md` |
| `RGBuff` | `src/game/status/Buff.cpp` | PLANNED | `analysis/class-map.md` |
| `RGChest` / `RGChestAppoint` / `RGChestBig` / `RGChestBigButton` / `RGChestBoss` / `RGChestMaterial` / `RGChestRoomReward` | `src/game/items/Chest.cpp` | PLANNED | `analysis/class-map.md` |
| `RGCoin` | `src/game/items/Currency.cpp` | PLANNED | `analysis/class-map.md` |
| `RGHealthPot` / `RGHealthPotBox` / `RGArmorPot` / `RGEnergyPot` | `src/game/items/Consumable.cpp` | PLANNED | `analysis/class-map.md` |
| `RGSecretKey` / `RGSecretKeyList` | `src/game/items/Key.cpp` | PLANNED | `analysis/class-map.md` |
| `ItemObjectProvider` | `src/game/items/ItemProvider.cpp` | PLANNED | `analysis/class-map.md` |
| `RGGameInfo` (progression) | `src/game/progression/ProgressionSystem.cpp` | PLANNED | `analysis/class-map.md` |
| `RGCharacterInfo` (unlocks) | `data/characters/*.toml` | PLANNED | `analysis/class-map.md` |

---

## UI / Audio / Sandbox

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGUIWindow` | `src/game/ui/UIWindow.cpp` | PLANNED | `analysis/class-map.md` |
| `RGMusicManager` | `src/platform/audio/AudioEngine.cpp` | PLANNED | `docs/AUDIO.md` |
| `RGMusicManager_emEffect` | `src/game/audio/EffectManager.cpp` | PLANNED | `analysis/class-map.md` |
| `MapManagerSandbox` / `SandboxObject*` / `SandboxConfigData` | `src/game/sandbox/SandboxMode.cpp` | PLANNED | `analysis/class-map.md` |
| `ColorPanel` / `SerializeLinkerColor` | `src/game/generation/ColorPanel.cpp` | PLANNED | `analysis/class-map.md` |
| `SerializeLinkerGravity` / `SerializeLinkerDirection` / `SerializeLinkerSprites` | `src/game/generation/SerializeLinker.cpp` | PLANNED | `analysis/class-map.md` |

---

## Removed / Out of Scope (v1.0)

| Reference | Reason |
|-----------|--------|
| `ChillyRoom_*` (promotion, network, video) | Marketing/analytics SDKs |
| `RGNetWorkManager` / `PendingPlayer*` / `MultiPlayerView` | Multiplayer — out of scope |
| `EnemySoccer` / `MapManagerSoccer` / `RaceController` / `RGRaceCar` | Soccer/Race modes — out of scope |
| `GunRace` | Race mode weapon — out of scope |
| `BoxSandBox` / `ExplodeBoxSandBox` / `GunBox*` / `ObjectExplodeBoxRace` | Sandbox/Race — out of scope |
| `SerializeLinkerRoom_SBRoomInfo_SBEnemeyRoomInfo` | Part of serialization — mapped to `SerializeLinker.cpp` |
| IronSource, Facebook, Firebase, AppsFlyer, Ad SDKs | Analytics/ads — removed |

---

## Mapping Guidelines

1. **One native file per logical system** — not per class
2. **Data-driven content** → `data/<category>/*.toml` or `*.json`
3. **Shared base classes** → `*Base.cpp` with virtual interface
4. **Variant-heavy classes** (EnemyAI01-20) → data table + single behavior system
5. **Component-based** — prefer composition over inheritance in native

---

## Update Protocol

- When implementing a system: Change status to `IN_PROGRESS`
- When tests pass: Change to `TESTED`
- When fidelity verified: Change to `VERIFIED`
- Update `docs/PARITY.md` in parallel

---

**Last Updated:** 2026-10-30
**Next Review:** After each Phase 5 system implementation