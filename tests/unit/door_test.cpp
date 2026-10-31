// Soul Knight Linux — recovered door animation and lifecycle tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "engine/locomotion/PlayerLocomotion.hpp"
#include "game/room/Door.hpp"
#include "game/room/RoomDoorController.hpp"
#include "platform/filesystem/FileSystem.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

using soulknight::engine::collision::AabbCollisionWorld;
using soulknight::engine::collision::OneWayDirection;
using soulknight::engine::locomotion::PlayerLocomotion;
using soulknight::engine::assets::DoorAnimationClip;
using soulknight::engine::assets::DoorCurve;
using soulknight::engine::assets::DoorOneWayBlock;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::SceneDoor;
using soulknight::game::room::Door;
using soulknight::game::room::DoorState;
using soulknight::game::room::DoorStateName;
using soulknight::game::room::DoorVisualState;
using soulknight::game::room::RoomDoorController;
using soulknight::platform::FileSystem;
using Json = nlohmann::json;

namespace {

DoorCurve ParseCurve(const Json& value) {
    DoorCurve curve;
    if (!value.is_array()) return curve;
    for (const Json& item : value) {
        curve.keys.push_back(soulknight::engine::assets::DoorCurveKey{
            item.at(0).get<float>(), item.at(1).get<float>()});
    }
    return curve;
}

DoorAnimationClip ParseClip(const Json& value) {
    DoorAnimationClip clip;
    clip.id = value.value("id", std::string{});
    clip.duration = value.value("duration", 0.0f);
    clip.sample_rate = value.value("sample_rate", 0.0f);
    clip.loop = value.value("loop", false);
    clip.leaf_extent = ParseCurve(value["leaf_extent"]);
    clip.shadow_visible = ParseCurve(value["shadow_visible"]);
    clip.ornament_visible = ParseCurve(value["ornament_visible"]);
    clip.collider_trigger = ParseCurve(value["collider_trigger"]);
    clip.collider_size_x = ParseCurve(value["collider_size_x"]);
    clip.collider_size_y = ParseCurve(value["collider_size_y"]);
    if (value.contains("leaf_sprites")) {
        for (const Json& item : value["leaf_sprites"]) {
            clip.leaf_sprites.push_back(soulknight::engine::assets::DoorSpriteKey{
                item.at(0).get<float>(), item.at(1).get<std::string>()});
        }
    }
    return clip;
}

SceneDoor ParseDoor(const Json& value) {
    SceneDoor door;
    door.id = value.value("id", std::string{});
    door.aisle = value.value("aisle", std::string{});
    door.enabled = value.value("enabled", false);
    door.open = value.value("open", false);
    const Json& barrier = value["barrier"];
    const Json& trigger = value["trigger"];
    door.barrier.min = {barrier.at(0).get<float>(),
                        barrier.at(1).get<float>()};
    door.barrier.max = {barrier.at(2).get<float>(),
                        barrier.at(3).get<float>()};
    door.trigger.min = {trigger.at(0).get<float>(),
                        trigger.at(1).get<float>()};
    door.trigger.max = {trigger.at(2).get<float>(),
                        trigger.at(3).get<float>()};
    const std::string block = value.value("one_way_block", "none");
    door.one_way_block =
        block == "positive_x" ? DoorOneWayBlock::PositiveX :
        block == "negative_x" ? DoorOneWayBlock::NegativeX :
        block == "positive_y" ? DoorOneWayBlock::PositiveY :
        block == "negative_y" ? DoorOneWayBlock::NegativeY :
                                DoorOneWayBlock::None;
    door.open_animation = ParseClip(value["open_animation"]);
    door.close_animation = ParseClip(value["close_animation"]);
    return door;
}

std::vector<SceneDoor> LoadRecoveredDoors() {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());
    const auto text = filesystem.ReadTextFile("test/scenes/r1_1_visual.json");
    CHECK(text.has_value());
    const Json scene = Json::parse(*text, nullptr, false);
    CHECK_FALSE(scene.is_discarded());
    std::vector<SceneDoor> doors;
    for (const Json& item : scene["collision"]["doors"]) {
        doors.push_back(ParseDoor(item));
    }
    return doors;
}

