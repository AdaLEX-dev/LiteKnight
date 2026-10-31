// Soul Knight Linux — sprite, scene, camera and geometry tests
#include <doctest/doctest.h>

#include "engine/assets/SceneLoader.hpp"
#include "engine/assets/SpriteData.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "engine/locomotion/PlayerLocomotion.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/SpriteBatch.hpp"
#include "platform/filesystem/FileSystem.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <cmath>
#include <string>
#include <vector>

using soulknight::engine::Camera2D;
using soulknight::engine::Color;
using soulknight::engine::Rect;
using soulknight::engine::Vec2;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::SceneLoader;
using soulknight::engine::assets::SpriteData;
using soulknight::engine::collision::Aabb;
using soulknight::engine::collision::AabbCollisionWorld;
using soulknight::engine::collision::OneWayDirection;
using soulknight::engine::locomotion::PlayerLocomotion;
using soulknight::engine::rendering::BuildSpriteQuad;
using soulknight::engine::rendering::SortLayer;
using soulknight::engine::rendering::SpriteDraw;
using soulknight::engine::rendering::SpriteQuad;
using soulknight::platform::FileSystem;
using Json = nlohmann::json;

TEST_CASE("SpriteData validates rect, UV, pivot, PPU and border" * doctest::test_suite("unit")) {
    SpriteData sprite;
    sprite.id = "test";
    sprite.texture_id = 3;
    sprite.texture_width = 256;
    sprite.texture_height = 128;
    sprite.rect = {64.0f, 16.0f, 32.0f, 24.0f};
    sprite.pivot = {0.25f, 0.75f};
    sprite.pixels_per_unit = 16.0f;
    sprite.border = {1.0f, 2.0f, 3.0f, 4.0f};

    CHECK(sprite.IsValid());
    CHECK(sprite.UvRect().x == doctest::Approx(0.25f));
    CHECK(sprite.UvRect().y == doctest::Approx(0.125f));
    CHECK(sprite.UvRect().w == doctest::Approx(0.125f));
    CHECK(sprite.UvRect().h == doctest::Approx(0.1875f));
    CHECK(sprite.WorldSize().x == doctest::Approx(2.0f));
    CHECK(sprite.WorldSize().y == doctest::Approx(1.5f));
    CHECK(sprite.border[0] == doctest::Approx(1.0f));
    CHECK(sprite.border[3] == doctest::Approx(4.0f));

    sprite.pivot = {1.5f, 0.5f};
    CHECK_FALSE(sprite.IsValid());
    sprite.pivot = {0.5f, 0.5f};
    sprite.rect = {240.0f, 0.0f, 32.0f, 24.0f};
    CHECK_FALSE(sprite.IsValid());
    sprite.rect = {0.0f, 100.0f, 32.0f, 40.0f};
    CHECK_FALSE(sprite.IsValid());
}

TEST_CASE("Sprite geometry handles pivot, crop, rotation, flips and determinism" * doctest::test_suite("unit")) {
    SpriteDraw draw;
    draw.position = {5.0f, 5.0f};
    draw.size = {2.0f, 2.0f};
    draw.uv = {0.5f, 0.1f, 0.25f, 0.2f};
    draw.pivot = {0.5f, 0.5f};
    draw.color = Color::White();
    draw.layer = SortLayer::Character;

    const SpriteQuad center = BuildSpriteQuad(draw);
    CHECK(center.positions[0].x == doctest::Approx(4.0f));
    CHECK(center.positions[0].y == doctest::Approx(4.0f));
    CHECK(center.positions[1].x == doctest::Approx(6.0f));
    CHECK(center.positions[2].y == doctest::Approx(6.0f));
    CHECK(center.uvs[0].x == doctest::Approx(0.5f));
    CHECK(center.uvs[0].y == doctest::Approx(0.3f));
    CHECK(center.uvs[2].x == doctest::Approx(0.75f));
    CHECK(center.uvs[2].y == doctest::Approx(0.1f));

    draw.pivot = {0.0f, 0.0f};
    const SpriteQuad corner = BuildSpriteQuad(draw);
    CHECK(corner.positions[0].x == doctest::Approx(5.0f));
    CHECK(corner.positions[0].y == doctest::Approx(5.0f));
    CHECK(corner.positions[2].x == doctest::Approx(7.0f));
    CHECK(corner.positions[2].y == doctest::Approx(7.0f));

    draw.pivot = {0.5f, 0.5f};
    draw.rotation = 3.14159265358979323846f * 0.5f;
    const SpriteQuad rotated = BuildSpriteQuad(draw);
    CHECK(rotated.positions[0].x == doctest::Approx(6.0f));
    CHECK(rotated.positions[0].y == doctest::Approx(4.0f));
    CHECK(rotated.positions[2].x == doctest::Approx(4.0f));
    CHECK(rotated.positions[2].y == doctest::Approx(6.0f));

    draw.rotation = 0.0f;
    draw.flip_x = true;
    draw.flip_y = true;
    const SpriteQuad flipped = BuildSpriteQuad(draw);
    CHECK(flipped.uvs[0].x == doctest::Approx(0.75f));
    CHECK(flipped.uvs[0].y == doctest::Approx(0.1f));
    CHECK(flipped.uvs[3].x == doctest::Approx(0.75f));
    CHECK(flipped.uvs[3].y == doctest::Approx(0.3f));

    const SpriteQuad repeated = BuildSpriteQuad(draw);
    CHECK(repeated.positions == flipped.positions);
    CHECK(repeated.uvs == flipped.uvs);
}

