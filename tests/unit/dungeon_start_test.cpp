// Soul Knight Linux — New Game must start at recovered level 1-1
#include <doctest/doctest.h>

#include "game/room/BiomeSystem.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <algorithm>
#include <string>

using soulknight::game::room::BiomeSystem;
using soulknight::platform::FileSystem;

TEST_CASE("Default campaign level is recovered 1-1" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    BiomeSystem biomes;
    std::string error;
    CHECK(biomes.Load(filesystem, "scenes/biomes.json", &error));
    CHECK(biomes.Count() == 16);
    CHECK(biomes.CurrentIndex() == 0);
    CHECK(biomes.Current() != nullptr);
    CHECK(biomes.Current()->id == "1-1");
    CHECK(biomes.Current()->map_long == 3);
    CHECK(biomes.Current()->chest_level >= 1);
    CHECK_FALSE(biomes.Current()->start_rooms.empty());
    CHECK(biomes.Current()->start_rooms.front() == "r_start_no_event");
    CHECK(std::find(biomes.Current()->end_rooms.begin(),
                    biomes.Current()->end_rooms.end(),
                    "r_transgete") != biomes.Current()->end_rooms.end());
}

TEST_CASE("Selecting and advancing levels follows 1-1..3-6 order" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    BiomeSystem biomes;
    std::string error;
    CHECK(biomes.Load(filesystem, "scenes/biomes.json", &error));
    CHECK(biomes.Select("1-2"));
    CHECK(biomes.Current()->id == "1-2");
    CHECK(biomes.Select("3-5"));
    CHECK(biomes.Advance());
    CHECK(biomes.Current()->id == "3-6");
    CHECK_FALSE(biomes.Advance());
    biomes.Reset();
    CHECK(biomes.Current()->id == "1-1");
    CHECK_FALSE(biomes.Select("0-1"));
    CHECK(biomes.Current()->id == "1-1");
}
