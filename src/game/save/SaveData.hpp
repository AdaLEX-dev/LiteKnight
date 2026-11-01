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

struct SaveChestState {
    std::string definition;
    float x = 0.0f;
    float y = 0.0f;
    int state = 0;
    float timer = 0.0f;
    bool loot_spawned = false;
    std::string pending_weapon;
    std::vector<std::string> pending_weapons;
};

struct SaveCoinState {
    std::string definition;
    float x = 0.0f;
    float y = 0.0f;
    bool active = true;
};

struct SaveMaterial {
    std::string id;
    int count = 0;
};

struct SaveRoomItems {
    std::string id;
    std::vector<SaveChestState> chests;
    std::vector<SaveCoinState> coins;
};

struct SaveData {
    static constexpr int kCurrentVersion = 5;

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
    int coins = 0;
    int orbs = 0;
    bool dead = false;
    std::int64_t playtime_seconds = 0;
    std::vector<std::string> unlocked_characters;
    std::vector<std::string> plants;
    std::vector<std::string> plant_states;
    std::vector<std::string> seed_stock;
    bool fullscreen = false;
    bool hub_visited = false;
    bool auto_aim = true;
    bool reduced_weapon_effects = true;
    bool living_room_suggestions = false;
    std::int32_t map_seed = 0;
    std::string campaign_level = "1-1";
    std::int64_t trader_stock_seed = 0;
    std::vector<int> sold_trader_offers;
    std::string pet_id;
    std::vector<SaveMaterial> materials;
    std::vector<std::string> unlocked_blueprints;
    std::vector<std::string> cleared_rooms;
    std::vector<std::string> chest_spawned_rooms;
    std::vector<std::string> sold_shop_items;
    std::string active_statue;
    std::string active_mercenary;
    int mercenary_level = 0;
    std::string active_mount;
    std::array<bool, 2> pot_empty{};
    std::vector<SaveRoomItems> rooms;
};

} // namespace soulknight::game::save
