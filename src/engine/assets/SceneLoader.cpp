// Soul Knight Linux — native scene loader implementation
#include "SceneLoader.hpp"

#include "PngDecoder.hpp"
#include "engine/core/Types.hpp"
#include "platform/filesystem/FileSystem.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace soulknight::engine::assets {
namespace {

using Json = nlohmann::json;

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
}

bool ReadFloat(const Json& value, float& output) {
    if (!value.is_number()) return false;
    output = value.get<float>();
    return true;
}

bool ReadVec2(const Json& value, Vec2& output) {
    if (!value.is_array() || value.size() != 2 ||
        !value[0].is_number() || !value[1].is_number()) {
        return false;
    }
    output = {value[0].get<float>(), value[1].get<float>()};
    return true;
}

bool ReadRect(const Json& value, Rect& output) {
    if (!value.is_array() || value.size() != 4) return false;
    for (const Json& element : value) {
        if (!element.is_number()) return false;
    }
    output = {value[0].get<float>(), value[1].get<float>(),
              value[2].get<float>(), value[3].get<float>()};
    return true;
}

bool ReadColor(const Json& value, Color& output) {
    if (!value.is_array() || value.size() != 4) return false;
    for (const Json& element : value) {
        if (!element.is_number_integer()) return false;
    }
    const auto channel = [&value](std::size_t index) {
        return static_cast<uint8_t>(std::clamp(value[index].get<int>(), 0, 255));
    };
    output = Color{channel(0), channel(1), channel(2), channel(3)};
    return true;
}

bool ReadFilter(const Json& value, TextureFilter& output) {
    if (!value.is_string()) return false;
    const std::string name = value.get<std::string>();
    if (name == "point") { output = TextureFilter::Point; return true; }
    if (name == "bilinear") { output = TextureFilter::Bilinear; return true; }
    if (name == "trilinear") { output = TextureFilter::Trilinear; return true; }
    return false;
}

bool ReadWrap(const Json& value, TextureWrap& output) {
    if (!value.is_string()) return false;
    const std::string name = value.get<std::string>();
    if (name == "repeat") { output = TextureWrap::Repeat; return true; }
    if (name == "clamp") { output = TextureWrap::Clamp; return true; }
    if (name == "mirror") { output = TextureWrap::Mirror; return true; }
    return false;
}

} // namespace

