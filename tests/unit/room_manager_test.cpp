// Soul Knight Linux — room graph foundation tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "game/room/DungeonPlan.hpp"
#include "game/room/RoomDoorController.hpp"
#include "game/room/RoomManager.hpp"

using soulknight::game::room::BuildDungeonPlan;
using soulknight::game::room::RoomDoorController;
using soulknight::game::room::DungeonRoomTemplate;
using soulknight::game::room::Room;
using soulknight::game::room::RoomConnection;
using soulknight::game::room::RoomManager;

TEST_CASE("Room manager allows only declared connections" *
          doctest::test_suite("unit")) {
    RoomManager rooms;
    rooms.Configure(
        {{"hero_room", "scenes/hero_room/scene.json"},
         {"r1_1_visual", "scenes/r1_1_visual/scene.json"}},
        {{"hero_room", "r1_1_visual", "bidirectional", "", ""}});
    CHECK(rooms.Current() != nullptr);
    CHECK(rooms.Current()->id == "hero_room");
    CHECK(rooms.CanTransition("r1_1_visual"));
    CHECK_FALSE(rooms.CanTransition("unknown_room"));
    CHECK(rooms.TransitionTo("r1_1_visual"));
    CHECK(rooms.Current()->id == "r1_1_visual");
    CHECK(rooms.CanTransition("hero_room"));
    CHECK(rooms.TransitionTo("hero_room"));
    CHECK(rooms.TransitionCount() == 2);
    CHECK(rooms.LastTransition().from == "r1_1_visual");
    CHECK(rooms.LastTransition().to == "hero_room");
}

TEST_CASE("Room manager rejects undeclared/empty configurations" *
          doctest::test_suite("unit")) {
    RoomManager rooms;
    CHECK(rooms.Current() == nullptr);
    CHECK_FALSE(rooms.TransitionTo("anything"));
    rooms.Configure({{"a", "a.json"}, {"b", "b.json"}}, {});
    CHECK_FALSE(rooms.TransitionTo("b"));
    CHECK(rooms.Current()->id == "a");
    CHECK(rooms.Find("b") != nullptr);
}

TEST_CASE("Physical door transitions select the matching destination door" *
          doctest::test_suite("unit")) {
    RoomManager rooms;
    rooms.Configure(
        {{"r1_1_visual", "scenes/r1_1_visual/scene.json"},
         {"r1_2_visual", "scenes/r1_2_visual/scene.json"}},
        {{"r1_1_visual", "r1_2_visual", "bidirectional", "north", "south"}});
    CHECK(rooms.ConnectionFromDoor("north") != nullptr);
    std::string destination_door;
    CHECK(rooms.TransitionThroughDoor("north", &destination_door));
    CHECK(rooms.Current()->id == "r1_2_visual");
    CHECK(destination_door == "south");
    CHECK(rooms.TransitionThroughDoor("south", &destination_door));
    CHECK(rooms.Current()->id == "r1_1_visual");
    CHECK(destination_door == "north");
    CHECK_FALSE(rooms.TransitionThroughDoor("east", &destination_door));
}

TEST_CASE("Native dungeon plan is deterministic per recovered RNG seed" *
          doctest::test_suite("unit")) {
    const std::vector<DungeonRoomTemplate> templates{
        {"r1_1_visual", "scenes/r1_1_visual/scene.json"},
        {"r1_2_visual", "scenes/r1_2_visual/scene.json"},
        {"r1_3_visual", "scenes/r1_3_visual/scene.json"},
    };
    const auto first = BuildDungeonPlan(12345, templates);
    const auto repeat = BuildDungeonPlan(12345, templates);
    CHECK(first.rooms.size() == 3);
    CHECK(first.connections.size() == 2);
    CHECK(first.rooms[0].id == repeat.rooms[0].id);
    CHECK(first.rooms[1].id == repeat.rooms[1].id);
    CHECK(first.rooms[2].id == repeat.rooms[2].id);
    CHECK(first.start_room_id == repeat.start_room_id);
    CHECK(first.connections[0].from_door == "north");
    CHECK(first.connections[0].to_door == "south");
    CHECK(first.connections[1].from_door == "east");
    CHECK(first.connections[1].to_door == "west");
    CHECK(first.connections[0].from == first.start_room_id);
    CHECK(first.connections[1].from == first.start_room_id);
    // Different seeds exercise a different assignment without fabricating rooms.
    const auto other = BuildDungeonPlan(54321, templates);
    CHECK(other.rooms.size() == 3);
    const bool differs = other.rooms[0].id != first.rooms[0].id ||
                         other.rooms[1].id != first.rooms[1].id ||
                         other.rooms[2].id != first.rooms[2].id;
    CHECK(differs);
}

TEST_CASE("Doors without a map edge stay sealed and block the room boundary" *
          doctest::test_suite("unit")) {
    soulknight::engine::assets::SceneAsset scene;
    for (const char* id : {"north", "east"}) {
        soulknight::engine::assets::SceneDoor door;
        door.id = id;
        door.enabled = true;
        door.open = true;
        door.open_animation.duration = 0.1f;
        door.close_animation.duration = 0.1f;
        scene.doors.push_back(std::move(door));
    }
    RoomDoorController doors;
    std::string error;
    CHECK(doors.Configure(scene, &error));
    CHECK_FALSE(doors.IsSealed(0));
    CHECK_FALSE(doors.IsSealed(1));
    doors.SealDoor(1);
    CHECK(doors.IsSealed(1));
    (void)doors.Update(0.2f);
    doors.OpenAll();
    (void)doors.Update(0.2f);
    CHECK(doors.At(0).State() == soulknight::game::room::DoorState::Open);
    CHECK(doors.At(1).State() == soulknight::game::room::DoorState::Closed);
    CHECK_FALSE(doors.At(0).IsBlocking());
    CHECK(doors.At(1).IsBlocking());
    doors.SealDoor(99);
    CHECK_FALSE(doors.IsSealed(99));
}

TEST_CASE("Generated room plans stay valid across many seeds" *
          doctest::test_suite("unit")) {
    const std::vector<DungeonRoomTemplate> templates{
        {"r1_1_visual", "scenes/r1_1_visual/scene.json"},
        {"r1_2_visual", "scenes/r1_2_visual/scene.json"},
        {"r1_3_visual", "scenes/r1_3_visual/scene.json"},
    };
    for (int seed = 0; seed < 100; ++seed) {
        CAPTURE(seed);
        const auto plan = BuildDungeonPlan(seed, templates);
        CHECK(plan.rooms.size() == 3);
        CHECK(plan.connections.size() == 2);
        CHECK(plan.rooms[0].id != plan.rooms[1].id);
        CHECK(plan.rooms[0].id != plan.rooms[2].id);
        CHECK(plan.rooms[1].id != plan.rooms[2].id);
        CHECK(plan.start_room_id == plan.rooms[0].id);
        for (const auto& connection : plan.connections) {
            CHECK(connection.from == plan.start_room_id);
            CHECK_FALSE(connection.from_door.empty());
            CHECK_FALSE(connection.to_door.empty());
        }
    }
}
