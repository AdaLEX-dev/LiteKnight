// Soul Knight Linux — Living Room geometry and boundary movement tests
#include <doctest/doctest.h>

#include "engine/collision/AabbCollision.hpp"
#include "engine/rendering/WorldDepth.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <cmath>
#include <string>

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

using soulknight::engine::Vec2;
using soulknight::engine::collision::AabbCollisionWorld;
using soulknight::platform::FileSystem;
using Json = nlohmann::json;

namespace {

Json LoadHub() {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    const auto text = filesystem.ReadTextFile("test/scenes/hero_room.json");
    CHECK(text.has_value());
    Json hub = Json::parse(text.value_or("{}"), nullptr, false);
    CHECK_FALSE(hub.is_discarded());
    return hub;
}

AabbCollisionWorld HubWorld(const Json& hub) {
    AabbCollisionWorld world;
    const Json& bounds = hub["collision"]["bounds"];
    if (bounds.is_array() && bounds.size() == 4) {
        world.SetBounds({{bounds[0].get<float>(), bounds[1].get<float>()},
                         {bounds[0].get<float>() + bounds[2].get<float>(),
                          bounds[1].get<float>() + bounds[3].get<float>()}});
    }
    for (const Json& solid : hub["collision"]["solids"]) {
        if (solid.size() != 4) continue;
        world.AddSolid({{solid[0].get<float>(), solid[1].get<float>()},
                        {solid[2].get<float>(), solid[3].get<float>()}});
    }
    return world;
}

} // namespace

