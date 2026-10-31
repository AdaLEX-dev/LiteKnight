# Soul Knight 2.6.8 — Architecture Reconstruction

> **Current implementation (2026-10-31):** the Phase 4 foundation follows the
> ownership model in `docs/PHASE_4_NATIVE_RUNTIME.md`. Implemented layers:
> `Application` lifecycle, SDL3/Wayland window, Vulkan context/swapchain, batched
> sprite renderer, 50 Hz fixed clock, render-world `SpriteBatch`, and PNG → RGBA8
> `TextureData` asset boundary, controlled Unity Texture2D/Sprite conversion,
> native `SceneLoader`/`SpriteData`/`Camera2D`, an `r1_1`+`c00` composition, and
> native `AnimationClip`/`AnimationPlayer` playback of the real `c00` idle clip.
> Full AnimatorController transitions/layers, general KTX2/atlas breadth/registry,
> input-to-simulation, audio, UI, and gameplay are not implemented. Gate B
> remains **PARTIAL**; Gate C is **PARTIAL**. Evidence:
> `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`, `analysis/assets/animation.md`.
>
> The text below is a historical class inventory, not the implementation
> architecture.
>
> ```text
> Application
>   ├── Platform (SDL3/Wayland, filesystem, timing)
>   ├── Asset Runtime (decoded registry; conversion output)
>   ├── Renderer (Vulkan device/swapchain/sprites)
>   ├── Input / Audio
>   └── Simulation (50 Hz fixed clock, RNG, ECS world)
>         └── Gameplay (controllers, projectiles, damage, rooms)
> ```
>
> Reference facts now confirmed: fixed timestep 0.02 s (50 Hz), Physics2D gravity
> `(0,-9.81)`, legacy input axes, damage bridge player `+0x228` / enemy `+0x2C8` /
> box `+0x168`, `CanDie` checks `RGGameProcess.this_index`.

Generated: 2026-10-30
Source: IL2CPP metadata extraction from `libil2cpp.so` + `global-metadata.dat`
Reference APK: SHA-256 `8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea`

> **Correction — 2026-10-31.** This document was originally presented as a complete reconstruction. It is a **class inventory plus unmarked inference**. It is not a behavioural specification.
>
> - Engine: **Unity 2018.4.12f1**, metadata **version 24** — not Unity 2021.x / v24.1.
> - Metadata supplies class names, field names/offsets, method names/signatures, and RVAs. It does **not** supply method bodies.
> - Any field list below marked "inferred", "likely", or based only on a class name is **HYPOTHESIS**, not CONFIRMED.
> - Corrected field-level evidence is in `analysis/re/` as each subsystem is reconstructed.
> - Gate B is **PARTIAL**; the original "complete" status was wrong. See `docs/GATE_B.md`.
>
> Historical claims are retained below where they are useful for audit. See `analysis/PROJECT_DIAGNOSIS.md` and `analysis/EVIDENCE_AUDIT.md`.

---

## Executive Summary

**Engine:** **Unity 2018.4.12f1** (IL2CPP scripting backend, metadata version 24)
**Language:** C# compiled to C++ via IL2CPP
**Game Namespace:** Primarily `RG*` (Rogue/Random Generation) classes
**Architecture Pattern:** Structural evidence shows MonoBehaviour-based components, manager singletons, and serialized data. Behaviour is not yet reconstructed.

---

