# Soul Knight 2.6.8 — Save System Design

> **Correction — 2026-10-31.** This document is a **native design proposal**, not a reconstruction of the reference save format. Metadata shows `RGSaveManager` with `char_list`, `pet_list`, `hero_skin_list`, `AboGameData game_data`, and `store_key`; the assembly references Sirenix/Odin serialization. The reference format is **UNKNOWN** until `RGSaveManager.Save/Load` is disassembled. The JSON schema and version history below were invented for the native design and must not be cited as reference behaviour.

**Target:** JSON + schema versioning, XDG compliance, fast save/load

> **Implementation status — 2026-10-31.** A native version-1 JSON save exists in
> `src/game/save/`: `SaveData`, `SaveSerializer`, `SaveManager`. It writes
> `$XDG_DATA_HOME/liteknight/saves/slot_0.json` atomically, keeps
> `slot_0.json.bak`, falls back to the backup on corruption, and rejects future
> versions. Unit tests cover round-trip, missing/corrupt files, migration,
> invalid ranges, atomic replacement, backup recovery and deletion. This remains
> a **native design**, not decoded RGSaveManager parity. The larger schema below
> is a design proposal; only the implemented subset is active.

---

## 1. Save Structure

### File Layout
```
$XDG_DATA_HOME/soul-knight/
├── saves/
│   ├── slot_0.json          # Auto-save
│   ├── slot_1.json          # Manual save 1
│   ├── slot_2.json          # Manual save 2
│   ├── slot_3.json          # Manual save 3
│   └── slot_4.json          # Manual save 4
├── settings/
│   └── config.toml          # User preferences (video, audio, controls)
├── assets/                  # Converted native assets (copied/linked at install)
│   ├── textures/
│   ├── audio/
│   └── data/
└── logs/
    └── latest.log
```

### Save Slot JSON Schema
```json
{
  "save_version": 3,
  "game_version": "2.6.8",
  "timestamp": 1701234567,
  "playtime_seconds": 3600,
  "slot": 0,

  "run": {
    "seed": 1234567890,
    "mode": "normal",           // "normal", "endless", "defence", "race", "sandbox", "boss_rush"
    "difficulty": "normal",     // "easy", "normal", "hard", "very_hard"
    "current_floor": 1,
    "current_room": "room_001",
    "room_seed": 987654321,
    "rng_state": {
      "world": "hex64...",
      "combat": "hex64...",
      "ai": "hex64...",
      "loot": "hex64...",
      "vfx": "hex64..."
    }
  },

  "player": {
    "character_id": "knight",
    "skin_id": "default",
    "level": 5,
    "experience": 1250,
    "stats": {
      "health": 100,
      "max_health": 120,
      "armor": 15,
      "energy": 80,
      "max_energy": 100,
      "attack": 12,
      "crit_chance": 0.05,
      "crit_multiplier": 1.5,
      "move_speed": 5.0,
      "dodge_cooldown": 1.5
    },
    "inventory": {
      "weapon_slots": [
        {"weapon_id": "pistol_basic", "ammo": 30, "level": 1, "mods": []},
        {"weapon_id": "sword_basic", "ammo": 0, "level": 2, "mods": ["fire"]},
        null
      ],
      "active_weapon": 0,
      "items": [
        {"item_id": "health_potion", "count": 3},
        {"item_id": "energy_potion", "count": 1},
        {"item_id": "key_red", "count": 1}
      ],
      "buffs": [
        {"buff_id": "speed_boost", "remaining_time": 15.0, "stacks": 1}
      ]
    },
    "skills": {
      "active_skill": "dash",
      "passive_skills": ["ammo_efficiency", "crit_damage"],
      "skill_cooldowns": {"dash": 0.0}
    }
  },

  "progression": {
    "currency": {
      "coins": 5000,
      "gems": 50,
      "energy_crystals": 10
    },
    "unlocked_characters": ["knight", "rogue", "mage"],
    "unlocked_weapons": ["pistol_basic", "sword_basic", "shotgun_basic"],
    "unlocked_skins": {"knight": ["default", "gold"], "rogue": ["default"]},
    "achievements": [
      {"id": "first_kill", "unlocked": true, "timestamp": 1701234000},
      {"id": "complete_floor_1", "unlocked": true, "timestamp": 1701234200}
    ],
    "statistics": {
      "total_kills": 1523,
      "total_deaths": 12,
      "total_playtime": 3600,
      "bosses_defeated": ["boss01", "boss02"],
      "rooms_cleared": 456,
      "chests_opened": 89,
      "distance_traveled": 125000.0
    }
  },

  "world": {
    "cleared_rooms": ["room_001", "room_002", "room_005"],
    "activated_portals": ["portal_01", "portal_02"],
    "defeated_bosses": ["boss01"],
    "collected_secrets": ["secret_001", "secret_003"]
  }
}
```

