// Soul Knight Linux — recovered MapManagerLevel surface generation
#include "LevelSurface.hpp"

#include "engine/math/UnityRandom.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace soulknight::game::room {
namespace {

struct CellKey {
    int x = 0;
    int y = 0;
    bool operator==(const CellKey& other) const {
        return x == other.x && y == other.y;
    }
};

struct CellHash {
    std::size_t operator()(const CellKey& key) const {
        return static_cast<std::size_t>(
            (static_cast<std::uint32_t>(key.x) * 73856093u) ^
            (static_cast<std::uint32_t>(key.y) * 19349663u));
    }
};

CellKey ToCell(const engine::Vec2& value) {
    return {static_cast<int>(std::lround(value.x)),
            static_cast<int>(std::lround(value.y))};
}

std::uint32_t MixSeed(std::uint32_t seed, std::uint32_t value) {
    seed ^= value + 0x9E3779B9u + (seed << 6u) + (seed >> 2u);
    return seed;
}

engine::Vec2 ToVec(const CellKey& cell) {
    return {static_cast<float>(cell.x), static_cast<float>(cell.y)};
}

void AddSurface(std::vector<SurfaceInstance>* output,
                const engine::assets::LevelSurfaceSprite& sprite,
                const engine::Vec2& position) {
    if (sprite.sprite_index < 0) return;
    SurfaceInstance instance;
    instance.sprite_index = sprite.sprite_index;
    instance.position = position + sprite.offset;
    instance.scale = sprite.scale;
    instance.rotation = sprite.rotation;
    instance.color = sprite.color;
    instance.sorting_layer = sprite.sorting_layer;
    instance.sorting_order = sprite.sorting_order;
    instance.flip_x = sprite.flip_x;
    instance.flip_y = sprite.flip_y;
    output->push_back(instance);
}

void CreateFloor(const std::vector<engine::Vec2>& cells,
                 const engine::assets::LevelTheme& theme,
                 engine::RGRandomStream& random,
                 std::vector<SurfaceInstance>* output) {
    if (cells.empty() || theme.floors.empty()) return;
    std::unordered_map<CellKey, int, CellHash> cells_set;
    for (const engine::Vec2& value : cells) cells_set[ToCell(value)] = 0;
    std::vector<CellKey> ordered;
    ordered.reserve(cells_set.size());
    for (const auto& item : cells_set) ordered.push_back(item.first);
    std::sort(ordered.begin(), ordered.end(), [](const CellKey& a,
                                                 const CellKey& b) {
        return a.y == b.y ? a.x < b.x : a.y < b.y;
    });
    std::unordered_map<CellKey, int, CellHash> pattern;
    for (const CellKey& cell : ordered) {
        cells_set[cell] = random.Range(0, 2);
    }
    const auto is_zero = [&](int x, int y) {
        const auto it = cells_set.find(CellKey{x, y});
        return it != cells_set.end() && it->second == 0 ? 1 : 0;
    };
    for (const CellKey& cell : ordered) {
        int p = is_zero(cell.x, cell.y);
        p += is_zero(cell.x - 1, cell.y);
        p += is_zero(cell.x + 1, cell.y);
        p += is_zero(cell.x, cell.y - 1);
        p += is_zero(cell.x, cell.y + 1);
        pattern[cell] = p;
    }

    const int floor_count = static_cast<int>(theme.floors.size());
    const auto clamp_index = [&](int index) {
        return std::clamp(index, 0, floor_count - 1);
    };
    for (const CellKey& cell : ordered) {
        const int p = pattern[cell];
        int index = 0;
        if (!theme.use_new_floor_pattern) {
            if (p == 0 || p == 3) {
                index = random.Range(0, 2) == 0 ? 2 : 3;
            } else if (p == 1 || p == 2) {
                index = random.Range(0, 4) < 3 ? 0 : 1;
            } else {
                index = random.Range(0, 4) < 1 ? 4 : 5;
            }
        } else if (p == 1 || p == 2) {
            index = random.Range(0, 4) < 3 ? 0 : 1;
        } else if (p == 4 || p == 5) {
            index = random.Range(0, 4) < 1 ? 0 : 1;
        } else {
            // p == 0 or 3: 60% -> 0/1, 40% -> 2..5.
            if (random.Range(0, 10) < 6) {
                index = random.Range(0, 2);
            } else {
                index = random.Range(2, 6);
            }
        }
        AddSurface(output, theme.floors[static_cast<std::size_t>(
                       clamp_index(index))].sprite, ToVec(cell));
    }
}

void CreateWalls(const std::vector<engine::Vec2>& cells,
                 const engine::assets::LevelTheme& theme,
                 engine::RGRandomStream& random,
                 std::vector<SurfaceInstance>* output) {
    if (cells.empty() || theme.walls.empty()) return;
    std::vector<CellKey> ordered;
    ordered.reserve(cells.size());
    for (const engine::Vec2& value : cells) ordered.push_back(ToCell(value));
    std::sort(ordered.begin(), ordered.end(), [](const CellKey& a,
                                                 const CellKey& b) {
        return a.y == b.y ? a.x < b.x : a.y < b.y;
    });
    const auto select_wall = [&]() -> const engine::assets::LevelWallVariant& {
        const int roll = random.Range(0, 100);
        const int index = roll < 30 ? 0 : 1;
        return theme.walls[static_cast<std::size_t>(
            std::min(index, static_cast<int>(theme.walls.size()) - 1))];
    };
    for (const CellKey& cell : ordered) {
        const engine::assets::LevelWallVariant& wall = select_wall();
        for (const engine::assets::LevelSurfaceSprite& sprite : wall.sprites) {
            AddSurface(output, sprite, ToVec(cell));
        }
    }
}

} // namespace

const engine::assets::LevelTheme* LevelSurfaceBuilder::Find(
        const engine::assets::SceneAsset& scene, const std::string& level,
        const std::string& branch) {
    const engine::assets::LevelTheme* fallback = nullptr;
    for (const engine::assets::LevelTheme& theme : scene.level_themes) {
        if (theme.level != level) continue;
        if (theme.branch == branch) return &theme;
        if (fallback == nullptr && branch == "A") fallback = &theme;
    }
    return fallback;
}

std::vector<SurfaceInstance> LevelSurfaceBuilder::Build(
        const engine::assets::SceneAsset& scene,
        const engine::assets::LevelTheme& theme, std::uint32_t room_seed) {
    std::vector<SurfaceInstance> surfaces;
    engine::RGRandomStream floor_random;
    floor_random.SetRandomSeed(
        static_cast<int32_t>(MixSeed(room_seed, 0x466C6F6Fu) & 0x7FFFFFFFu));
    engine::RGRandomStream aisle_random;
    aisle_random.SetRandomSeed(
        static_cast<int32_t>(MixSeed(room_seed, 0x4169736Cu) & 0x7FFFFFFFu));
    engine::RGRandomStream wall_random;
    wall_random.SetRandomSeed(
        static_cast<int32_t>(MixSeed(room_seed, 0x57616C6Cu) & 0x7FFFFFFFu));

    CreateFloor(scene.floor_cells, theme, floor_random, &surfaces);
    CreateFloor(scene.aisle_floor_cells, theme, aisle_random, &surfaces);
    CreateWalls(scene.wall_cells, theme, wall_random, &surfaces);
    CreateWalls(scene.corridor_wall_cells, theme, wall_random, &surfaces);
    return surfaces;
}

} // namespace soulknight::game::room
