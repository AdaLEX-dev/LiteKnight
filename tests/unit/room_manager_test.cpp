// Soul Knight Linux — room graph foundation tests
#include <doctest/doctest.h>

#include "game/room/RoomManager.hpp"

using soulknight::game::room::Room;
using soulknight::game::room::RoomConnection;
using soulknight::game::room::RoomManager;

TEST_CASE("Room manager allows only declared connections" *
          doctest::test_suite("unit")) {
    RoomManager rooms;
    rooms.Configure(
        {{"hero_room", "scenes/hero_room/scene.json"},
         {"r1_1_visual", "scenes/r1_1_visual/scene.json"}},
        {{"hero_room", "r1_1_visual", "bidirectional"}});
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