TEST_CASE("Camera2D keeps world-to-screen and screen-to-world consistent" * doctest::test_suite("unit")) {
    Camera2D camera;
    camera.position = {20.0f, 20.0f};
    camera.orthographic_size = 10.0f;
    camera.viewport_width = 1920.0f;
    camera.viewport_height = 1080.0f;

    CHECK(camera.Aspect() == doctest::Approx(16.0f / 9.0f));
    CHECK(camera.HalfHeight() == doctest::Approx(10.0f));
    CHECK(camera.HalfWidth() == doctest::Approx(10.0f * 16.0f / 9.0f));

    const Vec2 center = camera.WorldToScreen({20.0f, 20.0f});
    CHECK(center.x == doctest::Approx(960.0f));
    CHECK(center.y == doctest::Approx(540.0f));
    const Vec2 ndc_center = camera.ViewProjection().TransformPoint({20.0f, 20.0f});
    CHECK(ndc_center.x == doctest::Approx(0.0f));
    CHECK(ndc_center.y == doctest::Approx(0.0f));

    const Vec2 world = {30.0f, 30.0f};
    const Vec2 screen = camera.WorldToScreen(world);
    CHECK(screen.x == doctest::Approx(1500.0f));
    CHECK(screen.y == doctest::Approx(0.0f));
    const Vec2 roundtrip = camera.ScreenToWorld(screen);
    CHECK(roundtrip.x == doctest::Approx(world.x));
    CHECK(roundtrip.y == doctest::Approx(world.y));

    camera.position = {21.0f, 19.0f};
    const Vec2 moved_center = camera.WorldToScreen(camera.position);
    CHECK(moved_center.x == doctest::Approx(960.0f));
    CHECK(moved_center.y == doctest::Approx(540.0f));

    camera.zoom = 2.0f;
    CHECK(camera.HalfHeight() == doctest::Approx(5.0f));
    CHECK(camera.HalfWidth() == doctest::Approx(5.0f * 16.0f / 9.0f));
}

