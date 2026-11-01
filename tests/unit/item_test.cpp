// Soul Knight Linux — recovered chest/coin state tests
#include <doctest/doctest.h>

#include "game/items/ItemAssets.hpp"
#include "game/items/ItemWorld.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

using soulknight::game::items::ItemAssets;
using soulknight::game::items::ItemWorld;
using soulknight::platform::FileSystem;

namespace {
class TempItemDirectory {
public:
    TempItemDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("liteknight-item-test-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
        const std::filesystem::path source =
            std::filesystem::path(SOUL_KNIGHT_TEST_ASSET_DIR) / "phase4_smoke.png";
        std::filesystem::copy_file(source, path_ / "texture.png",
                                   std::filesystem::copy_options::overwrite_existing);
        std::ofstream json(path_ / "items.json");
        json << R"({
          "id": "test_items",
          "camera": {"position": [0,0], "orthographic_size": 10.5},
          "spawn": [0,0],
          "instances": [],
          "textures": [{"id":"tex","png":"texture.png","filter":"point","wrap":"clamp"}],
          "sprites": [{"id":"sprite","texture":"tex","rect":[0,0,1,1],
                       "pivot":[0.5,0.5],"pixels_per_unit":1.0}],
          "chests": [
            {"id":"chest00","level":0,"open_time":1.0,"loot_coin":"coin_0","loot_count":1,
             "visuals":[{"sprite":"sprite","offset":[0,0],
                         "sorting_layer":5,"sorting_order":1}]},
            {"id":"chest01","level":1,"open_time":1.0,"weapon_drop_level":1,
             "visuals":[{"sprite":"sprite","offset":[0,0],
                         "sorting_layer":5,"sorting_order":1}]},
            {"id":"chest_boss01","level":2,"open_time":1.0,"boss":true,"object_list":["weapon_fixed"],"weapon_drop_level":2,
             "visuals":[{"sprite":"sprite","offset":[0,0],
                         "sorting_layer":5,"sorting_order":1}]},
            {"id":"chest_big","level":1,"open_time":1.0,"big":true,
             "weapon_count":2,"weapon_drop_level":1,
             "visuals":[{"sprite":"sprite","offset":[0,0],
                         "sorting_layer":5,"sorting_order":1}]}
          ],
          "coins": [{"id":"coin_0","type":0,"value":5,"speed":20,"range":7,
                     "frames":["sprite"]}],
          "weapon_drops": [
            {"level":1,"entries":[
              {"weapon":"weapon_000","weight":1,"title":"Bad Pistol"},
              {"weapon":"weapon_001","weight":1,"title":"P250 Pistol"}]},
            {"level":2,"entries":[
              {"weapon":"weapon_002","weight":1,"title":"Chest A"}]},
            {"level":5,"entries":[
              {"weapon":"weapon_005","weight":1,"title":"Boss A"}]},
            {"level":6,"entries":[
              {"weapon":"weapon_006","weight":1,"title":"Boss B"}]}
          ]
        })";
    }
    ~TempItemDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }
    const std::filesystem::path& Path() const { return path_; }
private:
    std::filesystem::path path_;
};
} // namespace

TEST_CASE("Chest opens once and spawns one collectible coin" *
          doctest::test_suite("unit")) {
    TempItemDirectory temp;
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    ItemAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, temp.Path() / "items.json", &error));
    CHECK(error.empty());

    ItemWorld world;
    world.EnterRoom("r1_1_visual", true, {10.0f, 10.0f});
    CHECK(world.Chests().size() == 1);
    CHECK(world.Chests().front().state == soulknight::game::items::ChestState::Closed);
    CHECK(world.OpenNearestChest({10.0f, 10.0f}, 2.0f));
    CHECK_FALSE(world.OpenNearestChest({10.0f, 10.0f}, 2.0f));
    world.Update(1.01f, {10.0f, 10.0f}, assets);
    CHECK(world.Chests().front().state == soulknight::game::items::ChestState::Open);
    CHECK(world.Coins().size() == 1);
    CHECK(world.CollectCoins({10.0f, 10.0f}, 5.0f, assets) == 5);
    CHECK(world.CollectCoins({10.0f, 10.0f}, 5.0f, assets) == 0);
    world.Update(0.2f, {10.0f, 10.0f}, assets);
    CHECK(world.Coins().size() == 1); // no duplicated loot
}

