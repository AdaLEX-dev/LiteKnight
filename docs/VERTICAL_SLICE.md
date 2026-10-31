# Vertical Slice — Evidence-Gated Plan

**Status:** **BLOCKED** — planning only. No gameplay implementation until the gates
below are met or explicitly accepted.
**Target:** native CachyOS x86-64, Wayland, Vulkan, 1920×1080.
**Reference:** Soul Knight 2.6.8, Unity 2018.4.12f1, metadata v24.
**Gate B report:** `docs/GATE_B.md` — **PARTIAL**.
**Phase 3 report:** `docs/PHASE_3_BEHAVIOURAL_RE.md` — **PARTIAL**.

---

## 1. Purpose

Prove the native architecture and evidence pipeline with the smallest genuinely
playable loop, not a broad speculative port.

**Technical slice content:**

- room `r1_1`,
- player prefab `c00` (`C01Controller`; display name CANDIDATE),
- enemy prefab `e_slime01` (`EnemyAI04`; display name/AI partially unresolved),
- weapon `weapon_000` (`Gun001`; display name CANDIDATE),
- projectile `bullet_0` (`Bullet01`),
- movement, collision, attack, projectile, damage, enemy death, player death,
- restart,
- basic HUD,
- extracted sprites/audio,
- native Wayland/Vulkan window and renderer.

**Non-goals:** full content, full UI, all characters/weapons/enemies, online,
progression, final balance, packaging.

---

## 2. Evidence gates

A file existing is not a pass. Each gate needs an evidence document, exact missing
evidence, and a responsible subsystem.

| Gate | Status | Evidence document | Exact missing evidence | Responsible subsystem |
|---|---|---|---|---|
| Player identity | ⚠️ PARTIAL | `analysis/re/rooms_scene.md` | `c00`/`C01Controller`/stats confirmed; display name "Knight" not proven | Character data / `c00` |
| Enemy identity | ⚠️ PARTIAL | `analysis/re/rooms_scene.md` | `e_slime01`/`EnemyAI04`/stats confirmed; display name and AI state machine unknown | Enemy data / `RGEController` |
| Weapon identity | ⚠️ PARTIAL | `analysis/re/weapons_projectiles.md` | `weapon_000`/`Gun001`/`bullet_0` confirmed; display name/starting assignment not proven | Weapon data / `RGWeapon` |
| Input semantics | ⚠️ PARTIAL | `analysis/re/input_movement.md` | WASD/joystick + movement confirmed; exact attack/skill/dodge button semantics partial | Input / `RGJoyControl` |
| Room hierarchy | ⚠️ PARTIAL | `analysis/re/rooms_scene.md` | `r1_1` hierarchy confirmed; player spawn is `MapManager.centerPoint`; full door/aisle state transitions PARTIAL | Rooms / `RGRoomX` |
| Room collision | ⚠️ PARTIAL | `analysis/re/rooms_scene.md` | Runtime wall source confirmed (`CreateWall`→`wall_list`, layer Edge, 1×1 colliders); exact per-cell gaps PARTIAL | Rooms / `RGRoomX`, `MapManagerLevel` |
| Enemy AI | ⚠️ PARTIAL | `analysis/re/enemy_ai04.md` | `EnemyAI04` contact `HitUnit(3,1.5)` and base Scout/target path known; full state machine/animations PARTIAL | AI / `RGEController` |
| Weapon behaviour | ⚠️ PARTIAL | `analysis/re/weapons_projectiles.md` | `Gun001.Attack`→`CreateBullet`→`BulletFactory.TakeBullet`; speed 36, deviation 5, consume 0; cooldown PARTIAL | Weapons / `RGWeapon` |
| Projectile behaviour | ⚠️ PARTIAL | `analysis/re/weapons_projectiles.md` | `Bullet01` fields/trigger chain; BulletInfo speed 36 overrides prefab 10; rebound/through PARTIAL | Projectiles / `RGBullet` |
| Damage bridge | ✅ MET | `analysis/re/damage.md` | Player `+0x228`, enemy `+0x2C8`, box `+0x168` resolved | Combat / `DamageCarrier` |
| Damage formulas | ⚠️ PARTIAL | `analysis/re/damage.md` | Armor/HP/CanDie confirmed; crit roll location confirmed; effect/element details PARTIAL | Combat / `GetHurt` |
| Death/restart | ⚠️ PARTIAL | `analysis/re/damage.md` | Death/Reborn/DoReborn/GameFail/TryAgain entry flows confirmed; exact slots/UI sequencing PARTIAL | Combat/state |
| Tick rate | ✅ MET | `analysis/re/timing.md` | `TimeManager.Fixed_Timestep = 0.02` (50 Hz) confirmed | Engine loop / `FrameTimer` |
| Renderer | ⚠️ PARTIAL | `analysis/PHASE_4_RUNTIME_FOUNDATION_AUDIT.md`, `docs/RENDERING.md` | Vulkan 1.3 + multi-texture sprite batch + Camera2D; actual r1_1+c00 composition verified; animation/atlas breadth not implemented | Rendering / platform |
| RNG core | ⚠️ PARTIAL | `analysis/re/rgrandom.md` | Core/map/LongMap confirmed; `seed_state` 1/2, DateTime seed, reference vectors unavailable | RNG / `RGRandom` |
| Asset extraction | ⚠️ PARTIAL | `analysis/assets/extraction_manifest.json`, `analysis/assets/texture_conversion_manifest.json`, `docs/ASSET_PIPELINE.md` | Extraction reproducible; one real Unity Texture2D converted deterministically and native-round-tripped; atlas/sprite metadata/audio not started | Assets / extraction tool |
| Save/runtime boundary | ⚠️ PARTIAL | `analysis/re/save.md` | Unity PlayerPrefs string/int keys confirmed; payload/versioning UNKNOWN; not required for slice | Save / `RGSaveManager` |
| Build/test baseline | ✅ MET (tooling) | `analysis/REPOSITORY_INTEGRITY_AUDIT.md` | Not a gameplay gate | Build / tests |