void CheckDoorLifecycle(Door& door) {
    CHECK(door.State() == DoorState::Open);
    CHECK_FALSE(door.IsBlocking());
    DoorVisualState visuals = door.Visuals();
    CHECK(visuals.leaf_sprite_id.rfind("door03_", 0) == 0);
    CHECK_FALSE(visuals.shadow_visible);
    CHECK_FALSE(visuals.ornament_visible);
    CHECK(visuals.ornament_extent == doctest::Approx(0.1f));

    door.Close();
    CHECK(door.State() == DoorState::Closing);
    CHECK(door.IsBlocking());
    visuals = door.Visuals();
    CHECK(visuals.leaf_sprite_id.rfind("door03_", 0) == 0);
    CHECK_FALSE(visuals.shadow_visible);
    CHECK_FALSE(visuals.ornament_visible);

    door.Update(0.02f);
    CHECK(door.State() == DoorState::Closing);
    CHECK(door.IsBlocking());
    CHECK_FALSE(door.Visuals().shadow_visible);

    door.Update(0.02f);
    CHECK(door.State() == DoorState::Closing);
    CHECK(door.IsBlocking());
    visuals = door.Visuals();
    CHECK(visuals.shadow_visible);
    CHECK(visuals.ornament_visible);
    CHECK(visuals.leaf_sprite_id.rfind("door02_", 0) == 0);
    CHECK(visuals.ornament_extent > 0.1f);
    CHECK(visuals.ornament_extent < 1.0f);

    door.Update(0.2f);
    CHECK(door.State() == DoorState::Closed);
    CHECK(door.IsBlocking());
    visuals = door.Visuals();
    CHECK(visuals.leaf_sprite_id.rfind("door02_", 0) == 0);
    CHECK(visuals.shadow_visible);
    CHECK(visuals.ornament_visible);
    CHECK(visuals.ornament_extent == doctest::Approx(1.0f));

    door.Open();
    CHECK(door.State() == DoorState::Opening);
    CHECK(door.IsBlocking());
    CHECK(door.Visuals().ornament_extent == doctest::Approx(1.0f));

    door.Update(0.12f);
    CHECK(door.State() == DoorState::Opening);
    CHECK(door.IsBlocking());

    door.Update(0.02f);
    CHECK(door.State() == DoorState::Opening);
    CHECK_FALSE(door.IsBlocking());
    visuals = door.Visuals();
    CHECK_FALSE(visuals.shadow_visible);
    CHECK(visuals.ornament_visible);
    CHECK(visuals.leaf_sprite_id.rfind("door02_", 0) == 0);

    door.Update(0.03f);
    CHECK_FALSE(door.IsBlocking());
    visuals = door.Visuals();
    CHECK(visuals.leaf_sprite_id.rfind("door03_", 0) == 0);
    CHECK_FALSE(visuals.ornament_visible);

    door.Update(0.2f);
    CHECK(door.State() == DoorState::Open);
    CHECK_FALSE(door.IsBlocking());
    CHECK(door.Visuals().ornament_extent == doctest::Approx(0.1f));
}

} // namespace

