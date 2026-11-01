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
    // Recovered ObjectDistribution weight (special rooms: 10/10/10/10/4 for
    // 1-1). Unweighted normal/start/end/chest pools use 1.
    int weight = 1;
};

struct DungeonTemplateSet {
    std::vector<DungeonRoomTemplate> starts;
    std::vector<DungeonRoomTemplate> normals;
    std::vector<DungeonRoomTemplate> ends;
    // Recovered separate boss-room family (level-objects). The normal end pool
    // remains r_transgete; boss stages can select this pool.
    std::vector<DungeonRoomTemplate> boss_ends;
    std::vector<DungeonRoomTemplate> chests;
    std::vector<DungeonRoomTemplate> specials;
};

struct DungeonPlan {
    std::int32_t seed = 0;
    std::vector<Room> rooms;
    std::vector<RoomConnection> connections;
    std::string start_room_id;
};

// Reconstructed MapManagerLevel.CreateMap graph:
// - 5x5 room_relation grid, start at (2,2)
// - map_long main-path steps (1-1: 3)
// - branch rooms with recovered 70/40/10% acceptance thresholds
// - slot types 0 start, 1 normal, 2 end/boss, 3 chest, 4 special
// - connections derive from the relation grid and physical door sides
//
// The supplied templates are actual recovered room scenes. When fewer templates
// than generated slots exist, map_long and branch capacity shrink rather than
// fabricating duplicate scene ids.
DungeonPlan BuildDungeonPlan(std::int32_t seed,
                             const DungeonTemplateSet& templates,
                             int requested_map_long = 3);

// Compatibility overload: every template belongs to the normal pool.
DungeonPlan BuildDungeonPlan(std::int32_t seed,
                             std::vector<DungeonRoomTemplate> templates,
                             int requested_map_long = 3);

} // namespace soulknight::game::room