TEST_CASE("RGChest weapon loot uses the recovered drop table deterministically" *
          doctest::test_suite("unit")) {
    TempItemDirectory temp;
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    ItemAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, temp.Path() / "items.json", &error));
    CHECK(assets.WeaponDrops().size() == 4);
    ItemWorld world;
    world.SetAvailableWeapons({"weapon_005"});
    world.EnterRoom("r1_2_visual", false, {0.0f, 0.0f});
    world.AddChest("chest01", {4.0f, 4.0f});
    CHECK(world.OpenNearestChest({4.0f, 4.0f}, 2.0f));
    world.Update(0.99f, {4.0f, 4.0f}, assets);
    CHECK(world.TakePendingWeaponDrops().empty());
    world.Update(0.02f, {4.0f, 4.0f}, assets);
    const auto drops = world.TakePendingWeaponDrops();
    CHECK(drops.size() == 1);
    if (drops.size() == 1) {
        CHECK(drops[0].weapon == "weapon_005");
        CHECK(drops[0].position.x == doctest::Approx(4.0f));
    }
    CHECK(world.TakePendingWeaponDrops().empty());
}

TEST_CASE("RGChestBoss uses its fixed object_list or the level-5/6 pool" *
          doctest::test_suite("unit")) {
    TempItemDirectory temp;
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    ItemAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, temp.Path() / "items.json", &error));
    ItemWorld world;
    world.EnterRoom("r1_2_visual", false, {0.0f, 0.0f});
    world.AddChest("chest_boss01", {4.0f, 4.0f});
    CHECK(world.OpenNearestChest({4.0f, 4.0f}, 2.0f));
    world.Update(1.01f, {4.0f, 4.0f}, assets);
    const auto drops = world.TakePendingWeaponDrops();
    CHECK(drops.size() == 1);
    if (drops.size() == 1) {
        CHECK((drops[0].weapon == "weapon_fixed" ||
               drops[0].weapon == "weapon_005" ||
               drops[0].weapon == "weapon_006"));
    }
}

TEST_CASE("RGChestBig rolls its recovered two-item count" *
          doctest::test_suite("unit")) {
    TempItemDirectory temp;
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    ItemAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, temp.Path() / "items.json", &error));
    ItemWorld world;
    world.EnterRoom("r1_2_visual", false, {0.0f, 0.0f});
    world.AddChest("chest_big", {4.0f, 4.0f});
    CHECK(world.OpenNearestChest({4.0f, 4.0f}, 2.0f));
    world.Update(1.01f, {4.0f, 4.0f}, assets);
    CHECK(world.TakePendingWeaponDrops().size() == 2);
}

TEST_CASE("Room transitions preserve opened chest state" *
          doctest::test_suite("unit")) {
    TempItemDirectory temp;
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    ItemAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, temp.Path() / "items.json", &error));
    ItemWorld world;
    world.EnterRoom("r1_1_visual", true, {3.0f, 3.0f});
    CHECK(world.OpenNearestChest({3.0f, 3.0f}, 2.0f));
    world.Update(1.01f, {3.0f, 3.0f}, assets);
    world.EnterRoom("r1_2_visual", true, {6.0f, 6.0f});
    CHECK(world.Chests().size() == 1);
    CHECK(world.Chests().front().state == soulknight::game::items::ChestState::Closed);
    world.EnterRoom("r1_1_visual", true, {3.0f, 3.0f});
    CHECK(world.Chests().front().state == soulknight::game::items::ChestState::Open);
    CHECK(world.Coins().size() == 1);
}

TEST_CASE("RGChest auto-opens on player proximity without E" *
          doctest::test_suite("unit")) {
    TempItemDirectory temp;
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    ItemAssets assets;
    std::string error;
    CHECK(assets.Load(filesystem, temp.Path() / "items.json", &error));
    ItemWorld world;
    world.EnterRoom("r1_1_visual", false, {0.0f, 0.0f});
    world.AddChest("chest00", {4.0f, 4.0f});
    CHECK_FALSE(world.AutoOpenNear({0.0f, 0.0f}, 2.0f));
    CHECK(world.AutoOpenNear({4.0f, 4.0f}, 2.0f));
    world.Update(0.99f, {4.0f, 4.0f}, assets);
    CHECK(world.Chests().front().state == soulknight::game::items::ChestState::Opening);
    world.Update(0.02f, {4.0f, 4.0f}, assets);
    CHECK(world.Chests().front().state == soulknight::game::items::ChestState::Open);
}
