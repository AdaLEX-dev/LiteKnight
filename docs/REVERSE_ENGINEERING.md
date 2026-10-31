# Soul Knight 2.6.8 — Reverse Engineering Log

Generated: 2026-10-30
Session: Initial reconnaissance

> **Correction — 2026-10-31.** The original log called Phase 2 "complete" and recorded Unity 2021.x / metadata v24.1. Both are wrong. The reference is **Unity 2018.4.12f1**, metadata **version 24**. The metadata dump is structural evidence. Gate B is now **PARTIAL** (`docs/GATE_B.md`); core engine primitives and the vertical-slice damage/input/room contracts are reconstructed, but full gameplay/content remains partial. See `analysis/PROJECT_DIAGNOSIS.md`, `analysis/EVIDENCE_AUDIT.md`, and `analysis/re/`.

---

## 2026-10-31 — First Playable Combat Prototype

- Recovered the held attack path `RGJoyControl.UpdateBtnAtkClick ->
  RGController.BtnAtkClick -> RGWeapon.SetAttack -> AttackStart/Stop ->
  set_atk_b -> Gun001.Attack`.
- Recovered weapon_000's cadence directly from its AnimatorController:
  `w_pistol` loops for 1/3 s and its `Attack` AnimationEvent fires at 1/30 s.
  `RGWeapon.Start` speed factors are 1.0 for this prefab/player.
- Corrected the spread field: `RoleAttribute.deviation * (weapon.deviation+1)`;
  c00's attribute deviation is 0, so weapon_000 has 0° spread.
- Implemented native `Weapon`, `ProjectileSystem`, `HealthState`, and
  `EnemySystem`; effective bullet speed 36 (prefab 10 retained), damage 3,
  lifetime 5 s; deterministic swept collision and wall-order target rejection.
- Added two explicit `e_slime01` fixtures (hp 12, contact 3/1.5 s) with dead
  sprite, immediate collision disable, and 4.0 s despawn. Corrected the earlier
  0.95 death-delay claim: 0.95 is `friction`; `dead_destory_delay_time` defaults
  to 4.0 s.
- Added player contact damage/death/reset (`R`) and a native aim vector
  (mouse/right stick) separate from movement/facing.
- Fixed the body-shift regression on facing changes by mirroring sprite quad
  geometry around the pivot, not only UVs.
- Evidence: `analysis/re/combat_prototype.md`; tests: 84 cases / 4759
  assertions in RelWithDebInfo and Debug ASan/UBSan.

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
- **Gate B blockers**: runtime entrance bits for a generated `r1_1`, the
  event-to-`MapManagerLevel` traversal listener, door transition timing, weapon
  fire/projectile velocity, and full death/reborn. Wall/door gap placement and
  closed-door collision are now recovered.
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
- Gate B remains **PARTIAL**. At that stage the blockers were wall-collision
  placement, spawn/door state, weapon fire/projectile velocity, and full
  death/reborn; wall/door geometry has since been recovered in the room-topology
  slice below.

## 2026-10-31 — Final Gate B / Phase 3 Evidence Sprint

- Resolved the runtime wall source (`RGRoomX.CreateWall` → `MapManagerLevel.wall_list`),
  player spawn (`MapManager.centerPoint`), and door/clear entry points. The later
  room-topology pass corrected the wall layer to 13 (`Wall`), prefabs to
  `wall601`/`wall602`, and recovered the exact gaps; see the final section.
- Resolved `Gun001.Attack → CreateBullet → BulletFactory.TakeBullet`, effective
  speed 36, deviation 5, and `consume=0`; cooldown remains PARTIAL.
- Resolved `EnemyAI04` contact `HitUnit(3,1.5)` and the minimal `e_slime01` contract.
- Resolved death/reborn/game-over/restart entry flow; exact virtual slots/UI remain PARTIAL.
- Resolved the save boundary as Unity `PlayerPrefs`; payload remains UNKNOWN.
- Corrected native `FrameTimer` to the confirmed 50 Hz step and added a regression
  test: **23 cases / 1276 assertions at that time** (now 52/1676 after the animation stage).