---

## 2. Versioning & Migration

### Save Version History
| Version | Game Version | Changes |
|---------|--------------|---------|
| 1 | 2.6.0 | Initial |
| 2 | 2.6.5 | Added `rng_state` streams, `progression.currency.gems` |
| 3 | 2.6.8 | Added `run.mode`, `player.skills`, `world.activated_portals` |

### Migration Functions
```cpp
// src/game/save/SaveMigrations.cpp
namespace SaveMigrations {

bool Migrate1to2(nlohmann::json& save) {
    // Add RNG streams if missing
    if (!save["run"].contains("rng_state")) {
        save["run"]["rng_state"] = {
            {"world", "0"},
            {"combat", "0"},
            {"ai", "0"},
            {"loot", "0"},
            {"vfx", "0"}
        };
    }
    // Add gems currency
    if (!save["progression"]["currency"].contains("gems")) {
        save["progression"]["currency"]["gems"] = 0;
    }
    save["save_version"] = 2;
    return true;
}

bool Migrate2to3(nlohmann::json& save) {
    // Add run mode
    if (!save["run"].contains("mode")) {
        save["run"]["mode"] = "normal";
    }
    // Add skills
    if (!save["player"].contains("skills")) {
        save["player"]["skills"] = {
            {"active_skill", "dash"},
            {"passive_skills", nlohmann::json::array()},
            {"skill_cooldowns", {}}
        };
    }
    // Add world portals
    if (!save["world"].contains("activated_portals")) {
        save["world"]["activated_portals"] = nlohmann::json::array();
    }
    save["save_version"] = 3;
    return true;
}

using MigrationFn = bool(*)(nlohmann::json&);
const std::array<MigrationFn, 3> MIGRATIONS = {nullptr, Migrate1to2, Migrate2to3};

bool MigrateToCurrent(nlohmann::json& save) {
    int from = save.value("save_version", 1);
    int to = CURRENT_SAVE_VERSION;  // 3

    for (int v = from; v < to; v++) {
        if (!MIGRATIONS[v](save)) {
            LOG_ERROR("Save migration {}→{} failed", v, v+1);
            return false;
        }
    }
    return true;
}

} // namespace SaveMigrations
```

---

## 3. Save Manager

