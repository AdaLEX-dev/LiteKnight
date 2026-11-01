// Soul Knight Linux — recovered MapManagerLevel biome/level definitions
#pragma once

#include "platform/filesystem/FileSystem.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace soulknight::game::room {

struct BiomeEnemyWeight {
    std::string id;
    int weight = 1;
};

struct BiomeLevel {
    std::string id;
    int map_long = 3;
    int chest_level = 1;
    std::vector<BiomeEnemyWeight> enemies;
    std::vector<std::string> start_rooms;
    std::vector<std::string> end_rooms;
    std::vector<std::string> floors;
    std::vector<std::string> walls;
    std::vector<std::string> obstacles;
};

// Ordered campaign levels from the recovered MapManagerLevel assets. Level
// order is the serialized catalogue order (1-1..1-5, 2-1..2-5, 3-1..3-6).
class BiomeSystem {
public:
    bool Load(const platform::FileSystem& filesystem,
              const std::filesystem::path& path, std::string* error);
    bool IsLoaded() const { return loaded_; }
    std::size_t Count() const { return levels_.size(); }
    std::size_t CurrentIndex() const { return current_; }
    const BiomeLevel* Current() const;
    const BiomeLevel* At(std::size_t index) const;
    bool Select(const std::string& level_id);
    bool Advance();
    void Reset() { current_ = 0; }

    // Deterministic weighted pick from the level pool. Returns an empty string
    // when the level has no usable enemy entries.
    std::string PickEnemy(int roll) const;

private:
    std::vector<BiomeLevel> levels_;
    std::size_t current_ = 0;
    bool loaded_ = false;
};

} // namespace soulknight::game::room