- Gate B and Phase 3 remain **PARTIAL**; first-slice approximations are explicitly
  scoped in `docs/GATE_B.md` and `analysis/GATE_B_PHASE_3_FINAL_AUDIT.md`.

## 2026-10-31 — Phase 4 Native Runtime Foundation

- Added `Application` lifecycle, Wayland-only SDL3 window, resumable window-state
  model, measured 50 Hz/60 FPS loop, and idempotent shutdown.
- Added Vulkan 1.3 instance/device/queues/swapchain/render pass/frame resources,
  Debug validation, and compositor-aware resize/recreation.
- Added GLSL sprite shaders, dynamic batched quads, RGBA8 staging texture upload,
  alpha blending, stable layer sorting, and aspect-fit resize.
- Manual 10 s smoke: exit 0, 0 validation messages, 603 presented frames,
  506 fixed ticks, 16.665 ms average `FrameTimer` frame.
- Tests at that stage: **31 cases / 1335 assertions** (now 52/1676 after PNG/animation).
- Gate C is **PARTIAL**; asset decoding, input-to-simulation, audio, UI, and
  gameplay remain unimplemented. See `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`.

## 2026-10-31 — PNG and Controlled Unity Texture2D Gate

- Added libpng-backed PNG decode to canonical RGBA8 `TextureData`, exact-byte
  tests, synthetic fixtures, and native `PngDecoder` round-trip.
- Extended the existing UnityPy extractor with `convert-texture`; pinned
  UnityPy 1.25.4 + Pillow 13.0.0.
- Converted one real reference Texture2D: `level-objects`/`f903`, path ID
  `-8989119030688471403`, RGBA32 16x24, bundle SHA-256
  `0ceed5578212398f4019943c759c3e67fb4338351732d09a2effa45dc5bf8829`.
- Deterministic conversion: RGBA8 SHA-256
  `3b566df519684d6b4d7c9ca5daa6608ab72f9faf073fd2bb713fbae648662acd`,
  PNG SHA-256 `a71dae026afe598a451723a466ccd1eec684405355a157fe9a306e91004f5505`.
- Native `soul-knight-png-roundtrip` matched all 1536 bytes; the real PNG rendered
  through the existing Wayland/Vulkan sprite path.
- Tests: **39 cases / 1439 assertions**.
- Next dependency: Unity Sprite metadata/rect/pivot/PPU; no gameplay started.

## 2026-10-31 — Native Unity Sprite/Scene Composition

- Confirmed Unity Sprite rect/pivot/PPU/border/filter fields and the bottom-left
  texture-rect origin; documented in `analysis/assets/sprites.md`.
- Added native `SpriteData`, `Camera2D`, `SpriteGeometry`, `SceneAsset`, and
  `SceneLoader`; fixed a column-major `Mat4` multiplication bug and an indexed
  multi-texture vertex-offset bug.
- Built the actual `r1_1` visual composition: 109 room sprites from the shared
  256×256 floor atlas plus `c00` body/shadow from the character/object atlases;
  111 instances, 3 textures, 6 sprite assets.
- Scene metadata SHA-256:
  `ba9f3bdca080ddc67045ea64060f3b8353e1027d7b56356cc6d92995de708194`.
  Rendered through Wayland/Vulkan with 0 validation errors.
- Tests: **39 cases / 1439 assertions** at that stage.
- Next dependency: Unity animation frame selection; no gameplay started.

## 2026-10-31 — Native Unity Idle Animation

- Recovered `common/c00` Animator path `-2747643251060265406`, controller
  `char_0_skin_0` path `7636633949607769228`, default state `Base Layer.ide`
  path `580769968`, and clip `char_1_skin_0_ide` path `-6496688600943111911`.
- Clip: 0.5 s, 8 Hz, loop true. `m_ClipBindingConstant` binds
  `SpriteRenderer.m_Sprite` at `img/body`; `pptrCurveMapping` contains
  `c01_4..c01_7`; the streamed curve supplies mapping indices at
  0/.125/.25/.375 s. The high-level `m_PPtrCurves` list is empty and that fact is
  documented rather than hidden.