TEST_CASE("Recovered r1_1 door clips match the serialized Animator data" *
          doctest::test_suite("unit")) {
    const std::vector<SceneDoor> doors = LoadRecoveredDoors();
    CHECK(doors.size() == 4);
    for (const SceneDoor& door : doors) {
        CAPTURE(door.id);
        const DoorAnimationClip& open = door.open_animation;
        CHECK(open.id == "door_open");
        CHECK(open.duration == doctest::Approx(0.2f));
        CHECK(open.sample_rate == doctest::Approx(30.0f));
        CHECK_FALSE(open.loop);
        CHECK(open.leaf_extent.keys.size() == 2);
        CHECK(open.leaf_extent.keys[0].time == doctest::Approx(0.0f));
        CHECK(open.leaf_extent.keys[0].value == doctest::Approx(1.0f));
        CHECK(open.leaf_extent.keys[1].time == doctest::Approx(0.13333334f));
        CHECK(open.leaf_extent.keys[1].value == doctest::Approx(0.1f));
        CHECK(open.shadow_visible.EvaluateStep(0.13333334f) ==
              doctest::Approx(0.0f));
        CHECK(open.ornament_visible.EvaluateStep(0.16666667f) ==
              doctest::Approx(0.0f));
        CHECK(open.collider_trigger.EvaluateStep(0.0f) == doctest::Approx(0.0f));
        CHECK(open.collider_trigger.EvaluateStep(0.13333334f) ==
              doctest::Approx(1.0f));
        CHECK(open.SpriteAt(0.0f).rfind("door02_", 0) == 0);
        CHECK(open.SpriteAt(0.16666667f).rfind("door03_", 0) == 0);

        const DoorAnimationClip& close = door.close_animation;
        CHECK(close.sample_rate == doctest::Approx(30.0f));
        CHECK(close.duration == doctest::Approx(0.16666667f));
        CHECK_FALSE(close.loop);
        CHECK(close.leaf_extent.Evaluate(0.0f) == doctest::Approx(0.1f));
        CHECK(close.leaf_extent.Evaluate(0.16666667f) ==
              doctest::Approx(1.0f));
        CHECK(close.shadow_visible.EvaluateStep(0.0f) == doctest::Approx(0.0f));
        CHECK(close.shadow_visible.EvaluateStep(0.03333334f) ==
              doctest::Approx(1.0f));
        CHECK(close.collider_trigger.EvaluateStep(0.16666667f) ==
              doctest::Approx(0.0f));
        CHECK(close.SpriteAt(0.0f).rfind("door03_", 0) == 0);
        CHECK(close.SpriteAt(0.03333334f).rfind("door02_", 0) == 0);

        if (door.id == "south" || door.id == "north") {
            CHECK(close.id == "door_close2");
            CHECK_FALSE(close.collider_size_y.Empty());
            CHECK(close.collider_size_y.Evaluate(0.0f) ==
                  doctest::Approx(1.0f));
            CHECK(close.collider_size_x.Empty());
        } else {
            CHECK(close.id == "door_close");
            CHECK(close.collider_size_x.keys.size() == 2);
            CHECK(close.collider_size_x.keys[0].value ==
                  doctest::Approx(0.2f));
            CHECK(close.collider_size_x.keys[1].value ==
                  doctest::Approx(1.0f));
            CHECK(close.collider_size_y.Empty());
        }
    }
}

TEST_CASE("Door lifecycle exposes OPEN CLOSING CLOSED OPENING with animation-synchronized blocking" *
          doctest::test_suite("unit")) {
    const std::vector<SceneDoor> doors = LoadRecoveredDoors();
    CHECK(doors.size() == 4);
    for (const SceneDoor& data : doors) {
        CAPTURE(data.id);
        CHECK(std::string(DoorStateName(DoorState::Open)) == "OPEN");
        CHECK(std::string(DoorStateName(DoorState::Closing)) == "CLOSING");
        CHECK(std::string(DoorStateName(DoorState::Closed)) == "CLOSED");
        CHECK(std::string(DoorStateName(DoorState::Opening)) == "OPENING");
        Door door(data);
        CheckDoorLifecycle(door);

        // A close request during opening restarts the recovered close clip and
        // immediately restores the solid barrier.
        door.Close();
        CHECK(door.State() == DoorState::Closing);
        CHECK(door.IsBlocking());
        CHECK(door.Visuals().ornament_extent == doctest::Approx(0.1f));
    }
}

