// Soul Knight Linux — native save format, migration and atomicity tests
#include <doctest/doctest.h>

#include "game/save/SaveManager.hpp"
#include "game/save/SaveSerializer.hpp"

#include <chrono>
#include <fstream>
#include <filesystem>

using soulknight::game::save::Json;
using soulknight::game::save::SaveData;
using soulknight::game::save::SaveManager;

namespace {

class TempSaveDirectory {
public:
    TempSaveDirectory() {
        const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("liteknight-save-test-" + std::to_string(stamp));
    }
    ~TempSaveDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    const std::filesystem::path& Path() const { return path_; }
private:
    std::filesystem::path path_;
};

SaveData MakeSave() {
    SaveData data;
    data.game_version = "0.1.0-dev";
    data.scene_id = "r1_1_visual";
    data.character_id = "c00";
    data.weapon_slots = {"weapon_000", "weapon_048"};
    data.active_slot = 1;
    data.position_x = 20.25f;
    data.position_y = 15.75f;
    data.hp = 4;
    data.max_hp = 6;
    data.armor = 2;
    data.max_armor = 5;
    data.energy = 120;
    data.max_energy = 180;
    data.coins = 17;
    data.orbs = 123;
    data.dead = false;
    data.playtime_seconds = 42;
    data.unlocked_characters = {"c00", "c01", "c02"};
    data.plants = {"plant_carrot"};
    data.plant_states = {
        "garden_0|plant_carrot|1|12.5|1|0|30|4|weapon_177|1"};
    data.seed_stock = {"plant_carrot|3", "plant_cactus|1"};
    data.fullscreen = true;
    data.hub_visited = true;
    data.auto_aim = true;
    data.map_seed = 20261031;
    data.trader_stock_seed = 987654321;
    data.sold_trader_offers = {0, 2};
    data.unlocked_blueprints = {"weapon_184", "weapon_294"};
    data.materials = {{"material_iron", 37}, {"material_wood", 112}};
    data.sold_shop_items = {"health_pot", "weapon_005"};
    data.active_statue = "statue_04";
    data.active_mercenary = "npc_03";
    data.mercenary_level = 3;
    data.active_mount = "npc_mount_creature";
    data.pot_empty = {true, false};
    soulknight::game::save::SaveRoomItems room;
    room.id = "r1_1_visual";
    soulknight::game::save::SaveChestState chest;
    chest.definition = "chest01";
    chest.x = 20.0f;
    chest.y = 17.0f;
    chest.state = 2;
    chest.loot_spawned = true;
    chest.pending_weapon = "weapon_021";
    chest.pending_weapons = {"weapon_021", "weapon_002"};
    room.chests.push_back(chest);
    soulknight::game::save::SaveCoinState coin;
    coin.definition = "coin_0";
    coin.x = 19.0f;
    coin.y = 16.0f;
    room.coins.push_back(coin);
    data.rooms.push_back(room);
    return data;
}

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    CHECK(static_cast<bool>(stream));
    stream << text;
}

} // namespace

