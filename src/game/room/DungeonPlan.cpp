#include "DungeonPlan.hpp"

#include "engine/math/UnityRandom.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <unordered_set>
#include <utility>
#include <vector>

namespace soulknight::game::room {
namespace {
struct Cell {
    int x = 0;
    int y = 0;
};
constexpr int kGrid = 5;
constexpr int kStartX = 2;
constexpr int kStartY = 2;

int relation_at(const std::array<std::array<int, kGrid>, kGrid>& relation,
                const Cell& cell) {
    if (cell.x < 0 || cell.x >= kGrid || cell.y < 0 || cell.y >= kGrid) {
        return 0;
    }
    return relation[static_cast<std::size_t>(cell.y)]
                   [static_cast<std::size_t>(cell.x)];
}

void set_relation(std::array<std::array<int, kGrid>, kGrid>& relation,
                  const Cell& cell, int value) {
    relation[static_cast<std::size_t>(cell.y)]
            [static_cast<std::size_t>(cell.x)] = value;
}

bool inside(const Cell& cell) {
    return cell.x >= 0 && cell.x < kGrid && cell.y >= 0 && cell.y < kGrid;
}

void Shuffle(std::vector<DungeonRoomTemplate>& values,
             engine::RGRandomStream& random) {
    for (std::size_t i = 1; i < values.size(); ++i) {
        const int j = random.Range(0, static_cast<int>(i + 1));
        std::swap(values[i], values[static_cast<std::size_t>(j)]);
    }
}

const DungeonRoomTemplate* PickWeighted(
        const std::vector<DungeonRoomTemplate>& pool,
        engine::RGRandomStream& random,
        const std::unordered_set<std::string>& used) {
    int total = 0;
    for (const auto& entry : pool) {
        if (entry.id.empty() || used.contains(entry.id)) continue;
        total += std::max(1, entry.weight);
    }
    if (total <= 0) return nullptr;
    int roll = random.Range(0, total);
    for (const auto& entry : pool) {
        if (entry.id.empty() || used.contains(entry.id)) continue;
        const int weight = std::max(1, entry.weight);
        if (roll < weight) return &entry;
        roll -= weight;
    }
    return nullptr;
}
} // namespace

DungeonPlan BuildDungeonPlan(std::int32_t seed,
                             const DungeonTemplateSet& pools,
                             int requested_map_long) {
    DungeonPlan plan;
    plan.seed = seed;

    std::vector<DungeonRoomTemplate> starts = pools.starts;
    std::vector<DungeonRoomTemplate> normals = pools.normals;
    std::vector<DungeonRoomTemplate> ends = pools.ends;
    std::vector<DungeonRoomTemplate> chests = pools.chests;
    std::vector<DungeonRoomTemplate> specials = pools.specials;
    if (normals.empty()) {
        for (const auto& pool : {starts, ends, chests, specials}) {
            normals.insert(normals.end(), pool.begin(), pool.end());
        }
    }
    if (normals.empty() && starts.empty() && ends.empty() &&
        chests.empty() && specials.empty()) {
        return plan;
    }
    const std::size_t total_templates = starts.size() + normals.size() +
        ends.size() + chests.size() + specials.size();
    if (total_templates == 1) {
        const std::vector<DungeonRoomTemplate>* only = nullptr;
        for (const auto* pool : {&starts, &normals, &ends, &chests, &specials}) {
            if (!pool->empty()) {
                only = pool;
                break;
            }
        }
        if (only != nullptr) {
            plan.rooms.push_back(Room{only->front().id, only->front().scene_path,
                                      kStartX, kStartY, 0});
            plan.start_room_id = only->front().id;
        }
        return plan;
    }

    engine::RGRandomStream random;
    random.SetRandomSeed(seed);
    Shuffle(normals, random);

    // CONFIRMED MapManagerLevel.ctor/CreateMap: a 5x5 relation grid with the
    // player starting at (2,2) and map_long main-path steps.
    std::array<std::array<int, kGrid>, kGrid> relation{};
    int map_long = std::clamp(requested_map_long, 1,
                              std::max(1, static_cast<int>(normals.size())));
    std::vector<Cell> main_path;
    Cell current{kStartX, kStartY};
    main_path.push_back(current);
    set_relation(relation, current, 1);
    int step = 0;
    while (step < map_long) {
        std::vector<Cell> candidates;
        const Cell neighbors[4] = {
            {current.x + 1, current.y}, {current.x, current.y - 1},
            {current.x - 1, current.y}, {current.x, current.y + 1}};
        for (const Cell& candidate : neighbors) {
            if (inside(candidate) &&
                relation_at(relation, candidate) == 0) {
                candidates.push_back(candidate);
            }
        }
        if (candidates.empty()) {
            if (main_path.size() <= 1) break;
            set_relation(relation, current, 0);
            main_path.pop_back();
            current = main_path.back();
            --step;
            continue;
        }
        const int pick = random.Range(0, static_cast<int>(candidates.size()));
        current = candidates[static_cast<std::size_t>(pick)];
        set_relation(relation, current, step + 2);
        main_path.push_back(current);
        ++step;
    }

    std::vector<Cell> map_list = main_path;
    const int normal_needed = std::max(0, map_long - 1);
    const int extra_pool = static_cast<int>(
        (chests.empty() ? 0u : 1u) + specials.size());
    const int branch_capacity = std::max(
        0, static_cast<int>(normals.size()) - normal_needed - 1 + extra_pool);
    if (branch_capacity > 0 && main_path.size() > 1) {
        std::vector<std::pair<Cell, int>> branch_candidates;
        for (std::size_t i = 1; i + 1 < main_path.size(); ++i) {
            const Cell parent = main_path[i];
            const Cell neighbors[4] = {
                {parent.x + 1, parent.y}, {parent.x, parent.y - 1},
                {parent.x - 1, parent.y}, {parent.x, parent.y + 1}};
            for (const Cell& candidate : neighbors) {
                if (inside(candidate) &&
                    relation_at(relation, candidate) == 0) {
                    branch_candidates.emplace_back(candidate,
                        relation_at(relation, parent));
                }
            }
        }
        for (std::size_t i = 1; i < branch_candidates.size(); ++i) {
            const int j = random.Range(0, static_cast<int>(i + 1));
            std::swap(branch_candidates[i],
                      branch_candidates[static_cast<std::size_t>(j)]);
        }
        int accepted = 0;
        for (const auto& [candidate, parent_relation] : branch_candidates) {
            if (accepted >= branch_capacity) break;
            const int threshold = 70 - 30 * accepted;
            const bool force_chest = accepted == 0;
            if (!force_chest && random.Range(0, 100) >= threshold) continue;
            set_relation(relation, candidate, parent_relation);
            map_list.push_back(candidate);
            ++accepted;
        }
    }

    std::vector<int> types;
    types.reserve(map_list.size());
    for (std::size_t i = 0; i < map_list.size(); ++i) {
        if (i == 0) types.push_back(0);
        else if (static_cast<int>(i) < map_long) types.push_back(1);
        else if (static_cast<int>(i) == map_long) types.push_back(2);
        else if (static_cast<int>(i) == map_long + 1) types.push_back(3);
        else types.push_back(4);
    }

    std::unordered_set<std::string> used;
    std::size_t normal_next = 0;
    const auto take_normal = [&]() -> const DungeonRoomTemplate* {
        while (normal_next < normals.size()) {
            const DungeonRoomTemplate& entry = normals[normal_next++];
            if (entry.id.empty() || used.contains(entry.id)) continue;
            return &entry;
        }
        return nullptr;
    };
    const auto take_any = [&]() -> const DungeonRoomTemplate* {
        for (const auto* pool : {&normals, &starts, &ends, &chests, &specials}) {
            for (const auto& entry : *pool) {
                if (!entry.id.empty() && !used.contains(entry.id)) return &entry;
            }
        }
        return nullptr;
    };
    const auto pick_pool = [&](const std::vector<DungeonRoomTemplate>& pool)
            -> const DungeonRoomTemplate* {
        return PickWeighted(pool, random, used);
    };

    for (std::size_t i = 0; i < map_list.size(); ++i) {
        const int type = types[i];
        const DungeonRoomTemplate* chosen = nullptr;
        if (type == 0) {
            chosen = pick_pool(starts);
            if (chosen == nullptr) chosen = take_normal();
        } else if (type == 1) {
            chosen = take_normal();
        } else if (type == 2) {
            chosen = pick_pool(ends);
            if (chosen == nullptr) chosen = take_normal();
        } else if (type == 3) {
            chosen = pick_pool(chests);
            if (chosen == nullptr) chosen = take_normal();
        } else {
            chosen = pick_pool(specials);
            if (chosen == nullptr) chosen = take_normal();
        }
        if (chosen == nullptr) chosen = take_any();
        if (chosen == nullptr) break;
        used.insert(chosen->id);
        const Cell cell = map_list[i];
        plan.rooms.push_back(Room{chosen->id, chosen->scene_path,
                                  cell.x, cell.y, type});
    }
    if (plan.rooms.empty()) return plan;
    plan.start_room_id = plan.rooms.front().id;

    // Build one bidirectional edge per adjacent pair. The reference
    // entrance-bit rule allows relation values equal or consecutive; branches
    // inherit their parent's value and may also connect to sibling branches.
    for (std::size_t i = 0; i < plan.rooms.size(); ++i) {
        const Cell from{plan.rooms[i].map_x, plan.rooms[i].map_y};
        const int from_relation = relation_at(relation, from);
        const Cell neighbors[2] = {{from.x + 1, from.y}, {from.x, from.y + 1}};
        for (const Cell& to : neighbors) {
            const int to_relation = relation_at(relation, to);
            if (to_relation == 0) continue;
            if (std::abs(from_relation - to_relation) > 1) continue;
            std::size_t j = plan.rooms.size();
            for (std::size_t k = 0; k < plan.rooms.size(); ++k) {
                if (plan.rooms[k].map_x == to.x &&
                    plan.rooms[k].map_y == to.y) {
                    j = k;
                    break;
                }
            }
            if (j >= plan.rooms.size()) continue;
            const auto [from_door, to_door] = [&]() {
                if (to.x > from.x) return std::pair<std::string, std::string>{
                    "east", "west"};
                if (to.x < from.x) return std::pair<std::string, std::string>{
                    "west", "east"};
                if (to.y > from.y) return std::pair<std::string, std::string>{
                    "north", "south"};
                return std::pair<std::string, std::string>{"south", "north"};
            }();
            plan.connections.push_back(RoomConnection{
                plan.rooms[i].id, plan.rooms[j].id, "bidirectional",
                from_door, to_door});
        }
    }
    return plan;
}

DungeonPlan BuildDungeonPlan(std::int32_t seed,
                             std::vector<DungeonRoomTemplate> templates,
                             int requested_map_long) {
    DungeonTemplateSet pools;
    pools.normals = std::move(templates);
    return BuildDungeonPlan(seed, pools, requested_map_long);
}

} // namespace soulknight::game::room
