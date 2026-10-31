#include "UIAssets.hpp"

#include "engine/assets/PngDecoder.hpp"
#include "platform/filesystem/FileSystem.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>

namespace soulknight::game::ui {
namespace {

bool ReadRect(const nlohmann::json& item, engine::Rect* rect) {
    if (!item.contains("rect_uv") || !item["rect_uv"].is_array() ||
        item["rect_uv"].size() != 4) {
        return false;
    }
    for (std::size_t i = 0; i < 4; ++i) {
        if (!item["rect_uv"][i].is_number()) return false;
    }
    rect->x = item["rect_uv"][0].get<float>();
    rect->y = item["rect_uv"][1].get<float>();
    rect->w = item["rect_uv"][2].get<float>();
    rect->h = item["rect_uv"][3].get<float>();
    return true;
}

} // namespace

bool UIAssets::Load(const platform::FileSystem& filesystem,
                    const std::string& manifest_path, std::string* error) {
    if (error != nullptr) error->clear();
    loaded_ = false;
    textures_.clear();
    sprites_.clear();
    const auto text = filesystem.ReadTextFile(manifest_path);
    if (!text) {
        if (error != nullptr) *error = "UI manifest missing: " + manifest_path;
        return false;
    }
    nlohmann::json document = nlohmann::json::parse(*text, nullptr, false);
    if (document.is_discarded() || !document.is_object() ||
        !document.contains("textures") || !document["textures"].is_array() ||
        !document.contains("sprites") || !document["sprites"].is_array()) {
        if (error != nullptr) *error = "UI manifest is corrupt";
        return false;
    }
    const std::filesystem::path manifest_dir =
        std::filesystem::path(manifest_path).parent_path();
    for (const auto& item : document["textures"]) {
        if (!item.is_object() || !item.contains("id") ||
            !item.contains("png") || !item["png"].is_string()) {
            continue;
        }
        UIAssetTexture texture;
        texture.id = kFirstTextureId +
            static_cast<uint32_t>(textures_.size());
        texture.name = item.value("name", std::string{});
        texture.path = (manifest_dir / item["png"].get<std::string>())
                           .generic_string();
        const auto png = filesystem.ReadFile(texture.path);
        if (!png) continue;
        if (!engine::assets::PngDecoder::Decode(png->data(), png->size(),
                                                texture.data, error)) {
            return false;
        }
        textures_.push_back(std::move(texture));
    }
    if (textures_.empty()) {
        if (error != nullptr) *error = "UI manifest contains no textures";
        return false;
    }
    for (const auto& item : document["sprites"]) {
        if (!item.is_object() || !item.contains("name") ||
            !item["name"].is_string() || !item.contains("id") ||
            !item.contains("texture")) {
            continue;
        }
        UIAssetSprite sprite;
        sprite.name = item["name"].get<std::string>();
        sprite.id = item.value("id", std::string{});
        sprite.texture_id = 0;
        const std::string texture_id = item["texture"].get<std::string>();
        const std::string manifest_texture =
            (manifest_dir / ("textures/" + texture_id + ".png"))
                .generic_string();
        for (const UIAssetTexture& texture : textures_) {
            if (texture.path == manifest_texture) {
                sprite.texture_id = texture.id;
                break;
            }
        }
        if (sprite.texture_id == 0) continue;
        if (!ReadRect(item, &sprite.uv)) continue;
        if (item.contains("rect_pixels") &&
            item["rect_pixels"].is_array() &&
            item["rect_pixels"].size() == 4) {
            sprite.source_pixels = {
                item["rect_pixels"][2].get<float>(),
                item["rect_pixels"][3].get<float>()};
        }
        sprite.pixels_per_unit = item.value("pixels_per_unit", 0.0f);
        sprites_[sprite.name] = std::move(sprite);
    }
    loaded_ = true;
    return true;
}

const UIAssetSprite* UIAssets::Find(const std::string& name) const {
    const auto it = sprites_.find(name);
    return it == sprites_.end() ? nullptr : &it->second;
}

} // namespace soulknight::game::ui