TEST_CASE("Save round-trips through atomic slot file" *
          doctest::test_suite("unit")) {
    TempSaveDirectory temp;
    SaveManager manager(temp.Path());
    CHECK_FALSE(manager.Exists());
    CHECK_FALSE(manager.Load(nullptr, nullptr));

    std::string error;
    SaveData original = MakeSave();
    CHECK(manager.Save(original, &error));
    CHECK(error.empty());
    CHECK(manager.Exists());
    CHECK(std::filesystem::is_regular_file(manager.SlotPath()));
    CHECK_FALSE(std::filesystem::exists(manager.SlotPath().string() + ".tmp"));

    SaveData loaded;
    CHECK(manager.Load(&loaded, &error));
    CHECK(loaded.save_version == SaveData::kCurrentVersion);
    CHECK(loaded.character_id == "c00");
    CHECK(loaded.scene_id == "r1_1_visual");
    CHECK(loaded.weapon_slots[0] == "weapon_000");
    CHECK(loaded.weapon_slots[1] == "weapon_048");
    CHECK(loaded.active_slot == 1);
    CHECK(loaded.position_x == doctest::Approx(20.25f));
    CHECK(loaded.position_y == doctest::Approx(15.75f));
    CHECK(loaded.hp == 4);
    CHECK(loaded.armor == 2);
    CHECK(loaded.energy == 120);
    CHECK(loaded.coins == 17);
    CHECK(loaded.orbs == 123);
    CHECK(loaded.playtime_seconds == 42);
    CHECK(loaded.unlocked_characters.size() == 3);
    CHECK(loaded.plants.size() == 1);
    CHECK(loaded.plants.front() == "plant_carrot");
    CHECK(loaded.plant_states.size() == 1);
    CHECK(loaded.plant_states.front().find("garden_0|plant_carrot") == 0);
    CHECK(loaded.seed_stock.size() == 2);
    CHECK(loaded.fullscreen);
    CHECK(loaded.hub_visited);
    CHECK(loaded.auto_aim);
    CHECK(loaded.map_seed == 20261031);
    CHECK(loaded.trader_stock_seed == 987654321);
    CHECK(loaded.sold_trader_offers == std::vector<int>{0, 2});
    CHECK(loaded.unlocked_blueprints ==
          std::vector<std::string>{"weapon_184", "weapon_294"});
    CHECK(loaded.materials.size() == 2);
    if (loaded.materials.size() == 2) {
        CHECK(loaded.materials[0].id == "material_iron");
        CHECK(loaded.materials[0].count == 37);
        CHECK(loaded.materials[1].id == "material_wood");
        CHECK(loaded.materials[1].count == 112);
    }
    CHECK(loaded.sold_shop_items.size() == 2);
    CHECK(loaded.active_statue == "statue_04");
    CHECK(loaded.active_mercenary == "npc_03");
    CHECK(loaded.mercenary_level == 3);
    CHECK(loaded.active_mount == "npc_mount_creature");
    CHECK(loaded.pot_empty[0]);
    CHECK_FALSE(loaded.pot_empty[1]);
    CHECK(loaded.rooms.size() == 1);
    if (loaded.rooms.size() == 1) {
        CHECK(loaded.rooms[0].id == "r1_1_visual");
        CHECK(loaded.rooms[0].chests.size() == 1);
        if (loaded.rooms[0].chests.size() == 1) {
            CHECK(loaded.rooms[0].chests[0].definition == "chest01");
            CHECK(loaded.rooms[0].chests[0].state == 2);
            CHECK(loaded.rooms[0].chests[0].loot_spawned);
            CHECK(loaded.rooms[0].chests[0].pending_weapon == "weapon_021");
            CHECK(loaded.rooms[0].chests[0].pending_weapons.size() == 2);
            CHECK(loaded.rooms[0].chests[0].pending_weapons[1] == "weapon_002");
        }
        CHECK(loaded.rooms[0].coins.size() == 1);
        if (loaded.rooms[0].coins.size() == 1) {
            CHECK(loaded.rooms[0].coins[0].definition == "coin_0");
        }
    }
}

TEST_CASE("Fresh save defaults to auto aim" * doctest::test_suite("unit")) {
    const SaveData fresh;
    CHECK(fresh.auto_aim);
    CHECK(fresh.coins == 0);
}