TEST_CASE("SceneLoader reads native sprite and instance metadata" * doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());

    SceneAsset scene;
    std::string error;
    CHECK(SceneLoader::Load(filesystem, "test/scene_test.json", scene, &error));
    CHECK(error.empty());
    CHECK(scene.id == "synthetic_scene");
    CHECK(scene.textures.size() == 1);
    CHECK(scene.sprites.size() == 1);
    CHECK(scene.instances.size() == 1);
    CHECK(scene.textures[0].data.width == 4);
    CHECK(scene.textures[0].data.height == 4);
    CHECK(scene.textures[0].filter == soulknight::engine::assets::TextureFilter::Point);
    CHECK(scene.sprites[0].UvRect().w == doctest::Approx(1.0f));
    CHECK(scene.sprites[0].pixels_per_unit == doctest::Approx(4.0f));
    CHECK(scene.instances[0].sprite_index == 0);
    CHECK(scene.instances[0].position.x == doctest::Approx(1.0f));
    CHECK(scene.instances[0].position.y == doctest::Approx(2.0f));
    CHECK(scene.instances[0].color.g == 128);
    CHECK(scene.instances[0].sorting_layer == 5);
    CHECK(scene.instances[0].sorting_order == 2);
    CHECK(scene.instances[0].flip_x);
    CHECK(scene.instances[0].door == "south");

    CHECK(scene.has_room_topology);
    CHECK(scene.collision_bounds.x == doctest::Approx(-5.0f));
    CHECK(scene.collision_bounds.w == doctest::Approx(10.0f));
    CHECK(scene.interior_bounds.x == doctest::Approx(1.0f));
    CHECK(scene.interior_bounds.h == doctest::Approx(8.0f));
    CHECK(scene.collision_solids.size() == 1);
    CHECK(scene.collision_solids[0].min.x == doctest::Approx(4.0f));
    CHECK(scene.collision_solids[0].max.y == doctest::Approx(10.0f));
    CHECK(scene.obstacle_solids.size() == 1);
    CHECK(scene.obstacle_solids[0].min.x == doctest::Approx(2.0f));
    CHECK(scene.obstacle_solids[0].max.y == doctest::Approx(3.0f));
    CHECK(scene.doors.size() == 1);
    CHECK(scene.doors[0].id == "south");
    CHECK(scene.doors[0].aisle == "aisle_s");
    CHECK(scene.doors[0].direction == 1);
    CHECK(scene.doors[0].enter_exit_direction == 2);
    CHECK(scene.doors[0].enabled);
    CHECK(scene.doors[0].open);
    CHECK(scene.doors[0].barrier.min.x == doctest::Approx(3.0f));
    CHECK(scene.doors[0].trigger.max.y == doctest::Approx(6.0f));
    CHECK(scene.doors[0].one_way_block ==
          soulknight::engine::assets::DoorOneWayBlock::PositiveY);
    CHECK(scene.doors[0].open_animation.id == "synthetic_door_open");
    CHECK(scene.doors[0].open_animation.duration == doctest::Approx(0.2f));
    CHECK(scene.doors[0].open_animation.leaf_extent.Evaluate(0.0f) ==
          doctest::Approx(1.0f));
    CHECK(scene.doors[0].open_animation.leaf_extent.Evaluate(0.13333334f) ==
          doctest::Approx(0.1f));
    CHECK(scene.doors[0].open_animation.collider_trigger.EvaluateStep(0.13f) ==
          doctest::Approx(0.0f));
    CHECK(scene.doors[0].open_animation.collider_trigger.EvaluateStep(0.13333334f) ==
          doctest::Approx(1.0f));
    CHECK(scene.doors[0].close_animation.id == "synthetic_door_close");
    CHECK(scene.doors[0].close_animation.SpriteAt(0.02f) == "quad");
    CHECK(scene.doors[0].leaves.size() == 1);
    CHECK(scene.doors[0].leaves[0].name == "d1");
    CHECK(scene.doors[0].leaves[0].leaf_instance == 0);
    CHECK(scene.doors[0].leaves[0].shadow_instance == -1);
    CHECK(scene.instances[0].door_role == "leaf");
    CHECK(scene.instances[0].door_leaf == "d1");

    SceneAsset missing;
    std::string missing_error;
    CHECK_FALSE(SceneLoader::Load(filesystem, "test/missing_scene.json", missing, &missing_error));
    CHECK_FALSE(missing_error.empty());
}