**Slice gate:** **NOT CLEARED.** Exact wall/door gaps, weapon cooldown, full
reborn UI, and full AI remain PARTIAL. The first slice may use explicitly labelled
approximations for a rectangular 15×15 room with serialized door openings, a fixed
attack interval, death+restart, and minimal `EnemyAI04` contact behavior. Renderer
is a Phase 4 implementation gate, not an evidence gate.

---

## 3. First-pass assets

Generated by `tools/asset/extract.sh extract --profile first-pass`:

- `room-r1-1`: 102 shared floor/wall/door sprite entries from `res/floor`; the
  `room/r1_1` bundle itself is prefab references, not an atlas.
- `character-c01-candidate`: 133 entries (sprites + hierarchy; candidate). The
  prefab/controller evidence for `c00`/`c01` is now confirmed separately.
- `enemy-enemy01-slime-candidate`: 7 sprites (candidate).
- `enemy-e-slime01`: 1 hierarchy JSON (`e_slime01`/`EnemyAI04` confirmed technical).
- `weapon-candidate`: 5 entries (sprite sample + generic prefab hierarchies);
  `weapon_000`/`bullet_0` confirmed separately.
- `ui-sprites`: 30 representative UI sprites.
- `audio-sample`: 6 clips.

Display names remain candidate until a serialized name mapping is found.

---

## 4. Implementation order (after gates)

1. **Renderer smoke test** — ✅ DONE (Wayland window, Vulkan, present, resize,
   clean exit).
2. **Sprite path** — ⚠️ PARTIAL (real Unity Texture2D/Sprite metadata, shared
   atlases, Camera2D, and r1_1+c00 composition verified; animation/atlas breadth
   pending).
3. **Simulation clock** — ⚠️ PARTIAL (50 Hz fixed loop + measured 60 FPS render
   pacing; gameplay systems and input snapshots pending).
4. **Input** — WASD/joystick movement; verify attack mapping.
5. **Room load** — parse `r1_1` hierarchy/bounds, place sprites/colliders.
6. **Collision** — wall/door/bullet trigger representation.
7. **Player entity** — c00/C01Controller stats, movement, damage.
8. **Weapon/projectile** — weapon_000/Gun001 → bullet_0/Bullet01.
9. **Enemy** — e_slime01/EnemyAI04 minimal documented behavior.
10. **Damage/death/restart** — reconstructed pipeline.
11. **HUD** — HP/armor/energy from state.
12. **Audio** — one BGM and SFX.
13. **Integration** — complete `r1_1` loop.
14. **Reference comparison/performance** — only where evidence supports.

---

## 5. Tests for the slice

- simulation tick independent of render rate,
- room loads from fixed converted data,
- player wall collision and door trigger,
- weapon creates bullet_0 with documented damage/speed,
- projectile lifetime/hit/despawn,
- armor absorbs then overflows to HP,
- enemy HP reaches zero → death,
- player `CanDie` leaves 1 HP where applicable,
- restart resets state without stale entities,
- Vulkan frame presents/resizes/exits cleanly,
- headless test path does not require a display.

---

## 6. Open decisions

- Exact display identity of c00/c01/e_slime01/weapon_000 (technical identities are
  confirmed; names are candidates).
- Wall-collider placement and room spawn point.
- Exact weapon fire cadence/ammo/spread and projectile velocity.
- Full enemy AI behavior for e_slime01.
- Death/reborn/restart branches.
- Renderer material/shader parity.
- Save format (not required for first slice; runtime state can be transient).

---

**Rule:** the vertical slice is a validation vehicle, not a place to hide missing
evidence. If a system is implemented on a hypothesis, label it in code and in the
parity matrix.
