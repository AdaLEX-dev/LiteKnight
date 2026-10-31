// Soul Knight Linux — native versioned save data model
//
// The reference RGSaveManager format is UNKNOWN (Odin/Sirenix serialized).
// This is a native LiteKnight JSON schema, deliberately versioned. It is not a
// reconstruction of the original save file.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace soulknight::game::save {

struct SaveData {
    static constexpr int kCurrentVersion = 2;

    int save_version = kCurrentVersion;
    std::string game_version;
    std::string scene_id;
    std::string character_id;
    std::array<std::string, 2> weapon_slots{};
    int active_slot = 0;
    float position_x = 0.0f;
    float position_y = 0.0f;
    int hp = 0;
    int max_hp = 0;
    int armor = 0;
    int max_armor = 0;
    int energy = 0;
    int max_energy = 0;
    bool dead = false;
    std::int64_t playtime_seconds = 0;
    std::vector<std::string> unlocked_characters;
    std::vector<std::string> plants;
    bool fullscreen = false;
    bool hub_visited = false;
    bool auto_aim = false;
    std::int32_t map_seed = 0;
};

} // namespace soulknight::game::save
