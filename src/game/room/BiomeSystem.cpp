#include "BiomeSystem.hpp"

#include <algorithm>

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

namespace soulknight::game::room {
namespace {
using Json = nlohmann::json;

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
}

std::vector<std::string> ReadStrings(const Json& item, const char* key) {
    std::vector<std::string> result;
    if (!item.contains(key) || !item[key].is_array()) return result;
    for (const Json& value : item[key]) {
        if (value.is_string()) result.push_back(value.get<std::string>());
    }
    return result;
}
} // namespace

bool BiomeSystem::Load(const platform::FileSystem& filesystem,
                       const std::filesystem::path& path, std::string* error) {
    loaded_ = false;
    levels_.clear();
    current_ = 0;
    if (error != nullptr) error->clear();
    const auto text = filesystem.ReadTextFile(path);
    if (!text) {
        SetError(error, "biome catalogue not found: " + path.string());
        return false;
    }
    const Json document = Json::parse(*text, nullptr, false);
    if (document.is_discarded() || !document.is_object() ||
        !document.contains("levels") || !document["levels"].is_array()) {
        SetError(error, "biome catalogue is invalid");
        return false;
    }
    for (const Json& item : document["levels"]) {
        if (!item.is_object() || !item.contains("level") ||
            !item["level"].is_string()) {
            continue;
        }
        const std::string id = item["level"].get<std::string>();
        if (id.empty() || id.front() < '0' || id.front() > '9' ||
            id.find('-') == std::string::npos) {
            continue;
        }
        BiomeLevel level;
        level.id = id;
        level.map_long = item.value("map_long", 3);
        level.chest_level = item.value("chest_level", 1);
        level.start_rooms = ReadStrings(item, "room_start_list");
        level.end_rooms = ReadStrings(item, "room_end_list");
        level.floors = ReadStrings(item, "floor_list");
        level.walls = ReadStrings(item, "wall_list");
        level.obstacles = ReadStrings(item, "obstacle_list");
        if (item.contains("enemy_infos") && item["enemy_infos"].is_array()) {
            for (const Json& enemy : item["enemy_infos"]) {
                if (!enemy.is_object() || !enemy.contains("enemy") ||
                    !enemy["enemy"].is_string()) {
                    continue;
                }
                BiomeEnemyWeight entry;
                entry.id = enemy["enemy"].get<std::string>();
                entry.weight = std::max(1, enemy.value("weight", 1));
                if (!entry.id.empty()) level.enemies.push_back(std::move(entry));
            }
        }
        levels_.push_back(std::move(level));
    }
    std::sort(levels_.begin(), levels_.end(),
              [](const BiomeLevel& a, const BiomeLevel& b) {
                  return a.id < b.id;
              });
    loaded_ = !levels_.empty();
    if (!loaded_) {
        SetError(error, "biome catalogue has no levels");
        return false;
    }
    return true;
}

const BiomeLevel* BiomeSystem::Current() const {
    return At(current_);
}

const BiomeLevel* BiomeSystem::At(std::size_t index) const {
    if (index >= levels_.size()) return nullptr;
    return &levels_[index];
}

bool BiomeSystem::Select(const std::string& level_id) {
    for (std::size_t i = 0; i < levels_.size(); ++i) {
        if (levels_[i].id == level_id) {
            current_ = i;
            return true;
        }
    }
    return false;
}

bool BiomeSystem::Advance() {
    if (current_ + 1 >= levels_.size()) return false;
    ++current_;
    return true;
}

std::string BiomeSystem::PickEnemy(int roll) const {
    const BiomeLevel* level = Current();
    if (level == nullptr || level->enemies.empty()) return {};
    int total = 0;
    for (const BiomeEnemyWeight& enemy : level->enemies) total += enemy.weight;
    if (total <= 0) return {};
    roll %= total;
    if (roll < 0) roll += total;
    for (const BiomeEnemyWeight& enemy : level->enemies) {
        if (roll < enemy.weight) return enemy.id;
        roll -= enemy.weight;
    }
    return level->enemies.back().id;
}

} // namespace soulknight::game::room