```cpp
// src/game/save/SaveManager.h
class SaveManager {
public:
    static constexpr int MAX_SLOTS = 5;
    static constexpr int CURRENT_VERSION = 3;

    struct SaveMetadata {
        int slot = 0;
        int version = 0;
        std::string game_version;
        int64_t timestamp = 0;
        int playtime_seconds = 0;
        std::string character_name;
        int floor = 1;
        bool valid = false;
    };

    // Initialize (create directories)
    bool Initialize(const FileSystem& fs);

    // List all saves
    std::vector<SaveMetadata> ListSaves() const;

    // Save operations
    bool SaveGame(int slot, const GameState& state);
    bool SaveGame(int slot, const nlohmann::json& save_data);

    // Load operations
    std::optional<nlohmann::json> LoadGame(int slot);
    std::optional<GameState> LoadGameState(int slot);

    // Delete
    bool DeleteSave(int slot);

    // Auto-save
    void EnableAutoSave(bool enable, float interval_seconds = 30.0f);
    void TriggerAutoSave(const GameState& state);

    // Validation
    bool ValidateSave(const nlohmann::json& save) const;
    std::string GetSaveHash(const nlohmann::json& save) const;  // SHA-256 for integrity

private:
    FileSystem fs_;
    std::string saves_dir_;
    bool auto_save_enabled_ = true;
    float auto_save_timer_ = 0.0f;
    float auto_save_interval_ = 30.0f;

    std::string GetSlotPath(int slot) const;
    nlohmann::json CreateSaveData(const GameState& state);
    bool WriteSaveFile(const std::string& path, const nlohmann::json& data);
    std::optional<nlohmann::json> ReadSaveFile(const std::string& path);
};
```

---

## 4. Game State Serialization

```cpp
// src/game/save/GameStateSerializer.h
class GameStateSerializer {
public:
    // Serialize entire game state to JSON
    static nlohmann::json Serialize(const GameState& state);

    // Deserialize JSON to game state
    static std::optional<GameState> Deserialize(const nlohmann::json& json);

private:
    // Sub-serializers
    static nlohmann::json SerializePlayer(const Player& player);
    static nlohmann::json SerializeInventory(const Inventory& inv);
    static nlohmann::json SerializeProgression(const Progression& prog);
    static nlohmann::json SerializeWorld(const WorldState& world);
    static nlohmann::json SerializeRun(const RunState& run);
    static nlohmann::json SerializeRNG(const RNGState& rng);

    static std::optional<Player> DeserializePlayer(const nlohmann::json& json);
    static std::optional<Inventory> DeserializeInventory(const nlohmann::json& json);
    static std::optional<Progression> DeserializeProgression(const nlohmann::json& json);
    static std::optional<WorldState> DeserializeWorld(const nlohmann::json& json);
    static std::optional<RunState> DeserializeRun(const nlohmann::json& json);
    static std::optional<RNGState> DeserializeRNG(const nlohmann::json& json);
};
```

### RNG State Serialization (Critical for Determinism)
```cpp
struct RNGState {
    // Reference-compatible RNG state per stream (algorithm pending reconstruction)
    struct Stream {
        uint64_t state = 0;
        uint64_t inc = 0;
    };

    Stream world;      // Dungeon generation
    Stream combat;     // Damage, crit, procs
    Stream ai;         // Enemy behavior
    Stream loot;       // Drop tables
    Stream vfx;        // Particle variance

    nlohmann::json ToJson() const {
        return {
            {"world", StreamToHex(world)},
            {"combat", StreamToHex(combat)},
            {"ai", StreamToHex(ai)},
            {"loot", StreamToHex(loot)},
            {"vfx", StreamToHex(vfx)}
        };
    }

    static std::optional<RNGState> FromJson(const nlohmann::json& json) {
        RNGState state;
        if (!HexToStream(json.value("world", "0"), state.world)) return {};
        if (!HexToStream(json.value("combat", "0"), state.combat)) return {};
        if (!HexToStream(json.value("ai", "0"), state.ai)) return {};
        if (!HexToStream(json.value("loot", "0"), state.loot)) return {};
        if (!HexToStream(json.value("vfx", "0"), state.vfx)) return {};
        return state;
    }
};
```

---

## 5. Auto-Save Logic

