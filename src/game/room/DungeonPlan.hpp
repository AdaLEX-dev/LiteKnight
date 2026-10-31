// Soul Knight Linux — deterministic native test-dungeon plan
//
// The reference MapManagerLevel.CreateMap/CreateRoom algorithm remains
// unrecovered (docs/PROCEDURAL_GENERATION.md). This planner therefore builds
// only the native three-room test layout from actual recovered room scenes. It
// uses the reconstructed RGRandom stream seeded by --map-seed so the same seed
// always produces the same assignment. It does not claim reference parity.
#pragma once

#include "RoomManager.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace soulknight::game::room {

struct DungeonRoomTemplate {
    std::string id;
    std::string scene_path;
};

struct DungeonPlan {
    std::int32_t seed = 0;
    std::vector<Room> rooms;
    std::vector<RoomConnection> connections;
    std::string start_room_id;
};

// Fixed physical topology (native test reconstruction):
//
//        second
//          |
//   hub -- first -- third
//
// Door sides are the actual recovered r1_N doors. The seed deterministically
// assigns the supplied room templates to the second/third/first slots.
DungeonPlan BuildDungeonPlan(std::int32_t seed,
                             std::vector<DungeonRoomTemplate> templates);

} // namespace soulknight::game::room
