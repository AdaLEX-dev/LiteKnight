// Soul Knight Linux — full weapon catalogue index tests
#include <doctest/doctest.h>

#include "game/weapons/WeaponCatalogue.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

using soulknight::game::weapons::WeaponCatalogue;
using soulknight::platform::FileSystem;

namespace {
class TempCatalogue {
public:
    TempCatalogue() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("liteknight-catalogue-" + std::to_string(stamp) + ".json");
        std::ofstream stream(path_);
        stream << R"({
          "weapons": [
            {"id":"weapon_000","display_name":"Bad Pistol","type":"Pistol",
             "damage":3,"energy_cost":0,"item_level":0},
            {"id":"weapon_225","display_name":"weapon_225","type":null,
             "damage":10,"energy_cost":0,"item_level":5},
            {"id":"weapon_232","display_name":"Staff of Abyss","type":null}
          ]
        })";
    }
    ~TempCatalogue() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }
    const std::filesystem::path& Path() const { return path_; }
private:
    std::filesystem::path path_;
};
} // namespace

TEST_CASE("Weapon catalogue resolves names and tolerates null metadata fields" *
          doctest::test_suite("unit")) {
    TempCatalogue temp;
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    WeaponCatalogue catalogue;
    std::string error;
    CHECK(catalogue.Load(filesystem, temp.Path(), &error));
    CHECK(error.empty());
    CHECK(catalogue.Count() == 3);
    const auto* bad = catalogue.Find("weapon_000");
    CHECK(bad != nullptr);
    CHECK(bad->display_name == "Bad Pistol");
    CHECK(bad->damage == 3);
    CHECK(bad->tier == "B");
    const auto* special = catalogue.Find("weapon_225");
    CHECK(special != nullptr);
    CHECK(special->display_name == "weapon_225");
    CHECK(special->type_name.empty());
    const auto* no_stats = catalogue.Find("weapon_232");
    CHECK(no_stats != nullptr);
    CHECK_FALSE(no_stats->has_serialized_stats);
    CHECK(no_stats->tier == "C");
    catalogue.MarkRuntimeIntegrated("weapon_000");
    CHECK(catalogue.Find("weapon_000")->tier == "A");
    CHECK(catalogue.Find("missing") == nullptr);
}