TEST_CASE("Missing save returns a diagnostic rather than a fake default" *
          doctest::test_suite("unit")) {
    TempSaveDirectory temp;
    SaveManager manager(temp.Path());
    SaveData loaded;
    std::string error;
    CHECK_FALSE(manager.Load(&loaded, &error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("Corrupt primary save falls back to the previous backup" *
          doctest::test_suite("unit")) {
    TempSaveDirectory temp;
    SaveManager manager(temp.Path());
    std::string error;
    SaveData first = MakeSave();
    first.hp = 6;
    CHECK(manager.Save(first, &error));
    SaveData second = MakeSave();
    second.hp = 1;
    CHECK(manager.Save(second, &error));
    CHECK(std::filesystem::is_regular_file(manager.BackupPath()));

    WriteFile(manager.SlotPath(), "{ this is not json");
    SaveData loaded;
    CHECK(manager.Load(&loaded, &error));
    CHECK(loaded.hp == 6);
    CHECK(error.find("backup") != std::string::npos);
}

TEST_CASE("Corruption in both primary and backup is reported, not discarded" *
          doctest::test_suite("unit")) {
    TempSaveDirectory temp;
    std::filesystem::create_directories(temp.Path());
    WriteFile(temp.Path() / "slot_0.json", "{ broken");
    WriteFile(temp.Path() / "slot_0.json.bak", "also broken");
    SaveManager manager(temp.Path());
    SaveData loaded;
    std::string error;
    CHECK_FALSE(manager.Load(&loaded, &error));
    CHECK(error.find("primary") != std::string::npos);
    CHECK(error.find("backup") != std::string::npos);
}

TEST_CASE("Version 0 native draft migrates to current schema" *
          doctest::test_suite("unit")) {
    TempSaveDirectory temp;
    std::filesystem::create_directories(temp.Path());
    Json legacy = Json::object();
    legacy["character"] = "c03";
    legacy["weapons"] = Json::array({"weapon_002", "weapon_287"});
    legacy["active_weapon"] = 1;
    legacy["hp"] = 3;
    legacy["max_hp"] = 6;
    legacy["armor"] = 1;
    legacy["max_armor"] = 5;
    legacy["energy"] = 99;
    legacy["max_energy"] = 180;
    legacy["x"] = 7.0f;
    legacy["y"] = 8.0f;
    WriteFile(temp.Path() / "slot_0.json", legacy.dump(2));

    SaveManager manager(temp.Path());
    SaveData loaded;
    std::string error;
    CHECK(manager.Load(&loaded, &error));
    CHECK(loaded.save_version == SaveData::kCurrentVersion);
    CHECK(loaded.character_id == "c03");
    CHECK(loaded.weapon_slots[0] == "weapon_002");
    CHECK(loaded.weapon_slots[1] == "weapon_287");
    CHECK(loaded.active_slot == 1);
    CHECK(loaded.hp == 3);
    CHECK(loaded.energy == 99);
    CHECK(loaded.position_x == doctest::Approx(7.0f));
    CHECK(loaded.position_y == doctest::Approx(8.0f));
}

TEST_CASE("Version 1 native save migrates additively to current schema" *
          doctest::test_suite("unit")) {
    TempSaveDirectory temp;
    std::filesystem::create_directories(temp.Path());
    Json v1 = Json::object();
    v1["save_version"] = 1;
    v1["player"] = Json::object({
        {"character_id", "c00"},
        {"position", Json::array({1.0f, 2.0f})},
        {"hp", 6}, {"max_hp", 6}, {"armor", 5}, {"max_armor", 5},
        {"energy", 180}, {"max_energy", 180}, {"dead", false}});
    v1["inventory"] = Json::object({
        {"weapon_slots", Json::array({"weapon_000", ""})},
        {"active_slot", 0}});
    WriteFile(temp.Path() / "slot_0.json", v1.dump(2));
    SaveManager manager(temp.Path());
    SaveData loaded;
    std::string error;
    CHECK(manager.Load(&loaded, &error));
    CHECK(loaded.save_version == SaveData::kCurrentVersion);
    CHECK(loaded.character_id == "c00");
    CHECK(loaded.unlocked_characters.empty());
    CHECK(loaded.plants.empty());
    CHECK_FALSE(loaded.fullscreen);
    CHECK_FALSE(loaded.hub_visited);
}

TEST_CASE("Future save version is rejected and never treated as current" *
          doctest::test_suite("unit")) {
    TempSaveDirectory temp;
    std::filesystem::create_directories(temp.Path());
    Json future = Json::object();
    future["save_version"] = 99;
    future["player"] = Json::object();
    WriteFile(temp.Path() / "slot_0.json", future.dump(2));
    SaveManager manager(temp.Path());
    SaveData loaded;
    std::string error;
    CHECK_FALSE(manager.Load(&loaded, &error));
    CHECK(error.find("newer") != std::string::npos);
}

TEST_CASE("Invalid player ranges and slots are rejected" *
          doctest::test_suite("unit")) {
    TempSaveDirectory temp;
    std::filesystem::create_directories(temp.Path());
    Json document = Json::parse(R"({
        "save_version": 1,
        "player": {"character_id":"c00","position":[0,0],"hp":9,"max_hp":6,
                   "armor":0,"max_armor":5,"energy":0,"max_energy":180,
                   "dead":false},
        "inventory": {"weapon_slots":["weapon_000","weapon_001"],
                      "active_slot":0}
    })");
    WriteFile(temp.Path() / "slot_0.json", document.dump(2));
    SaveManager manager(temp.Path());
    SaveData loaded;
    std::string error;
    CHECK_FALSE(manager.Load(&loaded, &error));
    CHECK(error.find("out of range") != std::string::npos);
}

TEST_CASE("Delete removes both slot and backup" *
          doctest::test_suite("unit")) {
    TempSaveDirectory temp;
    SaveManager manager(temp.Path());
    std::string error;
    CHECK(manager.Save(MakeSave(), &error));
    CHECK(manager.Save(MakeSave(), &error));
    CHECK(std::filesystem::exists(manager.BackupPath()));
    CHECK(manager.Delete(&error));
    CHECK_FALSE(manager.Exists());
    CHECK_FALSE(std::filesystem::exists(manager.BackupPath()));
}

TEST_CASE("Second save preserves the first as a recoverable backup" *
          doctest::test_suite("unit")) {
    TempSaveDirectory temp;
    SaveManager manager(temp.Path());
    std::string error;
    SaveData first = MakeSave();
    first.energy = 10;
    CHECK(manager.Save(first, &error));
    SaveData second = MakeSave();
    second.energy = 20;
    CHECK(manager.Save(second, &error));

    SaveData loaded;
    CHECK(manager.Load(&loaded, &error));
    CHECK(loaded.energy == 20);
    std::ifstream backup(manager.BackupPath());
    CHECK(static_cast<bool>(backup));
    Json backup_document = Json::parse(backup, nullptr, false);
    CHECK(!backup_document.is_discarded());
    CHECK(backup_document["player"]["energy"].get<int>() == 10);
}