TEST_CASE("Recovered r1_1 CreateWall and door topology match serialized evidence" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    const auto scene_text = filesystem.ReadTextFile("test/scenes/r1_1_visual.json");
    CHECK(scene_text.has_value());
    if (!scene_text.has_value()) return;
    const Json scene = Json::parse(*scene_text, nullptr, false);
    CHECK_FALSE(scene.is_discarded());
    if (scene.is_discarded()) return;
    CHECK(scene.contains("collision"));
    if (!scene.contains("collision")) return;
    const Json& collision = scene["collision"];

    const auto& bounds = collision["bounds"];
    CHECK(bounds.at(0).get<float>() == doctest::Approx(0.0f));
    CHECK(bounds.at(1).get<float>() == doctest::Approx(0.0f));
    CHECK(bounds.at(2).get<float>() == doctest::Approx(41.0f));
    CHECK(bounds.at(3).get<float>() == doctest::Approx(41.0f));
    const auto& interior = collision["interior_bounds"];
    CHECK(interior.at(0).get<float>() == doctest::Approx(12.5f));
    CHECK(interior.at(1).get<float>() == doctest::Approx(13.0f));
    CHECK(interior.at(2).get<float>() == doctest::Approx(15.0f));
    CHECK(interior.at(3).get<float>() == doctest::Approx(15.0f));

    // 44 solid room-wall cells + 96 aisle-corridor wall cells. The wall cells
    // exactly realize the CreateWall perimeter minus four five-cell openings.
    CHECK(collision["solid_count"].get<int>() == 140);
    CHECK(collision["solids"].size() == 140);

    const Json& doors = collision["doors"];
    CHECK(doors.size() == 4);
    if (doors.size() != 4) return;
    const auto find_door = [&doors](const std::string& id) -> const Json* {
        for (const Json& door : doors) {
            if (door.value("id", "") == id) return &door;
        }
        return nullptr;
    };

    struct DoorExpectation {
        const char* id;
        const char* aisle;
        int direction;
        int enter_exit_direction;
        float barrier[4];
        float trigger[4];
    };
    const DoorExpectation expectations[] = {
        {"south", "aisle_s", 1, 2, {17.5f, 12.0f, 22.5f, 13.0f},
         {17.5f, 13.4f, 22.5f, 14.9f}},
        {"north", "aisle_n", 3, 1, {17.5f, 28.0f, 22.5f, 29.0f},
         {17.5f, 26.1f, 22.5f, 27.6f}},
        {"west", "aisle_w", 2, 3, {11.5f, 18.0f, 12.5f, 23.0f},
         {12.9f, 18.0f, 14.4f, 23.0f}},
        {"east", "aisle_e", 0, 4, {27.5f, 18.0f, 28.5f, 23.0f},
         {25.6f, 18.0f, 27.1f, 23.0f}},
    };
    for (const DoorExpectation& expected : expectations) {
        CAPTURE(expected.id);
        const Json* door = find_door(expected.id);
        CHECK(door != nullptr);
        if (door == nullptr) continue;
        CHECK(door->value("aisle", "") == expected.aisle);
        CHECK(door->value("direction", -1) == expected.direction);
        CHECK(door->value("enter_exit_direction", -1) ==
              expected.enter_exit_direction);
        CHECK(door->value("enabled", false));
        CHECK(door->value("open", false));
        const std::string expected_block =
            std::string(expected.id) == "south" ? "negative_y" :
            std::string(expected.id) == "north" ? "positive_y" :
            std::string(expected.id) == "west" ? "negative_x" : "positive_x";
        CHECK(door->value("one_way_block", "") == expected_block);
        CHECK_FALSE(door->value("door_path", "").empty());
        CHECK_FALSE(door->value("trigger_path", "").empty());
        const auto& barrier = (*door)["barrier"];
        const auto& trigger = (*door)["trigger"];
        for (int i = 0; i < 4; ++i) {
            CHECK(barrier.at(static_cast<std::size_t>(i)).get<float>() ==
                  doctest::Approx(expected.barrier[i]));
            CHECK(trigger.at(static_cast<std::size_t>(i)).get<float>() ==
                  doctest::Approx(expected.trigger[i]));
        }
    }

    CHECK(scene["topology"]["wall_cell_count"].get<int>() == 44);
    CHECK(scene["topology"]["corridor_wall_cell_count"].get<int>() == 96);
    CHECK(scene["topology"]["floor_cell_count"].get<int>() == 225);
    CHECK(scene["topology"]["aisle_cell_count"].get<int>() == 240);
    CHECK(scene["topology"]["room_center"].at(0).get<float>() ==
          doctest::Approx(20.0f));
    CHECK(scene["topology"]["room_center"].at(1).get<float>() ==
          doctest::Approx(20.5f));
    CHECK(scene["topology"]["openings"].size() == 4);
    CHECK(scene["topology"]["wall_cells"].size() == 44);
    CHECK(scene["topology"]["corridor_wall_cells"].size() == 96);
    CHECK(scene["topology"]["floor_cells"].size() == 225);
    CHECK(scene["topology"]["aisle_cells"].size() == 240);
    const Json& wall_prefabs = scene["topology"]["wall_prefabs"];
    CHECK(wall_prefabs["runtime_layer"].get<int>() == 17);
    CHECK(wall_prefabs["variants"].size() == 2);
    CHECK(wall_prefabs["variants"][0].value("name", "") == "wall602");
    CHECK(wall_prefabs["variants"][1].value("name", "") == "wall601");
    for (const Json& variant : wall_prefabs["variants"]) {
        CHECK(variant["serialized_layer"].get<int>() == 13);
        CHECK(variant["runtime_layer"].get<int>() == 17);
        CHECK(variant["collider_size"].at(0).get<float>() ==
              doctest::Approx(1.0f));
        CHECK(variant["collider_size"].at(1).get<float>() ==
              doctest::Approx(1.0f));
        CHECK(variant["collider_offset"].at(1).get<float>() ==
              doctest::Approx(0.5f));
    }

    int wall_instances = 0;
    int door_instances = 0;
    for (const Json& instance : scene["instances"]) {
        const std::string path = instance.value("path", "");
        if (path.rfind("native_wall/", 0) == 0) ++wall_instances;
        if (instance.contains("door")) ++door_instances;
    }
    CHECK(wall_instances == 420); // 140 cells x root/shadow/wall-font sprites
    CHECK(door_instances == 60);  // four doors x five leaves x (leaf/shadow/d)
}