- Added native `AnimationClip`/`SpriteTrack`/`AnimationPlayer`; kept animation
  separate from `SpriteData`/`TextureData`; integrated fixed-step playback into
  the existing `r1_1` scene.
- Animation metadata SHA-256:
  `23f34e0a6e3b5eb7d11f2e7dcbb11d89c25a9f2b8a17360ff70c280e846f7d44`.
- Tests: **52 cases / 1676 assertions**.
- Next dependency: input-driven locomotion and real `ide`/`run` state switching;
  no combat or AI started.

## 2026-10-31 — Native Keyboard Locomotion Slice

- Recovered and imported the real `run` state/clip (`Base Layer.run`, path
  `4142231225`, `char_1_skin_0_run`, path `3681644519574295089`) alongside idle.
  Both bind `SpriteRenderer.m_Sprite` at `img/body`; run maps `c01_0..c01_3`.
- Added keyboard-only `MovementInput`, fixed-step `PlayerLocomotion`, swept 2D
  AABB collision, `LocomotionAnimator` idle/run selection, and native
  `Camera2D::Follow`.
- c00 movement uses recovered `speed=6.5`, `speed_rate=0.5` with the reconstructed
  `speed*(speed_rate+1)` low-inertial formula; native effective speed is `9.75`
  units/s. Collision bounds are an explicit native 13x13 interior approximation
  from the recovered 15x15 room, not exact `CreateWall` parity.
- `r1_1_visual` is documented as a reverse-engineering test reconstruction, not a
  confirmed canonical starting level.
- Tests: **52 cases / 1676 assertions**.
- Next dependency: complete recovered room wall/door/aisle geometry and traversal;
  no combat or AI started. (Superseded by the room-topology section below.)

## 2026-10-31 — Native Room Topology and Door Traversal

- Decoded the full normal-room `RGRoomX.CreateWall` loop: wall rows at
  `room_y_offset-1` / `room_y_offset+height`, columns at
  `room_x_offset-1` / `room_x_offset+width`, with the five cells `18..22` skipped
  on every side. Runtime offsets are `(41-width)/2 = 13` / `(41-height)/2 = 13`.
- Confirmed the level-1-1 wall prefabs `wall601`/`wall602`: 1×1 non-trigger
  `BoxCollider2D` at offset `(0,0.5)`, tag `Wall`; both variants have identical
  collision geometry. The prefab is serialized on layer 13 `Wall`; generated
  instances are assigned `RGRoomX.EdgeLayer` = 17 `Edge`.
- Recovered `RGAisle.CreateAisle`/`CreateWall`: entrance bits open a door/corridor
  or disable the door and fill the five-cell gap with wall prefabs. Serialized
  `r1_1` entrance is `[0,0,0,0]`; `MapManagerLevel.CreateRoom` assigns real bits
  from `room_relation`.
- Recovered all four serialized door/trigger components, collider/trigger
  rectangles, path IDs, Animator controllers and clips.
- Decoded door open/close streamed clips: open sets `isTrigger=true` and hides
  leaves; closed restores the full solid collider with a one-way inward
  `PlatformEffector2D`. Closed doors block leaving but permit aisle entry.
- Recovered `RGDoorTrigger`/`RGDoor` event payloads: room name plus
  north/south/west/east direction, gated by `door_open` and
  `Body_P`/`Body_P_stealth` tags. Door triggers do not teleport; traversal is
  physical through the aisle. The event-to-map-manager listener remains UNKNOWN.
- Recovered the player physical collider: child `CircleCollider2D` radius 0.4 at
  `(0,0.5)` on layer `Body`; movement uses `Rigidbody2D.velocity` and Unity
  Physics2D. Native swept AABB now consumes recovered topology and models closed
  doors as one-way barriers; the circle is approximated by its enclosing AABB.
- Added `analysis/re/room_topology.md` and
  `analysis/re/r1_1_wall_map.json`/`.txt`; extended the native scene metadata with
  140 wall solids, four doors/triggers/one-way normals, and a labeled 41×41
  map-cell test boundary.
- Tests: **55 cases / 1838 assertions**.
- Next dependency: event-to-map-manager traversal listener/adjacent-room
  coordination; no combat or AI started.
