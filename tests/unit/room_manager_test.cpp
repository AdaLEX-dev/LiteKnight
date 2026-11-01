// Soul Knight Linux — room graph foundation tests
#include <doctest/doctest.h>

#include "engine/assets/SceneAsset.hpp"
#include "game/room/DungeonPlan.hpp"
#include "game/room/RoomDoorController.hpp"
#include "game/room/RoomManager.hpp"

#include <queue>
#include <unordered_map>
#include <unordered_set>

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

TEST_CASE("Native dungeon plan follows the recovered MapManagerLevel graph" *
          doctest::test_suite("unit")) {
    const std::vector<DungeonRoomTemplate> templates{
        {"r1_1_visual", "scenes/r1_1_visual/scene.json"},
        {"r1_2_visual", "scenes/r1_2_visual/scene.json"},
        {"r1_3_visual", "scenes/r1_3_visual/scene.json"},
        {"r1_4_visual", "scenes/r1_4_visual/scene.json"},
        {"r1_5_visual", "scenes/r1_5_visual/scene.json"},
        {"r1_6_visual", "scenes/r1_6_visual/scene.json"},
    };
    const auto first = BuildDungeonPlan(12345, templates, 3);
    const auto repeat = BuildDungeonPlan(12345, templates, 3);
    CHECK(first.rooms.size() >= 5);
    CHECK(first.rooms.size() <= 6);
    CHECK(first.rooms[0].type == 0);
    CHECK(first.rooms[1].type == 1);
    CHECK(first.rooms[2].type == 1);
    CHECK(first.rooms[3].type == 2);
    CHECK(first.rooms[4].type == 3);
    if (first.rooms.size() > 5) CHECK(first.rooms[5].type == 4);
    CHECK(first.start_room_id == first.rooms[0].id);
    CHECK(first.connections.size() >= 4);
    for (const auto& connection : first.connections) {
        CHECK(connection.from != connection.to);
        CHECK_FALSE(connection.from_door.empty());
        CHECK_FALSE(connection.to_door.empty());
    }
    CHECK(first.rooms[0].id == repeat.rooms[0].id);
    CHECK(first.rooms[3].id == repeat.rooms[3].id);
    CHECK(first.start_room_id == repeat.start_room_id);
    CHECK(first.connections.size() == repeat.connections.size());
    const auto other = BuildDungeonPlan(54321, templates, 3);
    CHECK(other.rooms.size() >= 5);
    const bool differs = other.rooms[0].id != first.rooms[0].id ||
                         other.rooms[1].id != first.rooms[1].id ||
                         other.rooms[3].id != first.rooms[3].id;
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
        {"r1_4_visual", "scenes/r1_4_visual/scene.json"},
        {"r1_5_visual", "scenes/r1_5_visual/scene.json"},
        {"r1_6_visual", "scenes/r1_6_visual/scene.json"},
    };
    for (int seed = 0; seed < 1000; ++seed) {
        CAPTURE(seed);
        const auto plan = BuildDungeonPlan(seed, templates, 3);
        CHECK(plan.rooms.size() >= 5);
        CHECK(plan.rooms.size() <= 6);
        std::unordered_set<std::string> ids;
        for (const auto& room : plan.rooms) {
            CHECK(ids.insert(room.id).second);
            CHECK(room.type >= 0);
            CHECK(room.type <= 4);
            CHECK(room.map_x >= 0);
            CHECK(room.map_x < 5);
            CHECK(room.map_y >= 0);
            CHECK(room.map_y < 5);
        }
        std::unordered_set<std::string> visited;
        std::queue<std::string> pending;
        visited.insert(plan.start_room_id);
        pending.push(plan.start_room_id);
        while (!pending.empty()) {
            const std::string current = pending.front();
            pending.pop();
            for (const auto& connection : plan.connections) {
                std::string next;
                if (connection.from == current) next = connection.to;
                else if (connection.to == current) next = connection.from;
                if (!next.empty() && visited.insert(next).second) {
                    pending.push(next);
                }
            }
        }
        CHECK(visited.size() == plan.rooms.size());
        for (const auto& connection : plan.connections) {
            CHECK(ids.contains(connection.from));
            CHECK(ids.contains(connection.to));
            CHECK_FALSE(connection.from_door.empty());
            CHECK_FALSE(connection.to_door.empty());
        }
    }
}

