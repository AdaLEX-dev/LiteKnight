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
    if (at == std::string::npos) return {0, 0};
    const std::string suffix = path.substr(at + 1);
    const std::size_t comma = suffix.find(',');
    if (comma == std::string::npos) return {0, 0};
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

bool OverlapsSpawn(float min_x, float min_y, float max_x, float max_y,
                   float spawn_x, float spawn_y) {
    constexpr float half = 0.4f;
    const float body_min_x = spawn_x - half;
    const float body_max_x = spawn_x + half;
    const float body_min_y = spawn_y + 0.5f - half;
    const float body_max_y = spawn_y + 0.5f + half;
    return body_max_x > min_x && body_min_x < max_x &&
           body_max_y > min_y && body_min_y < max_y;
}

} // namespace

TEST_CASE("r1_1 wall visuals correspond to collision cells and no wall sprite is offset" *
          doctest::test_suite("unit")) {
    const Json scene = LoadScene();
    const Json& collision = scene["collision"];
    const Json& instances = scene["instances"];

    std::set<Cell> wall_visual_cells;
    std::vector<Cell> wall_visual_instances;
    int door_instances = 0;
    int skin_marker_instances = 0;
    int player_instances = 0;
    int obstacle_instances = 0;
    int outside_map = 0;
    int misplaced_wall_instances = 0;
    for (const Json& instance : instances) {
        const std::string path = instance.value("path", "");
        const float px = instance["position"].at(0).get<float>();
        const float py = instance["position"].at(1).get<float>();
        if (px < 0.0f || px > 41.0f || py < 0.0f || py > 41.0f) {
            ++outside_map;
        }
        if (path.rfind("native_wall/", 0) == 0) {
            const Cell cell = ParseCellSuffix(path);
            wall_visual_cells.insert(cell);
            wall_visual_instances.push_back(cell);
            if (std::abs(px - static_cast<float>(cell.first)) > 1e-5f ||
                std::abs(py - static_cast<float>(cell.second)) > 1e-5f) {
                ++misplaced_wall_instances;
            }
        } else if (path.rfind("native_obstacle/", 0) == 0) {
            ++obstacle_instances;
        } else if (path.find("/wall_group/skin_") != std::string::npos) {
            ++skin_marker_instances;
        } else if (instance.contains("door")) {
            ++door_instances;
        } else if (path.rfind("c00/", 0) == 0) {
            ++player_instances;
        }
    }

    CHECK(instances.size() == 482);
    CHECK(skin_marker_instances == 0);
    CHECK(obstacle_instances == 0);
    CHECK(player_instances == 2);
    CHECK(door_instances == 60);
    CHECK(wall_visual_instances.size() == 420);
    CHECK(wall_visual_cells.size() == 140);
    CHECK(misplaced_wall_instances == 0);
    CHECK(outside_map == 0);

    std::set<Cell> wall_collision_cells;
    for (const Json& solid : collision["solids"]) {
        wall_collision_cells.insert(BoxCell(solid));
    }
    CHECK(wall_collision_cells.size() == 140);
    CHECK(wall_collision_cells == wall_visual_cells);
    CHECK(collision["solid_count"].get<int>() == 140);

    const Json& obstacles = scene["obstacles"];
    CHECK(obstacles["markers"].size() == 49);
    CHECK(obstacles["variants"].size() == 12);
    CHECK(obstacles.value("default_variant", -1) == 0);
    CHECK(obstacles["variants"].at(0).value("name", "") == "wall603");
    CHECK(obstacles["variants"].at(1).value("name", "") == "box04");

    std::set<Cell> marker_cells;
    for (const Json& marker : obstacles["markers"]) {
        const float mx = marker["position"].at(0).get<float>();
        const float my = marker["position"].at(1).get<float>();
        marker_cells.insert({static_cast<int>(std::lround(mx)),
                             static_cast<int>(std::lround(my))});
        CHECK(marker.value("obj_index", -1) == 0);
    }
    CHECK(marker_cells.size() == 49);

    // Every selectable recovered variant supplies visuals and, where the
    // prefab has a collider, a collider AABB relative to each marker.
    for (const Json& variant : obstacles["variants"]) {
        CAPTURE(variant.value("name", ""));
        CHECK_FALSE(variant["visuals"].empty());
        const Json& collider = variant["collider"];
        if (!collider.is_null()) {
            CHECK(collider["size"].at(0).get<float>() > 0.0f);
            CHECK(collider["size"].at(1).get<float>() > 0.0f);
        }
    }

    const Json& topology = scene["topology"];
    CHECK(topology["obstacle_markers"].size() == 49);
    CHECK(topology["obstacle_prefabs"].at(0).value("name", "") == "wall603");
    CHECK(topology["obstacle_solid_count"].get<int>() == 49);
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
        CHECK_FALSE(OverlapsSpawn(
            solid.at(0).get<float>(), solid.at(1).get<float>(),
            solid.at(2).get<float>(), solid.at(3).get<float>(),
            spawn_x, spawn_y));
    }

    const Json& obstacles = scene["obstacles"];
    const Json& default_collider = obstacles["variants"].at(0)["collider"];
    const float half_x = default_collider["size"].at(0).get<float>() * 0.5f;
    const float half_y = default_collider["size"].at(1).get<float>() * 0.5f;
    const float offset_x = default_collider["offset"].at(0).get<float>();
    const float offset_y = default_collider["offset"].at(1).get<float>();
    for (const Json& marker : obstacles["markers"]) {
        const float mx = marker["position"].at(0).get<float>();
        const float my = marker["position"].at(1).get<float>();
        CHECK_FALSE(OverlapsSpawn(
            mx + offset_x - half_x, my + offset_y - half_y,
            mx + offset_x + half_x, my + offset_y + half_y,
            spawn_x, spawn_y));
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

TEST_CASE("r1_1 carries recovered character assets and animation pairs" *
          doctest::test_suite("unit")) {
    const Json scene = LoadScene();
    const Json& characters = scene["characters"];
    CHECK(characters.size() == 15);
    CHECK(characters.at(0).value("id", "") == "c00");
    CHECK(characters.at(0).value("controller_name", "") == "char_0_skin_0");
    CHECK(characters.at(0).value("idle_animation", "") == "c00_idle");
    CHECK(characters.at(0).value("run_animation", "") == "c00_run");
    for (const Json& character : characters) {
        CAPTURE(character.value("id", ""));
        CHECK_FALSE(character.value("body_sprite", "").empty());
        CHECK_FALSE(character.value("idle_animation", "").empty());
        CHECK_FALSE(character.value("run_animation", "").empty());
    }

    // No recovered c00..c14 controller contains directional ide/run states:
    // each exposes the same ide/run/dead state hashes and relies on the
    // horizontal img mirror for facing.
    const Json& animations = scene["animations"];
    CHECK(animations.size() == 45);
}

TEST_CASE("r1_1 carries recovered player, weapon and enemy combat metadata" *
          doctest::test_suite("unit")) {
    const Json scene = LoadScene();
    const Json& player = scene["player_stats"];
    CHECK(player.value("character_id", "") == "c00");
    CHECK(player.value("max_hp", 0) == 6);
    CHECK(player.value("max_armor", 0) == 5);
    CHECK(player.value("camp", 0) == 1);
    CHECK(player.value("deviation", -1) == 0);

    CHECK(scene["weapons"].size() >= 15);
    CHECK(scene["room"].value("type", -1) == 1);
    CHECK(scene["room"].value("label", "") == "1-1");
    const Json& weapon = scene["weapons"].at(0);
    CHECK(weapon.value("id", "") == "weapon_000");
    CHECK(weapon.value("damage", 0) == 3);
    CHECK(weapon.value("deviation", 0) == 5);
    CHECK(weapon.value("owner_deviation", -1) == 0);
    CHECK(weapon.value("consume", -1) == 0);
    CHECK(weapon.value("attack_cycle_seconds", 0.0) ==
          doctest::Approx(1.0 / 3.0).epsilon(1e-5));
    CHECK(weapon.value("first_shot_delay_seconds", 0.0) ==
          doctest::Approx(1.0 / 30.0).epsilon(1e-5));
    for (const Json& entry : scene["weapons"]) {
        CAPTURE(entry.value("id", ""));
        CHECK_FALSE(entry.value("visual_sprite", "").empty());
        CHECK(entry.value("attack_cycle_seconds", 0.0) > 0.0);
        if (!entry.value("is_melee", false)) {
            const Json& projectile_entry = entry["projectile"];
            CHECK(projectile_entry.contains("sprite"));
            if (projectile_entry.contains("sprite") &&
                projectile_entry["sprite"].is_string()) {
                CHECK_FALSE(projectile_entry["sprite"].get<std::string>().empty());
            }
        }
    }
    const Json& projectile = weapon["projectile"];
    CHECK(projectile.value("id", "") == "bullet_0");
    CHECK(weapon.value("audio_clip", "") == "fx_gun_1");
    CHECK_FALSE(weapon.value("muzzle_sprite", "").empty());
    CHECK_FALSE(weapon["visual_position_x"].empty());
    CHECK(weapon["visual_position_x"].at(1)[1].get<float>() < -0.1f);
    CHECK(projectile.value("prefab_speed", 0.0) == doctest::Approx(10.0));
    CHECK(projectile.value("lifetime", 0.0) == doctest::Approx(5.0));

    CHECK(scene["enemy_definitions"].size() >= 80);
    bool found_slime = false;
    for (const Json& enemy : scene["enemy_definitions"]) {
        if (enemy.value("id", "") != "e_slime01") continue;
        found_slime = true;
        CHECK(enemy.value("hp", 0) == 12);
        CHECK(enemy.value("camp", 0) == 0);
        CHECK(enemy.value("contact_damage", 0) == 3);
        CHECK(enemy.value("contact_cooldown", 0.0) == doctest::Approx(1.5));
        CHECK(enemy.value("death_despawn_delay", 0.0) == doctest::Approx(4.0));
    }
    CHECK(found_slime);
    CHECK(scene["enemy_fixtures"].size() == 2);
}
