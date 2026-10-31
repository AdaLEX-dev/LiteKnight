#include "DungeonPlan.hpp"

#include "engine/math/UnityRandom.hpp"

#include <utility>

namespace soulknight::game::room {

DungeonPlan BuildDungeonPlan(std::int32_t seed,
                             std::vector<DungeonRoomTemplate> templates) {
    DungeonPlan plan;
    plan.seed = seed;
    if (templates.empty()) return plan;

    // Fisher-Yates over the actual recovered room templates. This uses the
    // reconstructed Unity 2018.4 RGRandom integer path, not std::shuffle, so a
    // fixed seed has a stable cross-platform room assignment.
    engine::RGRandomStream random;
    random.SetRandomSeed(seed);
    for (std::size_t i = templates.size() - 1; i > 0; --i) {
        const int j = random.Range(0, static_cast<int>(i + 1));
        std::swap(templates[i], templates[static_cast<std::size_t>(j)]);
    }

    // Always use every supplied actual room. With fewer than three templates
    // the fixed L-shape collapses to a chain; it never fabricates duplicates.
    const std::size_t count = templates.size();
    const std::string first_id = templates[0].id;
    const std::string second_id = templates[count > 1 ? 1 : 0].id;
    const std::string third_id = templates[count > 2 ? 2 : 0].id;

    for (const DungeonRoomTemplate& room : templates) {
        plan.rooms.push_back(Room{room.id, room.scene_path});
    }
    plan.start_room_id = first_id;

    if (count > 1 && second_id != first_id) {
        plan.connections.push_back(
            RoomConnection{first_id, second_id, "bidirectional",
                           "north", "south"});
    }
    if (count > 2 && third_id != first_id && third_id != second_id) {
        plan.connections.push_back(
            RoomConnection{first_id, third_id, "bidirectional",
                           "east", "west"});
    }
    return plan;
}

} // namespace soulknight::game::room