TEST_CASE("Dungeon plan uses recovered start, end, chest and special rooms" *
          doctest::test_suite("unit")) {
    using soulknight::game::room::DungeonTemplateSet;
    DungeonTemplateSet set;
    set.starts = {{"start_visual", "scenes/start_visual/scene.json"}};
    for (int i = 1; i <= 6; ++i) {
        set.normals.push_back({"r1_" + std::to_string(i) + "_visual",
                               "scenes/r1_" + std::to_string(i) +
                                   "_visual/scene.json"});
    }
    set.ends = {{"end_visual", "scenes/end_visual/scene.json"}};
    set.chests = {{"chest_visual", "scenes/chest_visual/scene.json"}};
    set.specials = {{"statue_visual", "scenes/statue_visual/scene.json", 10},
                    {"mineral_visual", "scenes/mineral_visual/scene.json", 10}};
    bool found_six = false;
    for (int seed = 0; seed < 200 && !found_six; ++seed) {
        const auto plan = BuildDungeonPlan(seed, set, 3);
        if (plan.rooms.size() < 6) continue;
        found_six = true;
        CHECK(plan.rooms[0].id == "start_visual");
        CHECK(plan.rooms[0].type == 0);
        CHECK(plan.rooms[3].id == "end_visual");
        CHECK(plan.rooms[3].type == 2);
        CHECK(plan.rooms[4].id == "chest_visual");
        CHECK(plan.rooms[4].type == 3);
        CHECK((plan.rooms[5].id == "statue_visual" ||
               plan.rooms[5].id == "mineral_visual"));
        CHECK(plan.rooms[5].type == 4);
        CHECK(plan.start_room_id == "start_visual");
    }
    CHECK(found_six);
}

TEST_CASE("The 108 recovered PatternRoom pool stays unique and connected" *
          doctest::test_suite("unit")) {
    std::vector<DungeonRoomTemplate> templates;
    for (int index = 1; index <= 108; ++index) {
        const std::string id = "r1_" + std::to_string(index) + "_visual";
        templates.push_back({id, "scenes/" + id + "/scene.json"});
    }
    for (int seed = 0; seed < 1000; ++seed) {
        CAPTURE(seed);
        const auto plan = BuildDungeonPlan(seed, templates, 3);
        CHECK(plan.rooms.size() >= 5);
        CHECK(plan.rooms.size() <= 8);
        std::unordered_set<std::string> rooms;
        for (const auto& room : plan.rooms) {
            CHECK(rooms.insert(room.id).second);
            CHECK(room.id.rfind("r1_", 0) == 0);
            CHECK(room.map_x >= 0);
            CHECK(room.map_x < 5);
            CHECK(room.map_y >= 0);
            CHECK(room.map_y < 5);
        }
        std::unordered_map<std::string, std::vector<std::string>> graph;
        for (const auto& connection : plan.connections) {
            graph[connection.from].push_back(connection.to);
            graph[connection.to].push_back(connection.from);
        }
        std::unordered_set<std::string> visited;
        std::queue<std::string> pending;
        pending.push(plan.start_room_id);
        visited.insert(plan.start_room_id);
        while (!pending.empty()) {
            const std::string current = pending.front();
            pending.pop();
            for (const std::string& next : graph[current]) {
                if (visited.insert(next).second) pending.push(next);
            }
        }
        CHECK(visited.size() == plan.rooms.size());
        // Normal templates are never reused in one generated map.
        std::unordered_set<std::string> normals;
        for (const auto& room : plan.rooms) {
            if (room.type == 1) CHECK(normals.insert(room.id).second);
        }
    }
}
