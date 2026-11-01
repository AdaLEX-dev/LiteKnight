#include "SaveSerializer.hpp"

#include <cmath>

namespace soulknight::game::save {
namespace {

bool ReadString(const Json& object, const char* key, std::string* out,
                std::string* error, bool required = true) {
    if (!object.contains(key)) {
        if (required && error != nullptr) {
            *error = std::string("save is missing string field: ") + key;
        }
        return !required;
    }
    if (!object[key].is_string()) {
        if (error != nullptr) {
            *error = std::string("save field is not a string: ") + key;
        }
        return false;
    }
    *out = object[key].get<std::string>();
    return true;
}

bool ReadInt(const Json& object, const char* key, int* out,
             std::string* error, bool required = true) {
    if (!object.contains(key)) {
        if (required && error != nullptr) {
            *error = std::string("save is missing integer field: ") + key;
        }
        return !required;
    }
    if (!object[key].is_number_integer()) {
        if (error != nullptr) {
            *error = std::string("save field is not an integer: ") + key;
        }
        return false;
    }
    *out = object[key].get<int>();
    return true;
}

bool ReadBool(const Json& object, const char* key, bool* out,
              std::string* error) {
    if (!object.contains(key) || !object[key].is_boolean()) {
        if (error != nullptr) {
            *error = std::string("save field is not boolean: ") + key;
        }
        return false;
    }
    *out = object[key].get<bool>();
    return true;
}

bool MigrateLegacyV0(Json& document, std::string* error) {
    // Native pre-versioned development layout. This is our own old draft, not
    // a decoded reference format.
    if (!document.is_object()) {
        if (error != nullptr) *error = "legacy save root is not an object";
        return false;
    }
    Json migrated = Json::object();
    migrated["save_version"] = 1;
    migrated["game_version"] = document.value("game_version", std::string{});
    migrated["scene_id"] = document.value("scene", std::string{"r1_1_visual"});
    migrated["playtime_seconds"] = document.value("playtime_seconds", 0);

    Json player = Json::object();
    player["character_id"] = document.value("character", std::string{"c00"});
    player["position"] = Json::array({document.value("x", 0.0f),
                                      document.value("y", 0.0f)});
    player["hp"] = document.value("hp", 6);
    player["max_hp"] = document.value("max_hp", 6);
    player["armor"] = document.value("armor", 5);
    player["max_armor"] = document.value("max_armor", 5);
    player["energy"] = document.value("energy", 180);
    player["max_energy"] = document.value("max_energy", 180);
    player["dead"] = document.value("dead", false);
    migrated["player"] = std::move(player);

    Json inventory = Json::object();
    if (document.contains("weapons") && document["weapons"].is_array()) {
        Json slots = Json::array();
        for (const Json& item : document["weapons"]) {
            if (item.is_string()) slots.push_back(item);
            if (slots.size() == 2) break;
        }
        while (slots.size() < 2) slots.push_back("");
        inventory["weapon_slots"] = std::move(slots);
    } else {
        inventory["weapon_slots"] = Json::array({"", ""});
    }
    inventory["active_slot"] = document.value("active_weapon", 0);
    migrated["inventory"] = std::move(inventory);
    document = std::move(migrated);
    return true;
}

} // namespace

bool MigrateToCurrent(Json& document, std::string* error) {
    if (error != nullptr) error->clear();
    if (!document.is_object()) {
        if (error != nullptr) *error = "save root is not an object";
        return false;
    }
    int version = 0;
    if (document.contains("save_version")) {
        if (!document["save_version"].is_number_integer()) {
            if (error != nullptr) *error = "save_version is not an integer";
            return false;
        }
        version = document["save_version"].get<int>();
    }
    if (version > SaveData::kCurrentVersion) {
        if (error != nullptr) {
            *error = "save version " + std::to_string(version) +
                     " is newer than supported version " +
                     std::to_string(SaveData::kCurrentVersion);
        }
        return false;
    }
    while (version < SaveData::kCurrentVersion) {
        if (version == 0) {
            if (!MigrateLegacyV0(document, error)) return false;
        } else if (version == 1) {
            // v1 -> v2: progression lists/settings are additive defaults.
            document["unlocked_characters"] = Json::array();
            document["plants"] = Json::array();
            document["fullscreen"] = false;
            document["hub_visited"] = false;
            document["auto_aim"] = true;
            document["map_seed"] = 0;
            if (document.contains("player") && document["player"].is_object() &&
                !document["player"].contains("coins")) {
                document["player"]["coins"] = 0;
            }
            document["save_version"] = 2;
        } else if (version == 2) {
            // v2 -> v3: Living Room orb/gem currency.
            if (document.contains("player") && document["player"].is_object() &&
                !document["player"].contains("orbs")) {
                document["player"]["orbs"] = 0;
            }
            document["save_version"] = 3;
        } else if (version == 3) {
            // v3 -> v4: per-room chest and coin state.
            document["rooms"] = Json::array();
            document["save_version"] = 4;
        } else if (version == 4) {
            // v4 -> v5: selected pet identity.
            document["pet_id"] = "";
            document["save_version"] = 5;
        } else {
            if (error != nullptr) {
                *error = "no migration path from save version " +
                         std::to_string(version);
            }
            return false;
        }
        version = document.value("save_version", 0);
    }
    return true;
}

Json Serialize(const SaveData& data) {
    Json document = Json::object();
    document["save_version"] = SaveData::kCurrentVersion;
    document["game_version"] = data.game_version;
    document["scene_id"] = data.scene_id;
    document["playtime_seconds"] = data.playtime_seconds;
    document["unlocked_characters"] = data.unlocked_characters;
    document["plants"] = data.plants;
    document["plant_states"] = data.plant_states;
    document["seed_stock"] = data.seed_stock;
    document["fullscreen"] = data.fullscreen;
    document["hub_visited"] = data.hub_visited;
    document["auto_aim"] = data.auto_aim;
    document["map_seed"] = data.map_seed;
    document["campaign_level"] = data.campaign_level;
    document["trader_stock_seed"] = data.trader_stock_seed;
    document["sold_trader_offers"] = data.sold_trader_offers;
    document["pet_id"] = data.pet_id;
    Json materials = Json::array();
    for (const SaveMaterial& material : data.materials) {
        materials.push_back(Json{{"id", material.id},
                                 {"count", material.count}});
    }
    document["materials"] = std::move(materials);
    document["unlocked_blueprints"] = data.unlocked_blueprints;
    document["cleared_rooms"] = data.cleared_rooms;
    document["chest_spawned_rooms"] = data.chest_spawned_rooms;
    document["sold_shop_items"] = data.sold_shop_items;
    document["active_statue"] = data.active_statue;
    document["active_mercenary"] = data.active_mercenary;
    document["mercenary_level"] = data.mercenary_level;
    document["active_mount"] = data.active_mount;
    document["reduced_weapon_effects"] = data.reduced_weapon_effects;
    document["living_room_suggestions"] = data.living_room_suggestions;
    document["pot_empty"] = Json::array({data.pot_empty[0], data.pot_empty[1]});

    Json player = Json::object();
    player["character_id"] = data.character_id;
    player["position"] = Json::array({data.position_x, data.position_y});
    player["hp"] = data.hp;
    player["max_hp"] = data.max_hp;
    player["armor"] = data.armor;
    player["max_armor"] = data.max_armor;
    player["energy"] = data.energy;
    player["max_energy"] = data.max_energy;
    player["coins"] = data.coins;
    player["orbs"] = data.orbs;
    player["dead"] = data.dead;
    document["player"] = std::move(player);

    Json inventory = Json::object();
    inventory["weapon_slots"] = Json::array(
        {data.weapon_slots[0], data.weapon_slots[1]});
    inventory["active_slot"] = data.active_slot;
    document["inventory"] = std::move(inventory);

    Json rooms = Json::array();
    for (const SaveRoomItems& room : data.rooms) {
        Json room_json = Json::object();
        room_json["id"] = room.id;
        Json chests = Json::array();
        for (const SaveChestState& chest : room.chests) {
            chests.push_back(Json{
                {"definition", chest.definition},
                {"position", Json::array({chest.x, chest.y})},
                {"state", chest.state},
                {"timer", chest.timer},
                {"loot_spawned", chest.loot_spawned},
                {"pending_weapon", chest.pending_weapon},
                {"pending_weapons", chest.pending_weapons},
            });
        }
        Json coins = Json::array();
        for (const SaveCoinState& coin : room.coins) {
            coins.push_back(Json{
                {"definition", coin.definition},
                {"position", Json::array({coin.x, coin.y})},
                {"active", coin.active},
            });
        }
        room_json["chests"] = std::move(chests);
        room_json["coins"] = std::move(coins);
        rooms.push_back(std::move(room_json));
    }
    document["rooms"] = std::move(rooms);
    return document;
}

bool Deserialize(const Json& document, SaveData* data, std::string* error) {
    if (data == nullptr) return false;
    if (error != nullptr) error->clear();
    if (!document.is_object() || !document.contains("player") ||
        !document.contains("inventory")) {
        if (error != nullptr) *error = "save is missing player/inventory";
        return false;
    }
    const Json& player = document["player"];
    const Json& inventory = document["inventory"];
    if (!player.is_object() || !inventory.is_object()) {
        if (error != nullptr) *error = "save player/inventory is not an object";
        return false;
    }

    SaveData loaded;
    loaded.save_version = document.value("save_version", 0);
    if (loaded.save_version != SaveData::kCurrentVersion) {
        if (error != nullptr) *error = "save was not migrated to current version";
        return false;
    }
    if (!ReadString(document, "game_version", &loaded.game_version, error, false) ||
        !ReadString(document, "scene_id", &loaded.scene_id, error, false) ||
        !ReadString(player, "character_id", &loaded.character_id, error)) {
        return false;
    }
    if (document.contains("playtime_seconds")) {
        if (!document["playtime_seconds"].is_number_integer()) {
            if (error != nullptr) *error = "playtime_seconds is not an integer";
            return false;
        }
        loaded.playtime_seconds = document["playtime_seconds"].get<std::int64_t>();
    }
    const auto read_position = [&]() -> bool {
        if (!player.contains("position") || !player["position"].is_array() ||
            player["position"].size() != 2) {
            if (error != nullptr) *error = "save player position is invalid";
            return false;
        }
        if (!player["position"][0].is_number() ||
            !player["position"][1].is_number()) {
            if (error != nullptr) *error = "save player position is not numeric";
            return false;
        }
        loaded.position_x = player["position"][0].get<float>();
        loaded.position_y = player["position"][1].get<float>();
        return std::isfinite(loaded.position_x) &&
               std::isfinite(loaded.position_y);
    };
    if (!read_position()) return false;
    if (!ReadInt(player, "hp", &loaded.hp, error) ||
        !ReadInt(player, "max_hp", &loaded.max_hp, error) ||
        !ReadInt(player, "armor", &loaded.armor, error) ||
        !ReadInt(player, "max_armor", &loaded.max_armor, error) ||
        !ReadInt(player, "energy", &loaded.energy, error) ||
        !ReadInt(player, "max_energy", &loaded.max_energy, error) ||
        !ReadInt(player, "coins", &loaded.coins, error, false) ||
        !ReadInt(player, "orbs", &loaded.orbs, error, false) ||
        !ReadBool(player, "dead", &loaded.dead, error)) {
        return false;
    }
    if (document.contains("unlocked_characters") &&
        document["unlocked_characters"].is_array()) {
        for (const Json& item : document["unlocked_characters"]) {
            if (item.is_string()) {
                loaded.unlocked_characters.push_back(item.get<std::string>());
            }
        }
    }
    if (document.contains("plants") && document["plants"].is_array()) {
        for (const Json& item : document["plants"]) {
            if (item.is_string()) loaded.plants.push_back(item.get<std::string>());
        }
    }
    if (document.contains("plant_states") &&
        document["plant_states"].is_array()) {
        for (const Json& item : document["plant_states"]) {
            if (item.is_string()) {
                loaded.plant_states.push_back(item.get<std::string>());
            }
        }
    }
    if (document.contains("seed_stock") &&
        document["seed_stock"].is_array()) {
        for (const Json& item : document["seed_stock"]) {
            if (item.is_string()) {
                loaded.seed_stock.push_back(item.get<std::string>());
            }
        }
    }
    if (document.contains("fullscreen") && document["fullscreen"].is_boolean()) {
        loaded.fullscreen = document["fullscreen"].get<bool>();
    }
    if (document.contains("hub_visited") && document["hub_visited"].is_boolean()) {
        loaded.hub_visited = document["hub_visited"].get<bool>();
    }
    if (document.contains("auto_aim") && document["auto_aim"].is_boolean()) {
        loaded.auto_aim = document["auto_aim"].get<bool>();
    }
    if (document.contains("map_seed") &&
        document["map_seed"].is_number_integer()) {
        loaded.map_seed = document["map_seed"].get<std::int32_t>();
    }
    if (document.contains("campaign_level") &&
        document["campaign_level"].is_string()) {
        loaded.campaign_level =
            document["campaign_level"].get<std::string>();
    }
    if (document.contains("trader_stock_seed") &&
        document["trader_stock_seed"].is_number_integer()) {
        loaded.trader_stock_seed =
            document["trader_stock_seed"].get<std::int64_t>();
    }
    if (document.contains("sold_trader_offers") &&
        document["sold_trader_offers"].is_array()) {
        for (const Json& item : document["sold_trader_offers"]) {
            if (item.is_number_integer()) {
                loaded.sold_trader_offers.push_back(item.get<int>());
            }
        }
    }
    if (document.contains("unlocked_blueprints") &&
        document["unlocked_blueprints"].is_array()) {
        for (const Json& item : document["unlocked_blueprints"]) {
            if (item.is_string()) {
                loaded.unlocked_blueprints.push_back(item.get<std::string>());
            }
        }
    }
    if (document.contains("materials") && document["materials"].is_array()) {
        for (const Json& item : document["materials"]) {
            if (!item.is_object() || !item.contains("id") ||
                !item["id"].is_string()) {
                continue;
            }
            SaveMaterial material;
            material.id = item["id"].get<std::string>();
            material.count = item.value("count", 0);
            if (material.count < 0) material.count = 0;
            loaded.materials.push_back(std::move(material));
        }
    }
    loaded.active_statue = document.value("active_statue", std::string{});
    loaded.active_mercenary = document.value("active_mercenary", std::string{});
    loaded.mercenary_level = document.value("mercenary_level", 0);
    loaded.active_mount = document.value("active_mount", std::string{});
    loaded.reduced_weapon_effects =
        document.value("reduced_weapon_effects", true);
    loaded.living_room_suggestions =
        document.value("living_room_suggestions", false);
    if (document.contains("pot_empty") && document["pot_empty"].is_array() &&
        document["pot_empty"].size() == 2) {
        loaded.pot_empty[0] = document["pot_empty"][0].get<bool>();
        loaded.pot_empty[1] = document["pot_empty"][1].get<bool>();
    }
    if (document.contains("sold_shop_items") &&
        document["sold_shop_items"].is_array()) {
        for (const Json& item : document["sold_shop_items"]) {
            if (item.is_string()) {
                loaded.sold_shop_items.push_back(item.get<std::string>());
            }
        }
    }
    if (document.contains("rooms") && document["rooms"].is_array()) {
        for (const Json& room_json : document["rooms"]) {
            if (!room_json.is_object() || !room_json.contains("id") ||
                !room_json["id"].is_string()) {
                continue;
            }
            SaveRoomItems room;
            room.id = room_json["id"].get<std::string>();
            if (room_json.contains("chests") && room_json["chests"].is_array()) {
                for (const Json& chest_json : room_json["chests"]) {
                    if (!chest_json.is_object()) continue;
                    SaveChestState chest;
                    chest.definition = chest_json.value("definition", std::string{});
                    if (chest_json.contains("position") &&
                        chest_json["position"].is_array() &&
                        chest_json["position"].size() == 2) {
                        chest.x = chest_json["position"][0].get<float>();
                        chest.y = chest_json["position"][1].get<float>();
                    }
                    chest.state = chest_json.value("state", 0);
                    chest.timer = chest_json.value("timer", 0.0f);
                    chest.loot_spawned = chest_json.value("loot_spawned", false);
                    chest.pending_weapon = chest_json.value("pending_weapon", std::string{});
                    if (chest_json.contains("pending_weapons") &&
                        chest_json["pending_weapons"].is_array()) {
                        for (const Json& weapon_json : chest_json["pending_weapons"]) {
                            if (weapon_json.is_string()) {
                                chest.pending_weapons.push_back(
                                    weapon_json.get<std::string>());
                            }
                        }
                    } else if (!chest.pending_weapon.empty()) {
                        chest.pending_weapons.push_back(chest.pending_weapon);
                    }
                    room.chests.push_back(std::move(chest));
                }
            }
            if (room_json.contains("coins") && room_json["coins"].is_array()) {
                for (const Json& coin_json : room_json["coins"]) {
                    if (!coin_json.is_object()) continue;
                    SaveCoinState coin;
                    coin.definition = coin_json.value("definition", std::string{});
                    if (coin_json.contains("position") &&
                        coin_json["position"].is_array() &&
                        coin_json["position"].size() == 2) {
                        coin.x = coin_json["position"][0].get<float>();
                        coin.y = coin_json["position"][1].get<float>();
                    }
                    coin.active = coin_json.value("active", true);
                    room.coins.push_back(std::move(coin));
                }
            }
            loaded.rooms.push_back(std::move(room));
        }
    }
    if (!inventory.contains("weapon_slots") ||
        !inventory["weapon_slots"].is_array() ||
        inventory["weapon_slots"].size() != 2) {
        if (error != nullptr) *error = "save weapon_slots is invalid";
        return false;
    }
    for (std::size_t i = 0; i < 2; ++i) {
        if (!inventory["weapon_slots"][i].is_string()) {
            if (error != nullptr) *error = "save weapon slot id is not a string";
            return false;
        }
        loaded.weapon_slots[i] = inventory["weapon_slots"][i].get<std::string>();
    }
    if (!ReadInt(inventory, "active_slot", &loaded.active_slot, error)) {
        return false;
    }
    if (loaded.active_slot < 0 || loaded.active_slot > 1) {
        if (error != nullptr) *error = "save active_slot is out of range";
        return false;
    }
    if (loaded.max_hp < 0 || loaded.max_armor < 0 || loaded.max_energy < 0 ||
        loaded.hp < 0 || loaded.hp > loaded.max_hp ||
        loaded.armor < 0 || loaded.armor > loaded.max_armor ||
        loaded.energy < 0 || loaded.energy > loaded.max_energy) {
        if (error != nullptr) *error = "save player values are out of range";
        return false;
    }
    *data = std::move(loaded);
    return true;
}

} // namespace soulknight::game::save