TEST_CASE("Player cannot escape the Living Room beside the mech spawner" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    AabbCollisionWorld world = HubWorld(hub);
    Vec2 out{};
    // The mech is at (45,7.6). The former object_portal gap was x=48..50,
    // y=9..10. Try to walk straight through it, then diagonally into it.
    auto result = world.Move({47.0f, 9.5f}, {0.4f, 0.4f},
                             {5.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x < 48.0f);

    result = world.Move({47.5f, 8.4f}, {0.4f, 0.4f},
                        {3.0f, 3.0f}, out);
    CHECK(out.x < 50.0f);
    CHECK(out.y < 10.0f);

    // From inside the mech platform, walking up into the sealed wall must not
    // cross the y=9 wall line.
    result = world.Move({49.5f, 8.4f}, {0.4f, 0.4f},
                        {0.0f, 4.0f}, out);
    CHECK(result.blocked_y);
    CHECK(out.y < 9.0f);
}

TEST_CASE("Player cannot leave the garden tree-line floor footprint" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    AabbCollisionWorld world = HubWorld(hub);
    Vec2 out{};
    // The garden floor ends at the tree line around x=-16 and x=-63.
    auto result = world.Move({-17.5f, 0.5f}, {0.4f, 0.4f},
                             {3.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x < -16.0f);

    // Bottom tree line: moving down from the lowest garden row is blocked.
    result = world.Move({-30.5f, -17.5f}, {0.4f, 0.4f},
                        {0.0f, -4.0f}, out);
    CHECK(result.blocked_y);
}

TEST_CASE("Living Room uses the recovered material chest asset" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    bool chest_material_visual = false;
    bool generic_chest03 = false;
    for (const Json& instance : hub["instances"]) {
        const std::string path = instance.value("path", "");
        if (path.find("chest_material") != std::string::npos) {
            chest_material_visual = true;
        }
        if (path.find("chest03") != std::string::npos) generic_chest03 = true;
    }
    CHECK(chest_material_visual);
    CHECK_FALSE(generic_chest03);
    bool marker = false;
    for (const Json& item : hub["interaction_markers"]) {
        if (item.value("kind", "") == "material_chest") marker = true;
    }
    CHECK(marker);
}

TEST_CASE("Living Room wall collision covers the old mech portal tile cells" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    const auto& solids = hub["collision"]["solids"];
    bool left_cell = false;
    bool right_cell = false;
    for (const Json& solid : solids) {
        if (solid.size() != 4) continue;
        if (std::fabs(solid[0].get<float>() - 48.0f) < 0.01f &&
            std::fabs(solid[1].get<float>() - 9.0f) < 0.01f &&
            std::fabs(solid[2].get<float>() - 49.0f) < 0.01f &&
            std::fabs(solid[3].get<float>() - 10.0f) < 0.01f) {
            left_cell = true;
        }
        if (std::fabs(solid[0].get<float>() - 49.0f) < 0.01f &&
            std::fabs(solid[1].get<float>() - 9.0f) < 0.01f &&
            std::fabs(solid[2].get<float>() - 50.0f) < 0.01f &&
            std::fabs(solid[3].get<float>() - 10.0f) < 0.01f) {
            right_cell = true;
        }
    }
    CHECK(left_cell);
    CHECK(right_cell);
}


TEST_CASE("Reforging furnace is solid from every approach direction" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    AabbCollisionWorld world = HubWorld(hub);
    Vec2 out{};
    // Furnace collider: x 33.49..36.91, y -6.42..-3.03.
    auto result = world.Move({31.5f, -4.8f}, {0.4f, 0.4f},
                             {5.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x < 33.5f);
    result = world.Move({39.0f, -4.8f}, {0.4f, 0.4f},
                        {-5.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x > 36.9f);
    result = world.Move({34.8f, -7.6f}, {0.4f, 0.4f},
                        {0.0f, 5.0f}, out);
    CHECK(result.blocked_y);
    CHECK(out.y < -6.4f);
    result = world.Move({34.8f, -1.5f}, {0.4f, 0.4f},
                        {0.0f, -5.0f}, out);
    CHECK(result.blocked_y);
    CHECK(out.y > -3.05f);
}

TEST_CASE("Reforging furnace has the recovered wall backdrop" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    int body = 0;
    int cap = 0;
    for (const Json& instance : hub["instances"]) {
        const std::string path = instance.value("path", "");
        if (path == "hub/forge_wall_repair") ++body;
        if (path == "hub/forge_wall_cap_repair") ++cap;
    }
    CHECK(body == 10);
    CHECK(cap == 10);
}

TEST_CASE("Trader alcove has collision on all four sides" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    AabbCollisionWorld world = HubWorld(hub);
    Vec2 out{};
    // Recovered bookshelf/stall collider: x 23.10..32.04, y 3.35..7.00.
    auto result = world.Move({27.6f, 1.0f}, {0.4f, 0.4f},
                             {0.0f, 4.0f}, out);
    CHECK(result.blocked_y);
    CHECK(out.y < 3.36f);
    result = world.Move({27.6f, 9.2f}, {0.4f, 0.4f},
                        {0.0f, -4.0f}, out);
    CHECK(result.blocked_y);
    CHECK(out.y > 6.99f);
    result = world.Move({20.0f, 5.0f}, {0.4f, 0.4f},
                        {5.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x < 23.11f);
    result = world.Move({35.0f, 5.0f}, {0.4f, 0.4f},
                        {-5.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x > 32.03f);
}

TEST_CASE("Mech station and material chest are solid from every side" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    AabbCollisionWorld world = HubWorld(hub);
    Vec2 out{};
    // Mech: x 41.14..47.64, y 4.34..8.84.
    auto result = world.Move({43.0f, 2.0f}, {0.4f, 0.4f},
                             {0.0f, 4.0f}, out);
    CHECK(result.blocked_y);
    CHECK(out.y < 4.35f);
    result = world.Move({39.0f, 6.5f}, {0.4f, 0.4f},
                        {4.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x < 41.15f);
    result = world.Move({49.2f, 6.5f}, {0.4f, 0.4f},
                        {-4.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x > 47.63f);

    // Material chest: x 47.30..50.00, y -0.17..3.57.
    result = world.Move({48.8f, -2.0f}, {0.4f, 0.4f},
                        {0.0f, 4.0f}, out);
    CHECK(result.blocked_y);
    CHECK(out.y < -0.16f);
    result = world.Move({45.5f, 1.7f}, {0.4f, 0.4f},
                        {4.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x < 47.31f);
}

TEST_CASE("Recovered planter beds are solid and cannot be walked through" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    AabbCollisionWorld world = HubWorld(hub);
    Vec2 out{};
    // Upper left planter land_0 spans x -27.7..-23.8, y 2.08..4.57 and has
    // the reference land_root child collider around y 1.70..5.10.
    auto result = world.Move({-25.77f, -0.6f}, {0.4f, 0.4f},
                             {0.0f, 4.0f}, out);
    CHECK(result.blocked_y);
    CHECK(out.y < 1.70f);
    result = world.Move({-25.77f, 6.5f}, {0.4f, 0.4f},
                        {0.0f, -4.0f}, out);
    CHECK(result.blocked_y);
    CHECK(out.y > 5.09f);
    result = world.Move({-30.0f, 3.0f}, {0.4f, 0.4f},
                        {4.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x < -28.05f);
    result = world.Move({-21.5f, 3.0f}, {0.4f, 0.4f},
                        {-4.0f, 0.0f}, out);
    CHECK(result.blocked_x);
    CHECK(out.x > -23.46f);
}

TEST_CASE("No sign objects remain in the Living Room scene" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    for (const Json& instance : hub["instances"]) {
        const std::string path = instance.value("path", "");
        CHECK(path.find("sign") == std::string::npos);
        CHECK(path.find("Sign") == std::string::npos);
        CHECK(instance.value("sprite", "") != "hub_spr_2241");
        CHECK(path.find("task/board") == std::string::npos);
        CHECK(path.find("hire_board") == std::string::npos);
    }
    for (const Json& sprite : hub["sprites"]) {
        const std::string name = sprite.value("source_name", "");
        CHECK(name.find("sign") == std::string::npos);
        CHECK(name.find("Sign") == std::string::npos);
    }
}

TEST_CASE("Ground props use foot-based depth and tilemaps stay static" *
          doctest::test_suite("unit")) {
    const Json hub = LoadHub();
    bool saw_layered_prop = false;
    bool saw_floor_tile = false;
    for (const Json& instance : hub["instances"]) {
        const std::string path = instance.value("path", "");
        if (path.find("research/bookshelf/SpriteRenderer") !=
            std::string::npos) {
            CHECK(instance.value("ground_depth", false));
            saw_layered_prop = true;
        }
        if (path.find("room_tilemap/floor_") != std::string::npos) {
            CHECK_FALSE(instance.value("ground_depth", true));
            saw_floor_tile = true;
        }
    }
    CHECK(saw_layered_prop);
    CHECK(saw_floor_tile);
}

TEST_CASE("Player and elevated prop sort correctly by foot position" *
          doctest::test_suite("unit")) {
    using soulknight::engine::rendering::SpriteFootY;
    using soulknight::engine::rendering::WorldDepthOrder;
    // Bookshelf sprite is centre-pivoted at y=7.0 and 3.2 units tall.
    const float foot = SpriteFootY(7.0f, 3.2f, 0.5f);
    CHECK(foot == doctest::Approx(5.4f));
    const int32_t prop_order = WorldDepthOrder(foot);
    // South of the shelf: the player draws in front (higher order).
    CHECK(WorldDepthOrder(4.8f) > prop_order);
    // North of the shelf: the player draws behind it (lower order).
    CHECK(WorldDepthOrder(6.2f) < prop_order);
}