```cpp
// src/game/save/AutoSave.cpp
class AutoSaveSystem {
public:
    void Update(float dt, const GameState& state) {
        if (!enabled_) return;

        timer_ += dt;
        if (timer_ >= interval_) {
            timer_ = 0.0f;
            save_manager_.SaveGame(0, state);  // Slot 0 = auto
        }
    }

    // Also trigger on:
    // - Room clear
    // - Level up
    // - Item pickup (rare)
    // - Boss defeat
    // - Portal activation
    void OnRoomCleared(const GameState& state) { save_manager_.SaveGame(0, state); }
    void OnLevelUp(const GameState& state) { save_manager_.SaveGame(0, state); }
    void OnBossDefeated(const GameState& state) { save_manager_.SaveGame(0, state); }

private:
    SaveManager& save_manager_;
    bool enabled_ = true;
    float timer_ = 0.0f;
    float interval_ = 30.0f;  // seconds
};
```

---

## 6. Settings (Separate from Saves)

```toml
# $XDG_CONFIG_HOME/soul-knight/config.toml
[video]
width = 1920
height = 1080
fullscreen = false
vsync = true
max_fps = 60
texture_quality = "high"     # "low", "medium", "high"
effects_quality = "high"
show_fps = false

[audio]
master = 1.0
music = 0.8
sfx = 1.0
ui = 0.7
ambience = 0.6

[input]
# Key bindings (see INPUT.md)
[keyboard]
MoveUp = ["W", "Up"]
...

[gamepad]
...

[gameplay]
language = "en"
auto_attack = false
show_damage_numbers = true
screen_shake = true
camera_shake = true

[accessibility]
colorblind_mode = "none"    # "none", "protanopia", "deuteranopia", "tritanopia"
high_contrast = false
large_text = false
```

---

## 7. Integrity & Corruption Handling

```cpp
// Save file structure with integrity
struct SaveFile {
    std::string magic = "SOUL_KNIGHT_SAVE";  // 16 bytes
    uint32_t version = 3;
    uint32_t crc32 = 0;         // CRC32 of JSON payload
    uint64_t timestamp = 0;
    std::string json_payload;   // Minified JSON

    bool Verify() const {
        return crc32 == ComputeCRC32(json_payload);
    }

    static SaveFile Pack(const nlohmann::json& json) {
        SaveFile file;
        file.json_payload = json.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        file.crc32 = ComputeCRC32(file.json_payload);
        file.timestamp = GetTimeUnix();
        return file;
    }

    static std::optional<nlohmann::json> Unpack(const SaveFile& file) {
        if (!file.Verify()) return {};
        try {
            return nlohmann::json::parse(file.json_payload);
        } catch (...) {
            return {};
        }
    }
};
```

### Corruption Recovery
```cpp
std::optional<nlohmann::json> SaveManager::LoadGame(int slot) {
    auto path = GetSlotPath(slot);
    auto file_data = fs_.ReadFile(path);
    if (!file_data) return {};

    // Try to parse as SaveFile (with CRC)
    SaveFile save_file;
    if (DeserializeSaveFile(*file_data, save_file)) {
        if (save_file.Verify()) {
            auto json = SaveFile::Unpack(save_file);
            if (json && SaveMigrations::MigrateToCurrent(*json)) {
                return json;
            }
        }
    }

    // Fallback: try raw JSON (legacy/corrupted CRC)
    LOG_WARN("Save slot {} CRC failed, attempting raw JSON parse", slot);
    try {
        auto json = nlohmann::json::parse(std::string(file_data->begin(), file_data->end()));
        if (SaveMigrations::MigrateToCurrent(json)) {
            LOG_INFO("Recovered save slot {} from raw JSON", slot);
            return json;
        }
    } catch (...) {}

    LOG_ERROR("Save slot {} corrupted beyond recovery", slot);
    return {};
}
```

---

## 8. Compression (Optional)

