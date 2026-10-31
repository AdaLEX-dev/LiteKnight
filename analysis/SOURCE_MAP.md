# Soul Knight 2.6.8 — Source Map

> **Correction — 2026-10-31.** The original evidence column cited `analysis/class-map.md`, a DEX/SDK class map that does **not** contain the cited `RG*` IL2CPP classes. Those citations were broken. They are replaced with `analysis/re/*.md` and the canonical phase reports. Gameplay implementation statuses remain `PLANNED`; the Phase 4 platform/renderer foundation is implemented and manually verified (see `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`).

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

## Current evidence-backed mappings (Gate B/Phase 3)

| Reference | Native target (planned) | Evidence |
|---|---|---|
| `RGRandom` / `RGRandomInterface` | `src/engine/math/UnityRandom.*` | `analysis/re/rgrandom.md` |
| `RGController` / `RGJoyControl` / `MovementInput` | `src/game/player/*`, `src/platform/input/*` | `analysis/re/input_movement.md` |
| `RGBulletTrigger` / `RGController.GetHurt` / `RGEController.GetHurt` / `RGBox.Hit` | `src/game/combat/*` | `analysis/re/damage.md` |
| `MapManager` / `MapManagerLevel` / `RGRoomX` / `EnemyMaker` | `src/game/generation/*`, `src/game/rooms/*` | `analysis/re/rooms_scene.md` |
| `RGWeapon` / `Gun001` / `RGBullet` / `Bullet01` | `src/game/weapons/*`, `src/game/projectiles/*` | `analysis/re/weapons_projectiles.md` |
| `TimeManager` / `Time` | `src/platform/timing/*`, `src/sim/*` | `analysis/re/timing.md` |
| Native application lifecycle | `src/app/Application.*`, `src/app/AppConfig.*` | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` |
| SDL3 / Wayland window | `src/platform/wayland/Window.*`, `WindowState.hpp` | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` |
| Native Vulkan renderer | `src/engine/rendering/VulkanContext.*`, `VulkanRenderer.*` | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` |
| Native sprite batch/renderer | `src/engine/rendering/SpriteBatch.*`, `SpriteRenderer.*`, `Texture2D.*`, `shaders/*` | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md` |
| PNG asset decoding | `src/engine/assets/PngDecoder.*`, `TextureData.hpp` | `docs/ASSET_PIPELINE.md` |
| Unity Texture2D conversion | `tools/asset/extract_unity.py convert-texture` | `analysis/assets/texture_conversion_manifest.json` |
| Unity Sprite metadata | `src/engine/assets/SpriteData.hpp` | `analysis/assets/sprites.md` |
| Native visual scene | `SceneLoader`, `SceneAsset`, `build_visual_scene.py` | `analysis/assets/scenes/r1_1_visual_manifest.json` |
| 2D camera / coordinates | `src/engine/rendering/Camera2D.*` | `analysis/assets/coordinate_system.md` |
| Unity animation | `src/engine/animation/AnimationClip.*`, `AnimationClipLoader.*`, `AnimationPlayer.*` | `analysis/assets/animation.md` |
| Animation scene generation | `tools/asset/build_visual_scene.py` | `analysis/assets/scenes/animations/c00_idle.json` |
| `EnemyAI04` / `e_slime01` | `src/game/enemies/*` | `analysis/re/enemy_ai04.md` |
| `RGSaveManager` / `PlayerSaveData` | `src/game/save/*` | `analysis/re/save.md` |

## Core Game Flow

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGGameProcess` | `src/game/core/GameProcess.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGGameSceneManager` | `src/game/core/SceneManager.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGGameInfo` | `src/game/core/GameInfo.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGGameConst` | `data/constants/game_constants.toml` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGRandom` / `RGRandomInterface` | `src/engine/math/Random.cpp` | PLANNED | `analysis/UNKNOWN_SYSTEMS.md#UNK-001` |
| `RGSaveManager` | `src/game/save/SaveManager.cpp` | PLANNED | `docs/SAVE_SYSTEM.md` |
| `RGNetWorkManager` | (stripped) | N/A | `docs/PHASE_3_BEHAVIOURAL_RE.md` |

---

## Player System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGBaseController` | `src/game/player/ControllerBase.cpp` | PLANNED | `analysis/re/input_movement.md` |
| `RGController` | `src/game/player/PlayerController.cpp` | PLANNED | `analysis/re/input_movement.md` |
| `RGCharacterInfo` | `data/characters/*.toml` | PLANNED | `analysis/re/input_movement.md` |
| `RGCharGray` | `src/game/player/CharacterVisuals.cpp` | PLANNED | `analysis/re/input_movement.md` |
| `RGPetController` | `src/game/player/PetController.cpp` | PLANNED | `analysis/re/input_movement.md` |
| `RGMountController` | `src/game/player/MountController.cpp` | PLANNED | `analysis/re/input_movement.md` |
| `RGJoyControl` / `RGJoyStick` / `RGJoyBtn*` | `src/platform/input/InputManager.cpp` | PLANNED | `docs/INPUT.md` |
| `MovementInput` | `src/game/player/MovementInput.cpp` | PLANNED | `analysis/re/input_movement.md` |

---

## Enemy System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGEController` | `src/game/enemies/EnemyController.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `EnemyAI01` – `EnemyAI20` | `src/game/enemies/ai/EnemyAI##.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `EnemyAIAlien` | `src/game/enemies/ai/EnemyAI_Alien.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `EnemyAIChestBattery` | `src/game/enemies/ai/EnemyAI_ChestBattery.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `EnemyAIFrog` | `src/game/enemies/ai/EnemyAI_Frog.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `EnemyAIGhost` | `src/game/enemies/ai/EnemyAI_Ghost.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `EnemyAIStake` | `src/game/enemies/ai/EnemyAI_Stake.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `EnemyAITombStone01/02` | `src/game/enemies/ai/EnemyAI_TombStone.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `EnemyAIWitch` | `src/game/enemies/ai/EnemyAI_Witch.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `RGEBossController` | `src/game/bosses/BossController.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `RGEWeapon` / `RGEHand` / `RGELaser*` | `src/game/enemies/EnemyWeapons.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `EnemyDummy` | `src/game/enemies/EnemyDummy.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `EnemySoccer` | (stripped) | N/A | `analysis/re/rooms_scene.md` |

---

## Weapon System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGWeapon` | `src/game/weapons/WeaponBase.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGSword` / `RGSwordBuffTrigger` | `src/game/weapons/melee/Sword.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGLaser` / `RGLaserBuff` | `src/game/weapons/beam/Laser.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGShortLaser` | `src/game/weapons/beam/ShortLaser.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGDrill` / `RGDrillTrigger` | `src/game/weapons/melee/Drill.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGChain` | `src/game/weapons/special/Chain.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGRuinSword` | `src/game/weapons/melee/RuinSword.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGBox` / `RGTBox` / `RGBatteryController` | `src/game/weapons/deployable/Box.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGBothPot` / `RGAutoDestory` | `src/game/weapons/thrown/Pot.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGSBullet01` / `RGSBullet02` / `RGSBulletFollow` | `src/game/weapons/special/SpecialBullets.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGTaskBullet` | `src/game/weapons/special/TaskBullet.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `GunBox*` / `GunJetPack` / `GunMagicStaff` / `GunTelescope` | `src/game/weapons/gunbox/*.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGAim` | `src/game/weapons/AimComponent.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGDash` | `src/game/player/DashComponent.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |

---

## Projectile System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGBullet` / `DamageCarrier` | `src/game/projectiles/ProjectileBase.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGBulletTrigger*` | `src/game/projectiles/triggers/*.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGBDelayCreate` / `RGBDelayDivision` | `src/game/projectiles/DelayedProjectile.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGBMoveToSpawn` | `src/game/projectiles/HomingProjectile.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGBTCreate` / `RGBTDelayCreate` / `RGBTDivision` | `src/game/projectiles/TriggerProjectile.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGBTEnergy` / `RGBTExplode` / `RGBTExplodeThrow` | `src/game/projectiles/ExplosiveProjectile.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGBTHeart` / `RGBTMultiExplode` / `RGBTRebound` / `RGBTSpawn` | `src/game/projectiles/SpecialProjectile.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGArrowTrigger*` / `RGArrowBuffTrigger*` | `src/game/projectiles/ArrowProjectile.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGSwordTrigger*` / `RGSwordSlash` | `src/game/projectiles/MeleeArc.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGDrillTrigger` / `RGShortLaserTrigger` | `src/game/projectiles/BeamProjectile.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |
| `RGChain` / `RGShield` / `RGParticleTrigger` | `src/game/projectiles/EffectProjectile.cpp` | PLANNED | `analysis/re/weapons_projectiles.md` |

---

## Room / Generation System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `MapManager` (base) | `src/game/generation/MapManager.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `MapManagerLevel` | `src/game/generation/MapManagerLevel.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `MapManagerEndless` | `src/game/generation/MapManagerEndless.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `MapManagerDefence` | `src/game/generation/MapManagerDefence.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `MapManagerBR` | (stripped) | N/A | `analysis/re/rooms_scene.md` |
| `MapManagerRace` | (stripped) | N/A | `analysis/re/rooms_scene.md` |
| `MapManagerSandbox` | `src/game/generation/MapManagerSandbox.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `MapManagerSoccer` | (stripped) | N/A | `analysis/re/rooms_scene.md` |
| `MapManagerSurvive` | `src/game/generation/MapManagerSurvive.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `MapManagerPattern` | `src/game/generation/MapManagerPattern.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `RGRoomX` / `RGRoomXEndless` / `RGRoomXZombie` | `src/game/rooms/RoomTemplate.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `RGAisle` / `RGBigRoom` / `RGMaze` | `src/game/rooms/RoomTypes.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `RGDoor` / `RGDoorTrigger` | `src/game/rooms/Door.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `RGTransferGate` / `RGTransferGateEndless` | `src/game/rooms/TransferGate.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `RGStartRoomEvent` | `src/game/rooms/StartRoom.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `SerializeLinker*` | `src/game/generation/SerializeLinker.cpp` | PLANNED | `analysis/re/rooms_scene.md` |
| `RandomTile` | `src/game/generation/RandomTile.cpp` | PLANNED | `analysis/re/rooms_scene.md` |

---

## Item / Progression System

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGItem` / `RGStateItem` | `src/game/items/ItemBase.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGBuff` | `src/game/status/Buff.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGChest` / `RGChestAppoint` / `RGChestBig` / `RGChestBigButton` / `RGChestBoss` / `RGChestMaterial` / `RGChestRoomReward` | `src/game/items/Chest.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGCoin` | `src/game/items/Currency.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGHealthPot` / `RGHealthPotBox` / `RGArmorPot` / `RGEnergyPot` | `src/game/items/Consumable.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGSecretKey` / `RGSecretKeyList` | `src/game/items/Key.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `ItemObjectProvider` | `src/game/items/ItemProvider.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGGameInfo` (progression) | `src/game/progression/ProgressionSystem.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGCharacterInfo` (unlocks) | `data/characters/*.toml` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |

---

## UI / Audio / Sandbox

| Reference | Native | Status | Evidence |
|-----------|--------|--------|----------|
| `RGUIWindow` | `src/game/ui/UIWindow.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `RGMusicManager` | `src/platform/audio/AudioEngine.cpp` | PLANNED | `docs/AUDIO.md` |
| `RGMusicManager_emEffect` | `src/game/audio/EffectManager.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `MapManagerSandbox` / `SandboxObject*` / `SandboxConfigData` | `src/game/sandbox/SandboxMode.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `ColorPanel` / `SerializeLinkerColor` | `src/game/generation/ColorPanel.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |
| `SerializeLinkerGravity` / `SerializeLinkerDirection` / `SerializeLinkerSprites` | `src/game/generation/SerializeLinker.cpp` | PLANNED | `docs/PHASE_3_BEHAVIOURAL_RE.md` |

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

**Last Updated:** 2026-10-31 (Gate B / Phase 3 pass; evidence citations corrected, statuses remain PLANNED)
**Next Review:** After each evidence-backed subsystem reconstruction
