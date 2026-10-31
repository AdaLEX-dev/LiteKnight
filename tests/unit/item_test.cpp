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
          "chests": [{"id":"chest00","level":0,"loot_coin":"coin_0","loot_count":1,
                      "visuals":[{"sprite":"sprite","offset":[0,0],
                                  "sorting_layer":5,"sorting_order":1}]}],
          "coins": [{"id":"coin_0","type":0,"value":5,"speed":20,"range":7,
                     "frames":["sprite"]}]
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
    world.Update(0.5f, {10.0f, 10.0f}, assets);
    CHECK(world.Chests().front().state == soulknight::game::items::ChestState::Open);
    CHECK(world.Coins().size() == 1);
    CHECK(world.CollectCoins({10.0f, 10.0f}, 5.0f, assets) == 5);
    CHECK(world.CollectCoins({10.0f, 10.0f}, 5.0f, assets) == 0);
    world.Update(0.5f, {10.0f, 10.0f}, assets);
    CHECK(world.Coins().size() == 1); // no duplicated loot
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
    world.Update(0.5f, {3.0f, 3.0f}, assets);
    world.EnterRoom("r1_2_visual", true, {6.0f, 6.0f});
    CHECK(world.Chests().size() == 1);
    CHECK(world.Chests().front().state == soulknight::game::items::ChestState::Closed);
    world.EnterRoom("r1_1_visual", true, {3.0f, 3.0f});
    CHECK(world.Chests().front().state == soulknight::game::items::ChestState::Open);
    CHECK(world.Coins().size() == 1);
}
