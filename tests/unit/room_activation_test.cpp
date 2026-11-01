// Soul Knight Linux — room activation/door state regression
#include <doctest/doctest.h>

#include "game/room/RoomActivation.hpp"

#include <string>

using soulknight::engine::Vec2;
using soulknight::game::room::AdvanceRoomActivation;
using soulknight::game::room::RoomActivationName;
using soulknight::game::room::RoomActivationState;

TEST_CASE("Room activation waits at the entrance before combat" *
          doctest::test_suite("unit")) {
    const Vec2 entry{0.0f, 0.0f};
    auto state = AdvanceRoomActivation(
        RoomActivationState::Inactive, 0.0f, entry, {0.0f, 0.0f},
        true, false);
    CHECK(state == RoomActivationState::Entered);
    // Dormant at the doorway: no combat activation yet.
    state = AdvanceRoomActivation(
        state, 0.20f, entry, {0.4f, 0.2f}, true, false);
    CHECK(state == RoomActivationState::Entered);
    // Grace expires -> combat begins, doors close.
    state = AdvanceRoomActivation(
        state, 0.50f, entry, {0.4f, 0.2f}, true, false);
    CHECK(state == RoomActivationState::Combat);
    CHECK(std::string(RoomActivationName(state)) == "COMBAT");
    // Enemies alive keeps combat active.
    state = AdvanceRoomActivation(
        state, 1.0f, entry, {1.0f, 0.0f}, true, false);
    CHECK(state == RoomActivationState::Combat);
    // All enemies dead -> cleared, doors reopen.
    state = AdvanceRoomActivation(
        state, 1.0f, entry, {1.0f, 0.0f}, false, true);
    CHECK(state == RoomActivationState::Cleared);
    CHECK(std::string(RoomActivationName(state)) == "CLEARED");
}

TEST_CASE("Moving inward activates combat immediately" *
          doctest::test_suite("unit")) {
    auto state = AdvanceRoomActivation(
        RoomActivationState::Entered, 0.05f, {0.0f, 0.0f},
        {1.5f, 0.0f}, true, false);
    CHECK(state == RoomActivationState::Combat);
}

TEST_CASE("Already clear rooms never re-enter combat" *
          doctest::test_suite("unit")) {
    auto state = AdvanceRoomActivation(
        RoomActivationState::Inactive, 0.0f, {}, {}, false, false);
    CHECK(state == RoomActivationState::Cleared);
    state = AdvanceRoomActivation(
        state, 100.0f, {}, {10.0f, 10.0f}, true, false);
    CHECK(state == RoomActivationState::Cleared);
}