## 1. High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Unity Engine                              │
├─────────────────────────────────────────────────────────────────┤
│  Game Loop (MonoBehaviour: Awake/Start/Update/FixedUpdate)      │
│  Physics (PhysX 2D/3D)                                          │
│  Rendering (SpriteRenderer, Canvas, ParticleSystem)             │
│  Audio (AudioSource, AudioListener)                             │
│  Input (Touch, Keyboard, Gamepad via Unity Input)               │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Game Systems (C# → IL2CPP)                   │
├─────────────────────────────────────────────────────────────────┤
│  RGGameProcess              ← Main game state machine           │
│  RGGameSceneManager         ← Scene loading/transitions         │
│  RGGameInfo                 ← run seeds + network seed sync     │
│  RGGameConst                ← constants/balance (unverified)    │
│  RGRandom / RGRandomInterface ← local wrapper over Unity RNG    │
│  RGSaveManager              ← Persistence                       │
│  RGNetWorkManager           ← Multiplayer (out of scope v1)     │
└─────────────────────────────────────────────────────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│   Player/Char   │ │    Enemies/AI   │ │  Weapons/Combat │
├─────────────────┤ ├─────────────────┤ ├─────────────────┤
│ RGController    │ │ RGEController   │ │ RGWeapon        │
│ RGBaseController│ │ EnemyAI01-20    │ │ RGSword         │
│ RGPetController │ │ RGEBossController│ │ RGLaser       │
│ RGMountController│ │ RGEWeapon       │ │ RGDrill        │
│ RGCharacterInfo │ │ RGELaser        │ │ RGShortLaser    │
│ RGCharGray      │ │ EnemyDummy      │ │ RGChain         │
│ RGJoyControl    │ │ EnemySoccer     │ │ RGBullet*       │
└─────────────────┘ └─────────────────┘ └─────────────────┘
          │                   │                   │
          └───────────────────┼───────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                  World / Generation                              │
├─────────────────────────────────────────────────────────────────┤
│  MapManager (base)                                                │
│   ├─ MapManagerLevel      — Story mode                          │
│   ├─ MapManagerEndless    — Endless mode                        │
│   ├─ MapManagerDefence    — Defence mode                        │
│   ├─ MapManagerBR         — Battle Royale                       │
│   ├─ MapManagerRace       — Race mode                           │
│   ├─ MapManagerSandbox    — Creative mode                       │
│   ├─ MapManagerSoccer     — Soccer mode                         │
│   └─ MapManagerSurvive    — Survival mode                       │
│                                                                  │
│  RGRoomX / RGRoomXEndless / RGRoomXZombie — Room templates     │
│  RGDoor / RGDoorTrigger / RGTransferGate — Connections         │
│  RGAisle / RGBigRoom / RGMaze — Room types                     │
│  SerializeLinker* — Room data serialization                    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Items / Pickups                              │
├─────────────────────────────────────────────────────────────────┤
│  RGItem → RGStateItem                                           │
│  RGBuff                                                         │
│  RGChest* (Chest, Appoint, Big, BigButton, Boss, Material,     │
│           RoomReward)                                           │
│  RGCoin                                                         │
│  RGHealthPot / RGHealthPotBox / RGArmorPot / RGEnergyPot       │
│  RGSecretKey / RGSecretKeyList                                  │
│  ItemObjectProvider (sandbox)                                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Game Loop

### Unity Message Flow (per MonoBehaviour)
```
Awake() → OnEnable() → Start() → [Update()/FixedUpdate()/LateUpdate()] → OnDisable() → OnDestroy()
```

### Game State Machine (RGGameProcess)
Candidate states (HYPOTHESIS; not observed in a runtime trace):
- Initialization
- Main Menu (Hall)
- Character Selection
- Level Loading
- Gameplay (In-Room)
- Room Transition
- Boss Fight
- Game Over / Victory
- Result Screen
- Save/Load

### Fixed Timestep
- Physics: `FixedUpdate()` at 50 Hz (0.02 s) is now **CONFIRMED** from the serialized `TimeManager.Fixed_Timestep` in `globalgamemanagers`; see `analysis/re/timing.md`. This supersedes the earlier unsupported status. Render target remains 60 FPS.
- Rendering: `Update()` at monitor refresh (variable)
- **Critical:** Simulation speed MUST NOT depend on frame rate

---

## 3. Subsystem Breakdown

### 3.1 Player System (`RGController` hierarchy)

```
RGBaseController (MonoBehaviour)
  ├─ RGController (Player)
  │    ├─ MovementInput handling
  │    ├─ Weapon management (currentWeapon, weaponSlots)
  │    ├─ Skill/Ability system
  │    ├─ Dodge/Roll mechanics
  │    ├─ Health/Armor/Energy stats
  │    ├─ Status effects (buffs/debuffs)
  │    ├─ Input: RGJoyControl (virtual joystick + buttons)
  │    └─ Animations: Animator parameters
  ├─ RaceController (Race mode)
  └─ RGDeveloperController (Debug)
```

**Key Fields — corrected from metadata (`dump.cs`), not inferred:**

`RGController` itself contains:
- `role_attribute` (`RoleAttributePlayer`)
- `skills` (`SkillInfo[]`), `skillIndex`, `skillCasting`, `skill_time`
- `movementInput` (`MovementInput`)
- `rg_random` (`RGRandom`)
- `hand` (`RGHand`), `move_dir`, `facing`, `in_battle`, `can_hurt`
- multiple `Action` callbacks (`OnRoleAttack`, `OnRoleSkillStart`, etc.)

Health/armor/energy are **not** fields of `RGController`; they live in `RoleAttribute` / `RoleAttributePlayer`:
- `RoleAttribute`: `camp`, `speed`, `speed_rate`, `max_hp`, `hp`, `critical`
- `RoleAttributePlayer`: `max_armor`, `armor`, `max_energy`, `_energy`, `atk`, `atk_speed`, `defence`, `c_index`, `c_level`

**Behaviour is UNKNOWN/HYPOTHESIS until reconstructed from disassembly or runtime observation.** Field presence confirms data layout only.

### 3.2 Enemy System (`RGEController` hierarchy)

```
RGEController (RandomBehaviour) ← Base enemy AI
  ├─ EnemyAI01...EnemyAI20 (numbered behaviors)
  ├─ EnemyAIAlien (has captain_ai reference)
  ├─ EnemyAIChestBattery (turret-like)
  ├─ EnemyAIFrog
  ├─ EnemyAIGhost
  ├─ EnemyAIStake
  ├─ EnemyAITombStone01/02
  ├─ EnemyAIWitch (has dashing coroutine)
  ├─ RGEControllerRace (Race mode)
  ├─ RGEBossController (Boss phases)
  ├─ RGEWeapon (Enemy weapons)
  ├─ RGELaser / RGELaserTrigger / RGEnergyBallTrigger / RGEnergyPot
  ├─ RGERotationLaser / RGEShortLaser
  └─ RGEHand (Enemy hand/weapon holder)
```

**AI Patterns — HYPOTHESIS, not observed behaviour:**
- Metadata shows `IEnumerator` classes and method names such as `CheckPositionValid`, `DizzyScout`, `SendingPosition`. That proves coroutine-shaped code exists; it does not reveal the state machine.
- Behaviour types (Melee, Ranged, Summoner, Boss, Turret, Suicide, Shielded) were inferred from class names and are **not confirmed**.
- Targeting and cooldown rules are **UNKNOWN** until disassembled or observed.

A reusable method → RVA → disassembly workflow is being added under `tools/re/`. Evidence documents belong in `analysis/re/`.

### 3.3 Weapon System (`RGWeapon` hierarchy)

> The parenthetical annotations in this hierarchy (melee, beam, homing, etc.) are inferred from class names and are **HYPOTHESIS**, not reconstructed behaviour.

```
RGWeapon (MonoBehaviour) ← Base weapon
  ├─ RGSword (melee, slash triggers)
  ├─ RGLaser (beam, continuous)
  ├─ RGShortLaser (short beam)
  ├─ RGDrill (piercing, drill trigger)
  ├─ RGChain (chain lightning?)
  ├─ RGRuinSword (special)
  ├─ RGBox / RGTBox (box-type weapons)
  ├─ RGBatteryController (battery weapons)
  ├─ RGBothPot (potion thrower?)
  ├─ RGAutoDestory (auto-destroy projectile)
  ├─ RGSBullet01 / RGSBullet02 (special bullets)
  ├─ RGSBulletFollow (homing)
  ├─ RGTaskBullet (task-based)
  ├─ GunBox / GunBoxChangable / GunBoxCharactor / GunBoxColor / GunBoxMultiObject / GunBoxRoom / GunBoxStartPoint / GunBoxBattery
  ├─ GunJetPack
  ├─ GunMagicStaff
  └─ GunTelescope
```

**Weapon Data — corrected from metadata (`dump.cs`):**

`RGWeapon` fields include:
- `weapon_type` (`emWeaponType`), `weapon_tags`
- `bulletsInfo` (`List<BulletInfoInspector>`)
- `deviation`, `consume`, `weapon_speed`, `atk_move_speed`
- `item_level`, `item_value`, `is_melee`, `need_lock`, `has_effect`
- `rg_random`, `weapon_item` (`WeaponItemBase`), `controller` (`RGBaseController`)
- `attack_count`, `attackPressed`, speed modifiers, and weapon-special callbacks

The original list (`damage`, `fireRate`, `bulletCount`, `spread`, `energyCost`, `ammo`, `reloadTime`, etc.) was **inferred and does not match the metadata field set**. Exact damage/fire-rate behaviour is **UNKNOWN** and must be reconstructed from `RGWeapon.AttackStart`, `BulletInfo`, `DamageCarrier`, and related disassembly.

`BulletInfo` fields include: `bulletProto`, `sourceObject`, `sourceWeapon`, `createPosition`, `initVelocity`, `speed`, `size`, `directionAngle`, `reflectCount`, `duration`, `camp`.

`DamageInfo` fields include: `damageType`, `damage`, `throughCount`, `critic`, `repel`, `damageInterval`, `camp`.

Field names are structural evidence; formulas are not implied.

### 3.4 Projectile System (`RGBullet` hierarchy)

> Parenthetical trigger/effect roles are inferred from class names and remain **UNVERIFIED**.

```
RGBullet (DamageCarrier)
  ├─ RGBulletTrigger (trigger-based)
  │    ├─ RGBulletModTrigger (modifiable)
  │    ├─ RGBulletBuffTrigger (applies buff)
  │    ├─ RGBulletEffectTrigger (VFX)
  │    ├─ RGBulletTriggerWall (wall bounce)
  │    ├─ RGArrowTrigger / RGArrowBuffTrigger / RGArrowBuffTriggerElf / RGArrowThroughTrigger / RGArrowTriggerExplode / RGArrowTriggerIce
  │    ├─ RGSwordTrigger / RGSwordTriggerIce / RGSwordBuffTrigger
  │    ├─ RGDrillTrigger / RGShortLaserTrigger
  │    ├─ RGChain / RGShield / RGParticleTrigger
  │    ├─ RGBTCreate / RGBTDelayCreate / RGBTDivision / RGBTEnergy / RGBTExplode / RGBTExplodeThrow / RGBTHeart / RGBTMultiExplode / RGBTRebound / RGBTSpawn
  ├─ RGBDelayCreate (delayed spawn)
  ├─ RGBDelayDivision (split on delay)
  ├─ RGBMoveToSpawn (move to target)
  ├─ RGTaskBullet (scripted behavior)
  ├─ RGArrow* (arrow variants)
  └─ RGSwordSlash (melee arc)
```

**DamageCarrier Interface:**
- `damage`, `knockback`, `critChance`, `element`
- `owner` (RGController), `weaponSource` (RGWeapon)
- `OnHit(target)`, `OnTriggerEnter(collider)`

### 3.5 Room / Procedural Generation

```
MapManager (Singleton)
  ├─ MapManagerLevel (Story)
  │    ├─ CreateRoom coroutine
  │    ├─ LoadingRoomBundle coroutine
  │    ├─ EnemyInfo per room
  │    └─ Room graph (nodes/edges)
  ├─ MapManagerEndless
  │    ├─ CreateRoom coroutine
  │    └─ CreatingEnemy coroutine
  ├─ MapManagerDefence
  │    ├─ WaveInfo[] (enemy waves)
  │    ├─ EnemyCreateInfo (melee/ranged split)
  │    └─ Direction enum
  ├─ MapManagerBR (Battle Royale)
  ├─ MapManagerRace
  ├─ MapManagerSandbox
  ├─ MapManagerSoccer
  └─ MapManagerSurvive
```

**Room Structure (`RGRoomX`):**
- `RGRoomArea` — Sub-areas within room
- `RGDoor` / `RGDoorTrigger` — Entrances/exits
- `RGTransferGate` / `RGTransferGateEndless` — Level transitions
- `RGAisle` — Corridors
- `RGBigRoom` — Large rooms
- `RGMaze` — Maze generation
- `RGStartRoomEvent` — Starting room logic

**Serialization (`SerializeLinker*`):**
- `SerializeLinkerRoom` → `SBRoomInfo` (serialized room)
  - `SBEnemeyRoomInfo` — Enemy placements
- `SerializeLinkerGravity` → `SBGravityInfo`
- `SerializeLinkerColor` → `SBColor`
- `SerializeLinkerDirection` — Door connections
- `SerializeLinkerSprites` — Visual variants

### 3.6 Item / Progression System

> The item/chest hierarchy is structural inventory only; loot and progression behaviour is **UNKNOWN**.

```
RGItem (SerializedRandomBahaviour) ← Base item
  ├─ RGStateItem (stateful items)
  └─ ItemObjectProvider (sandbox)

RGBuff (MonoBehaviour) — Temporary effects
  ├─ Speed, Damage, Defense, FireRate, etc.
  ├─ Duration, stacking rules
  └─ Visual indicators

RGChest (MonoBehaviour) — Loot containers
  ├─ RGChestAppoint (fixed loot)
  ├─ RGChestBig (large)
  ├─ RGChestBigButton (requires button)
  ├─ RGChestBoss (boss reward)
  ├─ RGChestMaterial (crafting materials)
  └─ RGChestRoomReward (room clear reward)

RGCoin — Currency
RGHealthPot / RGHealthPotBox / RGArmorPot / RGEnergyPot — Consumables
RGSecretKey / RGSecretKeyList — Progression keys
```

**Progression (RGGameInfo / RGSaveManager):**
- Unlocked characters
- Unlocked weapons
- Character upgrades (stats, skills)
- Achievements/Statements
- Currency (coins, gems, energy)
- Game completion stats

### 3.7 UI System

```
RGUIWindow (base)
RGJoyControl (virtual joystick)
  ├─ RGJoyStick (movement)
  ├─ RGJoyBtn (attack)
  ├─ RGJoyBtnSkill (skill/ability)
  ├─ RGJoyBtnSpecial (special)
  └─ RGJoyBtnWeapon (weapon switch)

ChillyRoom_UI* (promotion/marketing UI - REMOVE)
```

**HUD Elements (inferred):**
- Health/Armor/Energy bars
- Weapon icon + ammo
- Skill cooldowns
- Minimap (if any)
- Pickup prompts
- Pause menu

### 3.8 Audio System

```
RGMusicManager (singleton)
  ├─ BGM per biome/state
  ├─ emEffect (sound effects)
  └─ Volume controls (master, music, sfx)
```

**Audio Triggers:**
- Weapon fire/hit
- Enemy hit/death
- Player hit/dodge/death
- Pickup/chest open
- Room transition
- Boss phase change
- UI interactions

### 3.9 Save System (`RGSaveManager`) — corrected

**Metadata fields (`dump.cs`):**
- `char_list` (`RGCharacterInfo[]`)
- `pet_list` (`RGPetInfo[]`)
- `hero_skin_list` (`RGSpriteList[]`)
- `game_data` (`AboGameData`)
- `store_key` (`string`)
- `c_levelup_gems` (`int[]`)

**Format:** The assembly references **Sirenix.Serialization / Odin** and uses `AboGameData`. The original "BinaryFormatter or custom binary" claim was a guess; the original "multiple slots" claim and the detailed JSON schema in `docs/SAVE_SYSTEM.md` were **invented**, not reconstructed.

**Status:** Save file format and serialization behaviour are **UNKNOWN** until `RGSaveManager.Save/Load` and `AboGameData` are disassembled. A native JSON format is a new design, not reference parity.

---

## 4. Data-Driven Content

### Content Definitions (likely ScriptableObjects or static data classes)

| Data Type | Class | Fields (Inferred) |
|-----------|-------|-------------------|
| Weapon | `WeaponConfig` / `WeaponItemInfo` | id, name, damage, fireRate, bulletSpeed, spread, knockback, element, energyCost, projectilePrefab, sounds, animations |
| Enemy | `EnemyCreateInfo` / `EnemyInfos` | id, name, hp, damage, speed, aiType, attackPatterns, lootTable, spawnWeight, bossPhaseData |
| Character | `CharacterInfo` / `HeroSkinWeaponIndex` | id, name, baseStats, startingWeapon, skill, passive, skinData |
| Room | `SerializeLinkerRoom.SBRoomInfo` | roomId, width, height, tileData, enemySpawnPoints, propSpawnPoints, doorPositions, biome |
| Item | `ItemObjectProvider` / `RGItem` | id, name, type, effect, duration, value, icon, description |
| Buff | `RGBuff` | id, type, value, duration, stackable, icon |
| Wave | `MapManagerDefence_WaveInfo` | waveIndex, enemyGroups[], duration, rewards |

---

## 5. Multiplayer (Out of Scope for v1.0)

**Classes:** `RGNetWorkManager`, `PendingPlayer`, `PendingPlayerInfo`, `ConnectionPendingPlayers`, `MultiPlayerView`, `EnemySoccer`, `MapManagerSoccer`, `RaceController`, `RGRaceCar`

**Note:** All multiplayer code will be **stripped** in native port. Single-player only for v1.0.

---

## 6. Native Port Architecture Mapping

| Unity/IL2CPP System | Native Replacement | Priority |
|---------------------|-------------------|----------|
| Unity Game Loop | Custom fixed-timestep loop | CRITICAL |
| MonoBehaviour | Component system (ECS or manual) | CRITICAL |
| Physics (PhysX) | Box2D / custom 2D physics | HIGH |
| Rendering (SpriteRenderer) | Vulkan 2D sprite batcher | CRITICAL |
| Audio (AudioSource) | miniaudio / PipeWire | HIGH |
| Input (Unity Input) | Wayland + libinput / SDL3 | CRITICAL |
| AssetBundles | Custom asset pipeline (KTX2, glTF, JSON) | HIGH |
| ScriptableObjects | JSON/flatbuffers data files | HIGH |
| MonoBehaviour Coroutines | Custom coroutine scheduler | HIGH |
| Animator/Animation | Native discrete SpriteTrack playback (`AnimationClip`/`AnimationPlayer`); full controller system not reconstructed | MEDIUM |
| ParticleSystem | Custom particle system | MEDIUM |
| UI (uGUI/TextMeshPro) | Dear ImGui / custom UI | MEDIUM |
| Save (BinaryFormatter) | JSON + schema versioning | HIGH |
| RNG (Unity.Random) | Reference-compatible `RGRandomStream` (PCG32 rejected for parity) | CRITICAL |
| SceneManager | Custom scene graph | MEDIUM |

---

## 7. Determinism Requirements

**Determinism claims (corrected):**
- RNG: `RGRandom`/`RGRandomInterface` — core wrapper and Xorshift128 engine reconstructed in `analysis/re/rgrandom.md`; exact native implementation exists. **PARTIAL/CONFIRMED** for the core.
- Room Generation: `MapManager*` — "same seed = same dungeon" is **HYPOTHESIS**; the generation algorithm has not been reconstructed.
- Enemy Spawn: `EnemyCreateInfo` — deterministic placement is **HYPOTHESIS**; not yet traced.
- Combat: the player/enemy/box damage bridge is **CONFIRMED**; critical/element/effect-trigger rules and full death/reborn remain **PARTIAL/UNKNOWN**.

**RNG Streams (candidate isolation domains, not yet proven):**
1. World Generation (dungeon layout)
2. Combat (damage, crit, proc)
3. Enemy AI (behavior selection)
4. Loot (drop tables)
5. Visual Effects (particle variance)

Only the RNG wrapper/engine layer has been reconstructed. Do not claim deterministic generation or combat until their call order is traced.

---

## 8. Phase 2 Deliverable Status

✅ **Engine Identified:** Unity **2018.4.12f1** IL2CPP, metadata v24
✅ **Java/Native Split:** 100% gameplay in `libil2cpp.so`
✅ **Renderer:** Unity SpriteRenderer/Canvas → Native Vulkan
✅ **Game Loop:** MonoBehaviour messages → Custom fixed timestep
✅ **Asset System:** AssetBundles → Custom pipeline
✅ **Audio:** Unity Audio → miniaudio
✅ **Input:** Unity Input → Wayland/libinput
✅ **Persistence:** BinaryFormatter → JSON/MessagePack
✅ **Major Subsystems Mapped:** Player, Enemy, Weapon, Room, Generation, Item, Progression, UI, Audio, Save

---

## 9. Next Steps (Phase 3+)

1. **Behavioural Specification** — Document exact mechanics from RE
   - Player movement/dodge/attack
   - Enemy AI states/transitions
   - Weapon fire patterns/projectile physics
   - Room generation algorithm
   - Damage/armor/crit formulas
   - Status effect system

2. **Native Runtime (Phase 4)**
   - Wayland window + Vulkan context
   - Fixed timestep game loop
   - Input abstraction
   - Asset loading pipeline

3. **First Playable (Phase 5)**
   - Player movement + attack
   - One enemy + collision
   - One weapon + projectile
   - One room + transition
   - Death/restart

---

## 10. Confidence Assessment (corrected)

This table records **structural discovery confidence only**. Structural confidence is not behavioural confidence.

| Subsystem | Structural confidence | Behaviour verified? | Evidence |
|-----------|-----------------------|---------------------|----------|
| Engine | CONFIRMED | N/A | Unity 2018.4.12f1, IL2CPP metadata v24 |
| Class inventory | HIGH | NO | Full metadata dump (132 top-level RG* classes, 3,722 total classes) |
| Game loop | UNKNOWN | NO | No method bodies analyzed; "FixedUpdate" claim lacks evidence |
| Player | UNKNOWN | NO | `RGController` fields exist; mechanics not reconstructed |
| Enemies | UNKNOWN | NO | 20+ AI class names; state machines not reconstructed |
| Weapons | UNKNOWN | NO | 25+ class derivatives; fire logic not reconstructed |
| Projectiles | UNKNOWN | NO | 35+ class derivatives; physics not reconstructed |
| Rooms/Gen | UNKNOWN | NO | `MapManager` classes exist; generation algorithm not reconstructed |
| Items | UNKNOWN | NO | Items/chests exist; loot logic not reconstructed |
| Progression | UNKNOWN | NO | `RGSaveManager`/`RGGameInfo` exist; progression logic not reconstructed |
| Audio | UNKNOWN | NO | `RGMusicManager` exists; trigger semantics not reconstructed |
| UI | UNKNOWN | NO | UI classes exist; behaviour not reconstructed |
| Multiplayer | LOW | N/A | Out of scope for v1 |
| Determinism | PARTIAL | NO | `RGRandom` wraps `UnityEngine.Random.State`; wrapper semantics under reconstruction |

---

## 11. Unknown Systems (→ `analysis/UNKNOWN_SYSTEMS.md`)

| ID | Description | Priority |
|----|-------------|----------|
| UNK-001 | `RGRandom` wrapper semantics over `UnityEngine.Random` (local vs global state, seeding, stream isolation) | HIGH |
| UNK-002 | Damage formula (armor, resist, crit) | HIGH |
| UNK-003 | Room graph generation algorithm | HIGH |
| UNK-004 | Enemy AI state machine details | HIGH |
| UNK-005 | Weapon projectile physics (gravity, bounce) | MEDIUM |
| UNK-006 | Status effect stacking rules | MEDIUM |
| UNK-007 | Save file format/versioning | MEDIUM |
| UNK-008 | Animation event-driven mechanics | LOW |
| UNK-009 | Shader/effect system (custom shaders?) | LOW |

---

**Architecture reconstruction is NOT complete.** This document is a structural inventory plus historical hypotheses. Gate B is **PARTIAL** (`docs/GATE_B.md`). Behavioural reconstruction proceeds subsystem-by-subsystem under `analysis/re/` with evidence from disassembly or runtime observation.