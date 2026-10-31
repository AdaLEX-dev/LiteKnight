// Soul Knight Linux — recovered room visual/collision correspondence tests
#include <doctest/doctest.h>

#include "platform/filesystem/FileSystem.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

using soulknight::platform::FileSystem;
using Json = nlohmann::json;

namespace {

using Cell = std::pair<int, int>;

Cell ParseCellSuffix(const std::string& path) {
    const std::size_t at = path.rfind('@');
    CHECK(at != std::string::npos);
    const std::string suffix = path.substr(at + 1);
    const std::size_t comma = suffix.find(',');
    CHECK(comma != std::string::npos);
    return {std::stoi(suffix.substr(0, comma)),
            std::stoi(suffix.substr(comma + 1))};
}

Cell BoxCell(const Json& box) {
    const float min_x = box.at(0).get<float>();
    const float min_y = box.at(1).get<float>();
    const float max_x = box.at(2).get<float>();
    const float max_y = box.at(3).get<float>();
    const int x = static_cast<int>(std::lround((min_x + max_x) * 0.5f));
    const int y = static_cast<int>(std::lround((min_y + max_y) * 0.5f - 0.5f));
    return {x, y};
}

Json LoadScene() {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    const auto text = filesystem.ReadTextFile("test/scenes/r1_1_visual.json");
    CHECK(text.has_value());
    Json scene = Json::parse(text.value_or("{}"), nullptr, false);
    CHECK_FALSE(scene.is_discarded());
    return scene;
}

bool OverlapsSpawn(const Json& box, float spawn_x, float spawn_y) {
    constexpr float half = 0.4f;
    const float body_min_x = spawn_x - half;
    const float body_max_x = spawn_x + half;
    const float body_min_y = spawn_y + 0.5f - half;
    const float body_max_y = spawn_y + 0.5f + half;
    return body_max_x > box.at(0).get<float>() &&
           body_min_x < box.at(2).get<float>() &&
           body_max_y > box.at(1).get<float>() &&
           body_min_y < box.at(3).get<float>();
}

} // namespace

TEST_CASE("r1_1 wall and obstacle visuals correspond to recovered collision cells" *
          doctest::test_suite("unit")) {
    const Json scene = LoadScene();
    const Json& collision = scene["collision"];
    const Json& instances = scene["instances"];

    std::set<Cell> wall_visual_cells;
    std::set<Cell> obstacle_visual_cells;
    std::vector<Cell> wall_visual_instances;
    std::vector<Cell> obstacle_visual_instances;
    int door_instances = 0;
    int skin_marker_instances = 0;
    int player_instances = 0;
    for (const Json& instance : instances) {
        const std::string path = instance.value("path", "");
        if (path.rfind("native_wall/", 0) == 0) {
            const Cell cell = ParseCellSuffix(path);
            wall_visual_cells.insert(cell);
            wall_visual_instances.push_back(cell);
        } else if (path.rfind("native_obstacle/", 0) == 0) {
            const Cell cell = ParseCellSuffix(path);
            obstacle_visual_cells.insert(cell);
            obstacle_visual_instances.push_back(cell);
        } else if (path.find("/wall_group/skin_") != std::string::npos) {
            ++skin_marker_instances;
        } else if (instance.contains("door")) {
            ++door_instances;
        } else if (path.rfind("c00/", 0) == 0) {
            ++player_instances;
        }
    }

    CHECK(skin_marker_instances == 0);
    CHECK(player_instances == 2);
    CHECK(door_instances == 60);
    CHECK(wall_visual_instances.size() == 420);
    CHECK(wall_visual_cells.size() == 140);
    CHECK(obstacle_visual_instances.size() == 147);
    CHECK(obstacle_visual_cells.size() == 49);

    std::set<Cell> wall_collision_cells;
    for (const Json& solid : collision["solids"]) {
        wall_collision_cells.insert(BoxCell(solid));
    }
    CHECK(wall_collision_cells.size() == 140);
    CHECK(wall_collision_cells == wall_visual_cells);

    std::set<Cell> obstacle_collision_cells;
    for (const Json& solid : collision["obstacle_solids"]) {
        obstacle_collision_cells.insert(BoxCell(solid));
    }
    CHECK(obstacle_collision_cells.size() == 49);
    CHECK(obstacle_collision_cells == obstacle_visual_cells);
    CHECK(collision["solid_count"].get<int>() == 140);
    CHECK(collision["obstacle_solid_count"].get<int>() == 49);

    // The recovered obstacle replacement uses RGObjectSkin.obj_index 0 ->
    // obstacle_list[0] = wall603 for every r1_1 marker.
    const Json& topology = scene["topology"];
    CHECK(topology["obstacle_markers"].size() == 49);
    CHECK(topology["obstacle_prefabs"].at(0).value("name", "") == "wall603");
    CHECK(topology["obstacle_prefabs"].at(0)["collider_size"].at(0).get<float>() ==
          doctest::Approx(1.0f));
    CHECK(topology["obstacle_prefabs"].at(0)["collider_size"].at(1).get<float>() ==
          doctest::Approx(1.0f));
    CHECK(topology["obstacle_prefabs"].at(0)["collider_offset"].at(1).get<float>() ==
          doctest::Approx(0.5f));
    for (const Json& marker : topology["obstacle_markers"]) {
        CHECK(marker.value("prefab_name", "") == "wall603");
        CHECK(marker.value("obj_index", -1) == 0);
    }
}

TEST_CASE("r1_1 spawn and door bindings avoid the recovered obstacle block" *
          doctest::test_suite("unit")) {
    const Json scene = LoadScene();
    CHECK(scene["spawn"].at(0).get<float>() == doctest::Approx(20.0f));
    CHECK(scene["spawn"].at(1).get<float>() == doctest::Approx(15.0f));
    CHECK(scene.value("spawn_classification", std::string{}).find(
              "NATIVE TEST") != std::string::npos);

    const Json& collision = scene["collision"];
    const float spawn_x = scene["spawn"].at(0).get<float>();
    const float spawn_y = scene["spawn"].at(1).get<float>();
    for (const Json& solid : collision["solids"]) {
        CHECK_FALSE(OverlapsSpawn(solid, spawn_x, spawn_y));
    }
    for (const Json& solid : collision["obstacle_solids"]) {
        CHECK_FALSE(OverlapsSpawn(solid, spawn_x, spawn_y));
    }

    const Json& doors = collision["doors"];
    CHECK(doors.size() == 4);
    for (const Json& door : doors) {
        CAPTURE(door.value("id", ""));
        CHECK_FALSE(door.value("door_path", "").empty());
        CHECK(door.contains("open_animation"));
        CHECK(door.contains("close_animation"));
        CHECK(door["open_animation"].value("id", "") == "door_open");
        CHECK(door["open_animation"].value("duration", 0.0f) ==
              doctest::Approx(0.2f));
        const std::string close_id =
            door["close_animation"].value("id", "");
        if (door.value("id", "") == "south" ||
            door.value("id", "") == "north") {
            CHECK(close_id == "door_close2");
        } else {
            CHECK(close_id == "door_close");
        }
    }

    int leaves = 0;
    int shadows = 0;
    int ornaments = 0;
    for (const Json& instance : scene["instances"]) {
        const std::string role = instance.value("door_role", "");
        if (role == "leaf") ++leaves;
        if (role == "shadow") ++shadows;
        if (role == "ornament") ++ornaments;
        if (!role.empty()) {
            CHECK_FALSE(instance.value("door_leaf", "").empty());
        }
    }
    CHECK(leaves == 20);
    CHECK(shadows == 20);
    CHECK(ornaments == 20);
}