bool SceneLoader::Load(const platform::FileSystem& filesystem,
                       const std::filesystem::path& scene_path,
                       SceneAsset& scene,
                       std::string* error) {
    if (error != nullptr) error->clear();

    const auto text = filesystem.ReadTextFile(scene_path);
    if (!text) {
        SetError(error, "scene JSON could not be read: " + scene_path.string());
        return false;
    }

    const Json document = Json::parse(*text, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        SetError(error, "scene JSON is malformed: " + scene_path.string());
        return false;
    }

    SceneAsset loaded;
    loaded.id = document.value("id", "scene");

    if (!document.contains("camera") || !document["camera"].is_object()) {
        SetError(error, "scene JSON has no camera object");
        return false;
    }
    const Json& camera = document["camera"];
    if (!camera.contains("position") ||
        !ReadVec2(camera["position"], loaded.camera.position)) {
        SetError(error, "camera.position is invalid");
        return false;
    }
    if (!camera.contains("orthographic_size") ||
        !ReadFloat(camera["orthographic_size"], loaded.camera.orthographic_size) ||
        loaded.camera.orthographic_size <= 0.0f) {
        SetError(error, "camera.orthographic_size is invalid");
        return false;
    }
    if (camera.contains("viewport")) {
        Vec2 viewport;
        if (!ReadVec2(camera["viewport"], viewport) ||
            viewport.x <= 0.0f || viewport.y <= 0.0f) {
            SetError(error, "camera.viewport is invalid");
            return false;
        }
        loaded.camera.viewport_width = viewport.x;
        loaded.camera.viewport_height = viewport.y;
    }

    if (!document.contains("textures") || !document["textures"].is_array()) {
        SetError(error, "scene JSON has no textures array");
        return false;
    }
    std::unordered_map<std::string, uint32_t> texture_ids;
    for (const Json& item : document["textures"]) {
        if (!item.is_object() || !item.contains("id") || !item["id"].is_string() ||
            !item.contains("png") || !item["png"].is_string()) {
            SetError(error, "texture entry is missing id/png");
            return false;
        }

        TextureAsset texture;
        texture.name = item["id"].get<std::string>();
        texture.png_path = item["png"].get<std::string>();
        texture.id = static_cast<uint32_t>(loaded.textures.size());
        if (item.contains("filter") && !ReadFilter(item["filter"], texture.filter)) {
            SetError(error, "texture filter is invalid: " + texture.name);
            return false;
        }
        if (item.contains("wrap") && !ReadWrap(item["wrap"], texture.wrap)) {
            SetError(error, "texture wrap is invalid: " + texture.name);
            return false;
        }

        const std::filesystem::path texture_path =
            scene_path.parent_path() / texture.png_path;
        const std::filesystem::path resolved = filesystem.ResolvePath(texture_path);
        if (resolved.empty()) {
            SetError(error, "texture PNG not found: " + texture_path.string());
            return false;
        }
        std::string decode_error;
        if (!PngDecoder::DecodeFile(resolved, texture.data, &decode_error)) {
            SetError(error, "texture decode failed: " + decode_error);
            return false;
        }
        texture_ids.emplace(texture.name, texture.id);
        loaded.textures.push_back(std::move(texture));
    }

    if (!document.contains("sprites") || !document["sprites"].is_array()) {
        SetError(error, "scene JSON has no sprites array");
        return false;
    }
    std::unordered_map<std::string, uint32_t> sprite_ids;
    for (const Json& item : document["sprites"]) {
        if (!item.is_object() || !item.contains("id") || !item["id"].is_string() ||
            !item.contains("texture") || !item["texture"].is_string()) {
            SetError(error, "sprite entry is missing id/texture");
            return false;
        }

        SpriteData sprite;
        sprite.id = item["id"].get<std::string>();
        const std::string texture_name = item["texture"].get<std::string>();
        const auto texture_it = texture_ids.find(texture_name);
        if (texture_it == texture_ids.end()) {
            SetError(error, "sprite references unknown texture: " + texture_name);
            return false;
        }
        const TextureAsset& texture = loaded.textures[texture_it->second];
        sprite.texture_id = texture_it->second;
        sprite.texture_width = texture.data.width;
        sprite.texture_height = texture.data.height;

        if (!item.contains("rect") || !ReadRect(item["rect"], sprite.rect)) {
            SetError(error, "sprite rect is invalid: " + sprite.id);
            return false;
        }
        if (!item.contains("pivot") || !ReadVec2(item["pivot"], sprite.pivot)) {
            SetError(error, "sprite pivot is invalid: " + sprite.id);
            return false;
        }
        sprite.source_pivot = sprite.pivot;
        if (item.contains("source_pivot") &&
            !ReadVec2(item["source_pivot"], sprite.source_pivot)) {
            SetError(error, "sprite source_pivot is invalid: " + sprite.id);
            return false;
        }
        if (item.contains("texture_rect_offset") &&
            !ReadVec2(item["texture_rect_offset"], sprite.texture_rect_offset)) {
            SetError(error, "sprite texture_rect_offset is invalid: " + sprite.id);
            return false;
        }
        if (!item.contains("pixels_per_unit") ||
            !ReadFloat(item["pixels_per_unit"], sprite.pixels_per_unit) ||
            sprite.pixels_per_unit <= 0.0f) {
            SetError(error, "sprite pixels_per_unit is invalid: " + sprite.id);
            return false;
        }
        if (item.contains("border")) {
            if (!item["border"].is_array() || item["border"].size() != 4) {
                SetError(error, "sprite border is invalid: " + sprite.id);
                return false;
            }
            for (std::size_t i = 0; i < 4; ++i) {
                if (!ReadFloat(item["border"][i], sprite.border[i])) {
                    SetError(error, "sprite border is invalid: " + sprite.id);
                    return false;
                }
            }
        }
        if (item.contains("filter") && !ReadFilter(item["filter"], sprite.filter)) {
            SetError(error, "sprite filter is invalid: " + sprite.id);
            return false;
        }
        if (item.contains("wrap") && !ReadWrap(item["wrap"], sprite.wrap)) {
            SetError(error, "sprite wrap is invalid: " + sprite.id);
            return false;
        }
        if (!sprite.IsValid()) {
            SetError(error, "sprite metadata is invalid: " + sprite.id);
            return false;
        }

        const uint32_t index = static_cast<uint32_t>(loaded.sprites.size());
        sprite_ids.emplace(sprite.id, index);
        loaded.sprites.push_back(std::move(sprite));
    }

    if (!document.contains("instances") || !document["instances"].is_array()) {
        SetError(error, "scene JSON has no instances array");
        return false;
    }
    for (const Json& item : document["instances"]) {
        if (!item.is_object() || !item.contains("sprite") || !item["sprite"].is_string() ||
            !item.contains("position") || !item.contains("color") ||
            !item.contains("sorting_layer") || !item.contains("sorting_order")) {
            SetError(error, "scene instance is missing required fields");
            return false;
        }

        SpriteInstance instance;
        const std::string sprite_name = item["sprite"].get<std::string>();
        const auto sprite_it = sprite_ids.find(sprite_name);
        if (sprite_it == sprite_ids.end()) {
            SetError(error, "instance references unknown sprite: " + sprite_name);
            return false;
        }
        instance.sprite_index = sprite_it->second;

        if (!ReadVec2(item["position"], instance.position)) {
            SetError(error, "instance position is invalid");
            return false;
        }
        if (item.contains("rotation")) {
            if (!ReadFloat(item["rotation"], instance.rotation)) {
                SetError(error, "instance rotation is invalid");
                return false;
            }
        }
        if (item.contains("scale") && !ReadVec2(item["scale"], instance.scale)) {
            SetError(error, "instance scale is invalid");
            return false;
        }
        if (!ReadColor(item["color"], instance.color)) {
            SetError(error, "instance color is invalid");
            return false;
        }
        if (!item["sorting_layer"].is_number_integer() ||
            !item["sorting_order"].is_number_integer()) {
            SetError(error, "instance sorting fields are invalid");
            return false;
        }
        instance.sorting_layer = item["sorting_layer"].get<int32_t>();
        instance.sorting_order = item["sorting_order"].get<int32_t>();
        if (item.contains("flip_x") && item["flip_x"].is_boolean()) {
            instance.flip_x = item["flip_x"].get<bool>();
        }
        if (item.contains("flip_y") && item["flip_y"].is_boolean()) {
            instance.flip_y = item["flip_y"].get<bool>();
        }
        if (item.contains("path") && item["path"].is_string()) {
            instance.path = item["path"].get<std::string>();
        }
        loaded.instances.push_back(std::move(instance));
    }

    scene = std::move(loaded);
    return true;
}

} // namespace soulknight::engine::assets