```cpp
// For large saves (> 100KB), compress with zstd
std::vector<uint8_t> CompressSave(const nlohmann::json& json) {
    std::string json_str = json.dump();
    size_t compressed_size = ZSTD_compressBound(json_str.size());
    std::vector<uint8_t> compressed(compressed_size);

    size_t actual_size = ZSTD_compress(
        compressed.data(), compressed_size,
        json_str.data(), json_str.size(),
        3  // compression level
    );
    compressed.resize(actual_size);
    return compressed;
}

std::optional<nlohmann::json> DecompressSave(const std::vector<uint8_t>& data) {
    size_t original_size = ZSTD_getFrameContentSize(data.data(), data.size());
    if (original_size == ZSTD_CONTENTSIZE_ERROR || original_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        return {};
    }

    std::vector<char> decompressed(original_size);
    size_t result = ZSTD_decompress(decompressed.data(), original_size, data.data(), data.size());
    if (ZSTD_isError(result)) return {};

    try {
        return nlohmann::json::parse(std::string(decompressed.data(), result));
    } catch (...) {
        return {};
    }
}
```

---

## 9. Testing

```cpp
TEST("Save/Load roundtrip preserves state") {
    GameState original = CreateTestGameState();
    original.run.seed = 42;
    original.player.stats.health = 73;
    original.progression.currency.coins = 12345;

    // Save
    nlohmann::json json = GameStateSerializer::Serialize(original);
    SaveManager mgr;
    mgr.Initialize(fs);
    CHECK(mgr.SaveGame(1, json));

    // Load
    auto loaded_json = mgr.LoadGame(1);
    REQUIRE(loaded_json);

    auto loaded_state = GameStateSerializer::Deserialize(*loaded_json);
    REQUIRE(loaded_state);

    // Verify critical fields
    CHECK(loaded_state->run.seed == original.run.seed);
    CHECK(loaded_state->player.stats.health == original.player.stats.health);
    CHECK(loaded_state->progression.currency.coins == original.progression.currency.coins);
    CHECK(loaded_state->rng_state.world.state == original.rng_state.world.state);
}

TEST("Migration v1→v3 works") {
    nlohmann::json v1_save = R"({
        "save_version": 1,
        "run": {"seed": 123},
        "player": {"character_id": "knight"},
        "progression": {"currency": {"coins": 100}}
    })"_json;

    CHECK(SaveMigrations::MigrateToCurrent(v1_save));
    CHECK(v1_save["save_version"] == 3);
    CHECK(v1_save["run"]["rng_state"].contains("world"));
    CHECK(v1_save["progression"]["currency"].contains("gems"));
}

TEST("Corrupted save rejected") {
    SaveManager mgr;
    mgr.Initialize(fs);

    // Write garbage
    fs.WriteFile("saves/slot_1.json", std::vector<uint8_t>{'b','a','d',' ','d','a','t','a'});

    auto result = mgr.LoadGame(1);
    CHECK(!result);
}

TEST("RNG state preserved exactly") {
    RNGState original;
    original.world.state = 0x123456789ABCDEF0;
    original.world.inc = 0xFEDCBA9876543210;
    // ... other streams

    auto json = original.ToJson();
    auto restored = RNGState::FromJson(json);

    REQUIRE(restored);
    CHECK(restored->world.state == original.world.state);
    CHECK(restored->world.inc == original.world.inc);
}
```

---

## 10. Platform Integration

### FileSystem Interface
```cpp
class FileSystem {
public:
    std::optional<std::vector<uint8_t>> ReadFile(const std::string& path);
    bool WriteFile(const std::string& path, const std::vector<uint8_t>& data);
    bool WriteFile(const std::string& path, const std::string& text);
    bool DeleteFile(const std::string& path);
    bool Exists(const std::string& path);
    std::vector<std::string> ListDirectory(const std::string& path);
    int64_t GetModifiedTime(const std::string& path);
    int64_t GetFileSize(const std::string& path);
};
```

---

**Status:** DESIGN PROPOSAL ONLY — not reference evidence. Implementation is blocked by the evidence gates; this design is not a completion claim.