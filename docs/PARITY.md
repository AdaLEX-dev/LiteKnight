# Soul Knight 2.6.8 — Parity Matrix

> **Correction — 2026-10-31.** Gate B was incorrectly marked ✅. It is **NOT MET**. Gate C is **NOT MET**. Rows in this matrix describe intended comparison points; most "Reference Behaviour" and "Confidence" entries were inferred before behavioural reconstruction and are not evidence. See `analysis/EVIDENCE_AUDIT.md`.

**Status Definitions:**
- `UNKNOWN` — Not yet investigated
- `PARTIAL` — Some behavior implemented, gaps remain
- `FUNCTIONAL` — Works but not verified against reference
- `HIGH-FIDELITY` — Behavior matches reference, minor differences
- `VERIFIED` — Pixel-perfect match, regression tests pass

---

## Parity Matrix

| Subsystem | Reference Behaviour | Native Behaviour | Confidence | Test | Status | Notes |
|-----------|---------------------|------------------|------------|------|--------|-------|
| **Engine** | | | | | | |
| Game Loop | MonoBehaviour (Update/FixedUpdate) | Fixed timestep (60 Hz sim) | HIGH | frame_timing_test | UNKNOWN | |
| RNG | `RGRandom` wraps `UnityEngine.Random.State`; core wrapper and Xorshift128 engine reconstructed | `UnityRandom` / `RGRandomStream` implemented; map-seed virtual path unresolved | HIGH | `unity_random_test` | PARTIAL | `analysis/re/rgrandom.md`, UNK-001 |
| Asset Loading | AssetBundles (LZ4/LZMA) | KTX2/JSON/Ogg pipeline | HIGH | asset_load_test | UNKNOWN | |
| Save/Load | `RGSaveManager` (format?) | JSON + schema version | HIGH | save_roundtrip_test | UNKNOWN | UNK-007 |
| **Player** | | | | | | |
| Movement | `RGController` + `MovementInput` | Vec2 input → velocity/accel | HIGH | player_movement_test | UNKNOWN | |
| Dodge/Roll | `RGDash` coroutine | Invincibility frames + dash | MEDIUM | player_dodge_test | UNKNOWN | |
| Attack | `RGWeapon` fire | Input → projectile spawn | HIGH | player_attack_test | UNKNOWN | |
| Skill/Ability | Character-specific coroutines | Cooldown + effect | MEDIUM | player_skill_test | UNKNOWN | |
| Health/Armor/Energy | Stats in `RGController` | Same fields + regen | HIGH | player_stats_test | UNKNOWN | |
| Status Effects | `RGBuff` on player | Buff component + timers | MEDIUM | player_buff_test | UNKNOWN | UNK-006 |
| **Enemies** | | | | | | |
| AI Base | `RGEController` coroutines | State machine + timers | HIGH | enemy_ai_base_test | UNKNOWN | UNK-004 |
| EnemyAI01-20 | 20 distinct behaviors | Data-driven AI table | HIGH | enemy_ai_##_test | UNKNOWN | |
| Special Enemies | Alien, Witch, Ghost, etc. | Per-type overrides | MEDIUM | enemy_special_test | UNKNOWN | |
| Bosses | `RGEBossController` phases | Phase machine + HP thresholds | HIGH | boss_phase_test | UNKNOWN | UNK-011 |
| Enemy Weapons | `RGEWeapon` + triggers | Mirror player projectiles | MEDIUM | enemy_weapon_test | UNKNOWN | |
| **Weapons** | | | | | | |
| Fire Modes | Auto/semi/charge/burst | Per-weapon config | HIGH | weapon_firemode_test | UNKNOWN | |
| Projectiles | `RGBullet` hierarchy | Instanced projectiles | HIGH | projectile_spawn_test | UNKNOWN | |
| Ammo/Reload | `WeaponItemInfo` ammo | Ammo component + UI | MEDIUM | weapon_ammo_test | UNKNOWN | |
| Upgrades/Mods | `WeaponInBattle` + mods | Level + mod slots | MEDIUM | weapon_upgrade_test | UNKNOWN | UNK-012 |
| **Projectiles** | | | | | | |
| Physics | Straight/homing/bounce/gravity | Per-type physics params | HIGH | projectile_physics_test | UNKNOWN | UNK-005 |
| Triggers | `RGBulletTrigger*` variants | Collision → effect | HIGH | projectile_trigger_test | UNKNOWN | |
| Pierce/Multi-hit | `pierce_count`, `max_hits` | Hit tracking per projectile | MEDIUM | projectile_pierce_test | UNKNOWN | |
| **Combat** | | | | | | |
| Damage Formula | Player/enemy `GetHurt` entry points reconstructed; projectile→target bridge unresolved | Not implemented | PARTIAL | none | UNKNOWN | `analysis/re/damage.md`, UNK-002 |
| Armor/Resist | Armor absorbs first, negative armor overflows to HP; buff 6 blocks overflow | Not implemented | HIGH | none | UNKNOWN | `analysis/re/damage.md` |
| Crit | `DamageInfo.critic` exists; application rule not yet reconstructed | Not implemented | LOW | crit_test | UNKNOWN | `analysis/re/damage.md` |
| Knockback | `knockback` on hit | Velocity impulse | MEDIUM | knockback_test | UNKNOWN | |
| Elements | Fire/Electric/Poison/Ice | Status effect system | MEDIUM | element_test | UNKNOWN | |
| Status Effects | Burn/Shock/Poison/Freeze | DoT + visual + cleanup | MEDIUM | status_effect_test | UNKNOWN | UNK-006 |
| **Rooms / Generation** | | | | | | |
| Room Templates | `RGRoomX` + `SerializeLinker` | JSON room defs + graph | HIGH | room_load_test | UNKNOWN | UNK-003 |
| Map Modes | 8 `MapManager` variants | Shared gen + mode params | HIGH | map_mode_test | UNKNOWN | |
| Doors/Portals | `RGDoor` / `RGTransferGate` | Room transition system | HIGH | room_transition_test | UNKNOWN | |
| Procedural Gen | BSP + graph + placement | Deterministic per seed | HIGH | gen_determinism_test | UNKNOWN | UNK-003 |
| **Items** | | | | | | |
| Pickups | `RGItem` + `ItemObjectProvider` | Collision → inventory | HIGH | item_pickup_test | UNKNOWN | |
| Chests | 7 `RGChest` variants | Loot tables + anim | MEDIUM | chest_test | UNKNOWN | |
| Consumables | Potions, keys | Use → effect + consume | HIGH | consumable_test | UNKNOWN | |
| Buffs | `RGBuff` + triggers | Timed + stackable | MEDIUM | buff_test | UNKNOWN | UNK-006 |
| **Progression** | | | | | | |
| Currency | Coins, gems, crystals | Persistent + save | HIGH | currency_test | UNKNOWN | |
| Unlocks | Characters, weapons, skins | Flags in save | HIGH | unlock_test | UNKNOWN | |
| Experience/Levels | `RGCharacterInfo` XP curve | Formula + rewards | MEDIUM | xp_test | UNKNOWN | |
| Achievements | Statements system | Tracking + UI | LOW | achievement_test | UNKNOWN | |
| **UI** | | | | | | |
| HUD | Health, weapon, ammo, skill | Real-time overlay | HIGH | hud_test | UNKNOWN | |
| Menus | Main, char select, pause, inv | State machine | MEDIUM | menu_test | UNKNOWN | |
| Shop | Buy/sell/refresh | Currency + unlock | LOW | shop_test | UNKNOWN | |
| Settings | Video/audio/input | Persists to TOML | HIGH | settings_test | UNKNOWN | |
| **Audio** | | | | | | |
| Music | Per-biome/boss streaming | Crossfade + layers | HIGH | music_test | UNKNOWN | |
| SFX | Per-event (weapon, hit, etc.) | Pooled + priority | HIGH | sfx_test | UNKNOWN | |
| Ambience | Room/biome loops | Fade on transition | MEDIUM | ambience_test | UNKNOWN | |
| **Multiplayer** | | | | | | |
| All MP systems | `RGNetWorkManager` + modes | STRIPPED | N/A | N/A | N/A | Out of scope v1 |

---

## Parity Gates

| Gate | Requirement | Status |
|------|-------------|--------|
| Gate B (Understanding) | All subsystems mapped to UNKNOWN or better | ❌ **NOT MET** — class inventory only; behaviour unresolved |
| Gate C (Runtime) | Native app launches, renders, inputs | ❌ **NOT MET** — no renderer/simulation; build must be restored first |
| Gate D (First Playable) | Core loop playable, all FUNCTIONAL | ⬜ Not started |
| Gate E (Content) | All discovered content PARTIAL+ | ⬜ Not started |
| Gate F (Release) | All VERIFIED, tests pass | ⬜ Not started |

---

## Tracking

- Update this matrix after each subsystem implementation
- Link to test files in `tests/`
- Reference evidence in `analysis/re/`
- Status changes require evidence commit
- A field name or class name is not evidence for a mechanic

**Last Updated:** 2026-10-31 (gates corrected; behavioural entries marked unverified)
**Next Review:** After Phase 4 (Gate C)