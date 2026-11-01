#include "ItemAssets.hpp"

#include "engine/assets/SceneLoader.hpp"

#include <unordered_map>

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

namespace soulknight::game::items {
namespace {
using Json = nlohmann::json;

engine::Vec2 ReadVec2(const Json& value, engine::Vec2 fallback) {
    if (!value.is_array() || value.size() != 2 ||
        !value[0].is_number() || !value[1].is_number()) {
        return fallback;
    }
    return {value[0].get<float>(), value[1].get<float>()};
}

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
}
} // namespace

bool ItemAssets::Load(const platform::FileSystem& filesystem,
                      const std::filesystem::path& path, std::string* error) {
    loaded_ = false;
    chests_.clear();
    coins_.clear();
    portals_.clear();
    weapon_drops_.clear();
    if (error != nullptr) error->clear();
    std::string scene_error;
    if (!engine::assets::SceneLoader::Load(filesystem, path, assets_,
                                           &scene_error)) {
        SetError(error, scene_error);
        return false;
    }
    const auto text = filesystem.ReadTextFile(path);
    if (!text) {
        SetError(error, "item metadata could not be read: " + path.string());
        return false;
    }
    const Json document = Json::parse(*text, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        SetError(error, "item metadata is malformed");
        return false;
    }
    if (document.contains("chests")) {
        for (const Json& item : document["chests"]) {
            if (!item.is_object() || !item.contains("id") ||
                !item["id"].is_string()) {
                SetError(error, "item chest definition is invalid");
                return false;
            }
            ChestDefinition chest;
            chest.id = item["id"].get<std::string>();
            chest.level = item.value("level", 0);
            chest.collider_half_extents = ReadVec2(
                item.value("collider_half_extents", Json::array()),
                chest.collider_half_extents);
            chest.collider_offset = ReadVec2(
                item.value("collider_offset", Json::array()),
                chest.collider_offset);
            chest.open_time = item.value("open_time", 1.0f);
            chest.loot_coin = item.value("loot_coin", std::string{});
            chest.loot_count = item.value("loot_count", 0);
            chest.appointed_weapon = item.value("appointed_weapon", std::string{});
            chest.weapon_drop_level = item.value("weapon_drop_level", -1);
            if (item.contains("visuals")) {
                for (const Json& visual_item : item["visuals"]) {
                    if (!visual_item.is_object() ||
                        !visual_item.contains("sprite") ||
                        !visual_item["sprite"].is_string()) {
                        SetError(error, "item chest visual is invalid");
                        return false;
                    }
                    ItemVisual visual;
                    visual.sprite = visual_item["sprite"].get<std::string>();
                    visual.offset = ReadVec2(
                        visual_item.value("offset", Json::array()),
                        visual.offset);
                    visual.sorting_layer = visual_item.value("sorting_layer", 5);
                    visual.sorting_order = visual_item.value("sorting_order", 0);
                    chest.visuals.push_back(std::move(visual));
                }
            }
            chests_.push_back(std::move(chest));
        }
    }
    if (document.contains("coins")) {
        for (const Json& item : document["coins"]) {
            if (!item.is_object() || !item.contains("id") ||
                !item["id"].is_string()) {
                SetError(error, "item coin definition is invalid");
                return false;
            }
            CoinDefinition coin;
            coin.id = item["id"].get<std::string>();
            coin.type = item.value("type", 0);
            coin.value = item.value("value", 0);
            coin.speed = item.value("speed", 20.0f);
            coin.range = item.value("range", 7.0f);
            if (item.contains("frames")) {
                for (const Json& frame : item["frames"]) {
                    if (frame.is_string()) coin.frames.push_back(frame.get<std::string>());
                }
            }
            coins_.push_back(std::move(coin));
        }
    }
    if (document.contains("weapon_drops")) {
        for (const Json& item : document["weapon_drops"]) {
            if (!item.is_object()) continue;
            WeaponDropLevel level;
            level.level = item.value("level", 0);
            if (item.contains("entries")) {
                for (const Json& entry_item : item["entries"]) {
                    if (!entry_item.is_object()) continue;
                    WeaponDropEntry entry;
                    entry.weapon = entry_item.value("weapon", std::string{});
                    entry.weight = entry_item.value("weight", 0);
                    entry.title = entry_item.value("title", std::string{});
                    if (!entry.weapon.empty() && entry.weight > 0) {
                        level.entries.push_back(std::move(entry));
                    }
                }
            }
            if (!level.entries.empty()) weapon_drops_.push_back(std::move(level));
        }
    }
    if (document.contains("portals")) {
        for (const Json& item : document["portals"]) {
            if (!item.is_object() || !item.contains("id") ||
                !item["id"].is_string()) {
                SetError(error, "item portal definition is invalid");
                return false;
            }
            PortalDefinition portal;
            portal.id = item["id"].get<std::string>();
            portal.fps = item.value("fps", 12.0f);
            portal.duration = item.value("duration", 0.5f);
            portal.loop = item.value("loop", true);
            if (item.contains("frames")) {
                for (const Json& frame : item["frames"]) {
                    if (frame.is_string()) {
                        portal.frames.push_back(frame.get<std::string>());
                    }
                }
            }
            if (!portal.frames.empty()) portals_.push_back(std::move(portal));
        }
    }
    loaded_ = !assets_.textures.empty() && !assets_.sprites.empty() &&
              !chests_.empty() && !coins_.empty();
    if (!loaded_) {
        SetError(error, "item pack is incomplete");
        return false;
    }
    return true;
}

const ChestDefinition* ItemAssets::FindChest(const std::string& id) const {
    for (const ChestDefinition& chest : chests_) {
        if (chest.id == id) return &chest;
    }
    return nullptr;
}

const PortalDefinition* ItemAssets::FindPortal(const std::string& id) const {
    for (const PortalDefinition& portal : portals_) {
        if (portal.id == id) return &portal;
    }
    return nullptr;
}

const CoinDefinition* ItemAssets::FindCoin(const std::string& id) const {
    for (const CoinDefinition& coin : coins_) {
        if (coin.id == id) return &coin;
    }
    return nullptr;
}

void ItemAssets::AppendToScene(engine::assets::SceneAsset& scene) const {
    if (!loaded_) return;
    std::unordered_map<uint32_t, uint32_t> texture_map;
    for (const auto& texture : assets_.textures) {
        const uint32_t new_id = static_cast<uint32_t>(scene.textures.size());
        engine::assets::TextureAsset copy = texture;
        copy.id = new_id;
        texture_map[texture.id] = new_id;
        scene.textures.push_back(std::move(copy));
    }
    for (const auto& source : assets_.sprites) {
        engine::assets::SpriteData sprite = source;
        const auto it = texture_map.find(source.texture_id);
        if (it == texture_map.end()) continue;
        sprite.texture_id = it->second;
        scene.sprites.push_back(std::move(sprite));
    }
}

} // namespace soulknight::game::items
