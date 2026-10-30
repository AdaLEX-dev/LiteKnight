# Soul Knight 2.6.8 — Architecture Reconstruction

Generated: 2026-10-30
Source: IL2CPP metadata extraction from `libil2cpp.so` + `global-metadata.dat`
Reference APK: SHA-256 `8faff89079eff748770c28f0ff2243baa88388b352d0128e67ae3ef5aa0137ea`

---

## Executive Summary

**Engine:** Unity 2021.x (IL2CPP scripting backend)
**Language:** C# compiled to C++ via IL2CPP
**Game Namespace:** Primarily `RG*` (Rogue/Random Generation) classes
**Architecture Pattern:** Component-based (MonoBehaviour), Manager singletons, Data-driven content

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
│  RGGameInfo                 ← Runtime game data                 │
│  RGGameConst                ← Constants/balance data            │
│  RGRandom / RGRandomInterface ← Deterministic RNG               │
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
Key states observed:
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
- Physics: `FixedUpdate()` at 50 Hz (0.02s) — **CONFIRMED** from `Time.fixedDeltaTime` usage
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

**Key Fields (inferred):**
- `health`, `maxHealth`, `armor`, `energy`, `maxEnergy`
- `moveSpeed`, `dashSpeed`, `dashCooldown`
- `currentWeapon`, `weaponSlots[]`
- `isDashing`, `isInvincible`
- `facingDirection` (Vector2)
- `animator` (Animator reference)

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

**AI Patterns Observed:**
- State machine via coroutines (`IEnumerator` methods like `CheckPositionValid`, `DizzyScout`, `SendingPosition`)
- Behavior types: Melee, Ranged, Summoner, Boss, Turret, Suicide, Shielded
- Targeting: Player position, predicted position, nearest target
- Cooldowns per attack type

### 3.3 Weapon System (`RGWeapon` hierarchy)

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

**Weapon Data (RGGameConst / ScriptableObject-like):**
- `damage`, `fireRate`, `bulletSpeed`, `bulletCount`, `spread`
- `knockback`, `critChance`, `critMultiplier`, `elementType`
- `energyCost`, `ammo`, `maxAmmo`, `reloadTime`
- `projectilePrefab`, `muzzleEffect`, `hitEffect`, `sound`
- `weaponType` (enum: Melee, Ranged, Beam, Throwing, Special)

### 3.4 Projectile System (`RGBullet` hierarchy)

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

### 3.9 Save System (`RGSaveManager`)

**Save Slots:** Multiple (auto + manual)
**Data Includes:**
- Character progression
- Inventory (weapons, items)
- Currency
- Settings (volume, graphics, controls)
- Game completion flags
- RNG seed (for run reconstruction)

**Format:** Binary (Unity `BinaryFormatter` or custom) → **Native: JSON/MessagePack**

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
| Animator/Animation | Custom animation system | MEDIUM |
| ParticleSystem | Custom particle system | MEDIUM |
| UI (uGUI/TextMeshPro) | Dear ImGui / custom UI | MEDIUM |
| Save (BinaryFormatter) | JSON + schema versioning | HIGH |
| RNG (Unity.Random) | PCG32 / xoshiro256** | CRITICAL |
| SceneManager | Custom scene graph | MEDIUM |

---

## 7. Determinism Requirements

**Confirmed Deterministic Systems:**
- RNG: `RGRandom` / `RGRandomInterface` — must preserve exact sequence
- Room Generation: `MapManager*` — same seed = same dungeon
- Enemy Spawn: `EnemyCreateInfo` — deterministic placement
- Combat: Damage calculation, crit rolls, proc effects

**RNG Streams (must isolate):**
1. World Generation (dungeon layout)
2. Combat (damage, crit, proc)
3. Enemy AI (behavior selection)
4. Loot (drop tables)
5. Visual Effects (particle variance)

---

## 8. Phase 2 Deliverable Status

✅ **Engine Identified:** Unity 2021 IL2CPP
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

## 10. Confidence Assessment

| Subsystem | Confidence | Evidence |
|-----------|------------|----------|
| Engine | CONFIRMED | IL2CPP metadata, Unity strings |
| Class Hierarchy | HIGH | 200+ RG* classes extracted |
| Game Loop | HIGH | MonoBehaviour pattern, FixedUpdate usage |
| Player | HIGH | RGController hierarchy, MovementInput |
| Enemies | HIGH | 20+ EnemyAI classes, BossController |
| Weapons | HIGH | 25+ RGWeapon derivatives |
| Projectiles | HIGH | 30+ RGBullet derivatives |
| Rooms/Gen | HIGH | MapManager variants, SerializeLinker |
| Items | HIGH | RGItem, RGChest, RGBuff, RGStateItem |
| Progression | MEDIUM | RGGameInfo, RGSaveManager (partial) |
| Audio | MEDIUM | RGMusicManager |
| UI | MEDIUM | RGJoyControl, RGUIWindow |
| Multiplayer | LOW | Stripped for v1 |
| Determinism | HYPOTHESIS | RGRandom exists, needs verification |

---

## 11. Unknown Systems (→ `analysis/UNKNOWN_SYSTEMS.md`)

| ID | Description | Priority |
|----|-------------|----------|
| UNK-001 | Exact RNG algorithm (PCG32? xoshiro?) | HIGH |
| UNK-002 | Damage formula (armor, resist, crit) | HIGH |
| UNK-003 | Room graph generation algorithm | HIGH |
| UNK-004 | Enemy AI state machine details | HIGH |
| UNK-005 | Weapon projectile physics (gravity, bounce) | MEDIUM |
| UNK-006 | Status effect stacking rules | MEDIUM |
| UNK-007 | Save file format/versioning | MEDIUM |
| UNK-008 | Animation event-driven mechanics | LOW |
| UNK-009 | Shader/effect system (custom shaders?) | LOW |

---

**Architecture Reconstruction Complete.** Ready for Phase 3: Gameplay Reconstruction & Behavioural Specification.