TEST_CASE("Recovered r1_1 wall and door barriers block, slide and pass" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    const auto scene_text = filesystem.ReadTextFile("test/scenes/r1_1_visual.json");
    CHECK(scene_text.has_value());
    if (!scene_text.has_value()) return;
    const Json scene = Json::parse(*scene_text, nullptr, false);
    CHECK_FALSE(scene.is_discarded());
    if (scene.is_discarded()) return;
    const Json& collision = scene["collision"];

    const auto build_world = [&collision](bool closed_doors) {
        AabbCollisionWorld world;
        const auto& bounds = collision["bounds"];
        world.SetBounds(Aabb{
            {bounds.at(0).get<float>(), bounds.at(1).get<float>()},
            {bounds.at(2).get<float>(), bounds.at(3).get<float>()}});
        for (const Json& solid : collision["solids"]) {
            world.AddSolid(Aabb{
                {solid.at(0).get<float>(), solid.at(1).get<float>()},
                {solid.at(2).get<float>(), solid.at(3).get<float>()}});
        }
        if (closed_doors) {
            for (const Json& door : collision["doors"]) {
                const Json& barrier = door["barrier"];
                const Aabb box{
                    {barrier.at(0).get<float>(), barrier.at(1).get<float>()},
                    {barrier.at(2).get<float>(), barrier.at(3).get<float>()}};
                const std::string block = door.value("one_way_block", "");
                if (block == "positive_x") {
                    world.AddOneWaySolid(box, OneWayDirection::PositiveX);
                } else if (block == "negative_x") {
                    world.AddOneWaySolid(box, OneWayDirection::NegativeX);
                } else if (block == "positive_y") {
                    world.AddOneWaySolid(box, OneWayDirection::PositiveY);
                } else if (block == "negative_y") {
                    world.AddOneWaySolid(box, OneWayDirection::NegativeY);
                } else {
                    world.AddSolid(box);
                }
            }
        }
        return world;
    };

    const auto run = [](const Vec2& start, const Vec2& direction, int ticks,
                        const AabbCollisionWorld& world) {
        PlayerLocomotion player(start);
        for (int i = 0; i < ticks; ++i) {
            player.FixedUpdate(direction, 1.0f, 0.02f, world);
        }
        return player;
    };

    // A confirmed solid perimeter cell at (14,12) stops downward motion.
    {
        const AabbCollisionWorld world = build_world(true);
        PlayerLocomotion player = run({14.0f, 13.4f}, {0.0f, -1.0f}, 20, world);
        CHECK(player.Position().x == doctest::Approx(14.0f));
        CHECK(player.Position().y == doctest::Approx(12.9f));
        CHECK(player.LastCollision().blocked_y);
        CHECK_FALSE(player.LastCollision().blocked_x);
    }

    // Down-right input slides along the same wall instead of stopping.
    {
        const AabbCollisionWorld world = build_world(true);
        const Vec2 diagonal{0.70710678f, -0.70710678f};
        PlayerLocomotion player = run({14.0f, 13.4f}, diagonal, 20, world);
        CHECK(player.Position().x > 15.0f);
        CHECK(player.Position().y == doctest::Approx(12.9f));
        CHECK(player.LastCollision().blocked_y);
    }

    struct PassageCase {
        const char* id;
        Vec2 start;
        Vec2 direction;
        bool x_axis;
        float closed_stop;
        float passed_threshold;
        bool passed_greater;
    };
    const PassageCase cases[] = {
        {"south", {20.0f, 14.0f}, {0.0f, -1.0f}, false, 12.9f, 12.0f, false},
        {"north", {20.0f, 27.0f}, {0.0f, 1.0f}, false, 27.1f, 29.0f, true},
        {"west", {13.0f, 20.0f}, {-1.0f, 0.0f}, true, 12.9f, 11.5f, false},
        {"east", {27.0f, 20.0f}, {1.0f, 0.0f}, true, 27.1f, 28.5f, true},
    };
    const AabbCollisionWorld closed_world = build_world(true);
    const AabbCollisionWorld open_world = build_world(false);
    for (const PassageCase& test : cases) {
        CAPTURE(test.id);
        PlayerLocomotion closed = run(test.start, test.direction, 40,
                                      closed_world);
        const float closed_value = test.x_axis ? closed.Position().x
                                               : closed.Position().y;
        CHECK(closed_value == doctest::Approx(test.closed_stop));
        CHECK((test.x_axis ? closed.LastCollision().blocked_x
                           : closed.LastCollision().blocked_y));

        PlayerLocomotion open = run(test.start, test.direction, 40, open_world);
        const float open_value = test.x_axis ? open.Position().x
                                             : open.Position().y;
        if (test.passed_greater) {
            CHECK(open_value > test.passed_threshold);
        } else {
            CHECK(open_value < test.passed_threshold);
        }
    }
}