TEST_CASE("RoomDoorController drives collision from door animation across all orientations" *
          doctest::test_suite("unit")) {
    SceneAsset scene;
    scene.doors = LoadRecoveredDoors();
    CHECK(scene.doors.size() == 4);

    RoomDoorController controller;
    std::string error;
    CHECK(controller.Configure(scene, &error));
    CHECK(error.empty());
    CHECK(controller.Count() == 4);
    CHECK(controller.TargetOpen());

    AabbCollisionWorld world;
    controller.AddColliders(world);
    CHECK(world.OneWaySolids().empty());

    controller.CloseAll();
    CHECK_FALSE(controller.TargetOpen());
    for (std::size_t i = 0; i < controller.Count(); ++i) {
        CHECK(controller.At(i).State() == DoorState::Closing);
        CHECK(controller.At(i).IsBlocking());
    }
    world.ClearSolids();
    controller.AddColliders(world);
    CHECK(world.OneWaySolids().size() == 4);
    struct ExpectedDirection {
        const char* id;
        OneWayDirection direction;
    };
    const ExpectedDirection expected[] = {
        {"north", OneWayDirection::PositiveY},
        {"south", OneWayDirection::NegativeY},
        {"east", OneWayDirection::PositiveX},
        {"west", OneWayDirection::NegativeX},
    };
    for (const ExpectedDirection& entry : expected) {
        bool found = false;
        for (const auto& solid : world.OneWaySolids()) {
            if (solid.blocked_direction == entry.direction) found = true;
        }
        CAPTURE(entry.id);
        CHECK(found);
    }

    for (int i = 0; i < 9; ++i) {
        controller.Update(0.02f);
    }
    for (std::size_t i = 0; i < controller.Count(); ++i) {
        CHECK(controller.At(i).State() == DoorState::Closed);
        CHECK(controller.At(i).IsBlocking());
    }

    controller.OpenAll();
    CHECK(controller.TargetOpen());
    bool collision_changed = false;
    for (int i = 0; i < 6; ++i) {
        collision_changed = controller.Update(0.02f) || collision_changed;
    }
    CHECK_FALSE(collision_changed);
    for (std::size_t i = 0; i < controller.Count(); ++i) {
        CHECK(controller.At(i).IsBlocking());
    }
    world.ClearSolids();
    controller.AddColliders(world);
    CHECK(world.OneWaySolids().size() == 4);

    // t=0.14 crosses the recovered 0.133333 m_IsTrigger key.
    collision_changed = controller.Update(0.02f);
    CHECK(collision_changed);
    for (std::size_t i = 0; i < controller.Count(); ++i) {
        CHECK_FALSE(controller.At(i).IsBlocking());
    }
    world.ClearSolids();
    controller.AddColliders(world);
    CHECK(world.OneWaySolids().empty());

    controller.ToggleAll();
    CHECK_FALSE(controller.TargetOpen());
    for (std::size_t i = 0; i < controller.Count(); ++i) {
        CHECK(controller.At(i).State() == DoorState::Closing);
    }
}

TEST_CASE("Animated south door blocks exit only while collision is solid" *
          doctest::test_suite("unit")) {
    SceneAsset scene;
    scene.doors = LoadRecoveredDoors();
    CHECK(scene.doors.size() == 4);

    RoomDoorController controller;
    std::string error;
    CHECK(controller.Configure(scene, &error));
    CHECK(error.empty());

    AabbCollisionWorld world;
    world.SetBounds({{0.0f, 0.0f}, {41.0f, 41.0f}});
    controller.CloseAll();
    controller.AddColliders(world);

    // Only the south barrier is between y=12 and y=13 at x=20.
    PlayerLocomotion player({20.0f, 14.0f});
    for (int i = 0; i < 40; ++i) {
        player.FixedUpdate({0.0f, -1.0f}, 1.0f, 0.02f, world);
    }
    CHECK(player.Position().y == doctest::Approx(12.9f));
    CHECK(player.LastCollision().blocked_y);

    // The recovered opening clip makes m_IsTrigger true at 0.133333 s.
    controller.OpenAll();
    for (int i = 0; i < 7; ++i) {
        controller.Update(0.02f);
    }
    for (std::size_t i = 0; i < controller.Count(); ++i) {
        if (controller.At(i).Data().id == "south") {
            CHECK_FALSE(controller.At(i).IsBlocking());
        }
    }
    world.ClearSolids();
    controller.AddColliders(world);
    for (int i = 0; i < 40; ++i) {
        player.FixedUpdate({0.0f, -1.0f}, 1.0f, 0.02f, world);
    }
    CHECK(player.Position().y < 12.0f);
}