TEST_CASE("Closed r1_1 doors are one-way inward across all four sides" *
          doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    const auto scene_text = filesystem.ReadTextFile("test/scenes/r1_1_visual.json");
    CHECK(scene_text.has_value());
    if (!scene_text.has_value()) return;
    const Json scene = Json::parse(*scene_text, nullptr, false);
    CHECK_FALSE(scene.is_discarded());
    if (scene.is_discarded()) return;
    const Json& collision = scene["collision"];

    AabbCollisionWorld world;
    const auto& bounds = collision["bounds"];
    world.SetBounds(Aabb{
        {bounds.at(0).get<float>(), bounds.at(1).get<float>()},
        {bounds.at(2).get<float>(), bounds.at(3).get<float>()}});
    for (const Json& solid : collision["solids"]) {
        world.AddSolid(Aabb{
            {solid.at(0).get<float>(), solid.at(1).get<float>()},
            {solid.at(2).get<float>(), solid.at(3).get<float>()}});
    }
    for (const Json& door : collision["doors"]) {
        const Json& barrier = door["barrier"];
        const Aabb box{
            {barrier.at(0).get<float>(), barrier.at(1).get<float>()},
            {barrier.at(2).get<float>(), barrier.at(3).get<float>()}};
        const std::string block = door.value("one_way_block", "");
        if (block == "positive_x") {
            world.AddOneWaySolid(box, OneWayDirection::PositiveX);
        } else if (block == "negative_x") {
            world.AddOneWaySolid(box, OneWayDirection::NegativeX);
        } else if (block == "positive_y") {
            world.AddOneWaySolid(box, OneWayDirection::PositiveY);
        } else if (block == "negative_y") {
            world.AddOneWaySolid(box, OneWayDirection::NegativeY);
        }
    }

    struct EntryCase {
        const char* id;
        Vec2 start;
        Vec2 direction;
        bool x_axis;
        float passed_threshold;
        bool passed_greater;
    };
    const EntryCase entries[] = {
        {"south", {20.0f, 11.0f}, {0.0f, 1.0f}, false, 13.0f, true},
        {"north", {20.0f, 30.0f}, {0.0f, -1.0f}, false, 27.0f, false},
        {"west", {11.0f, 20.0f}, {1.0f, 0.0f}, true, 13.0f, true},
        {"east", {29.0f, 20.0f}, {-1.0f, 0.0f}, true, 27.0f, false},
    };
    for (const EntryCase& test : entries) {
        CAPTURE(test.id);
        PlayerLocomotion player(test.start);
        for (int i = 0; i < 40; ++i) {
            player.FixedUpdate(test.direction, 1.0f, 0.02f, world);
        }
        const float value = test.x_axis ? player.Position().x
                                        : player.Position().y;
        if (test.passed_greater) {
            CHECK(value > test.passed_threshold);
        } else {
            CHECK(value < test.passed_threshold);
        }
    }
}
