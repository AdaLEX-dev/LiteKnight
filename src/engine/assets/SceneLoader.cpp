// Soul Knight Linux — native scene loader implementation
#include "SceneLoader.hpp"

#include "PngDecoder.hpp"
#include "engine/animation/AnimationClipLoader.hpp"
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

bool ReadCollisionBox(const Json& value, CollisionBox& output) {
    if (!value.is_array() || value.size() != 4) return false;
    for (const Json& element : value) {
        if (!element.is_number()) return false;
    }
    output.min = {value[0].get<float>(), value[1].get<float>()};
    output.max = {value[2].get<float>(), value[3].get<float>()};
    return output.IsValid();
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

bool ReadDoorOneWayBlock(const Json& value, DoorOneWayBlock& output) {
    if (!value.is_string()) return false;
    const std::string name = value.get<std::string>();
    if (name == "none") { output = DoorOneWayBlock::None; return true; }
    if (name == "positive_x") { output = DoorOneWayBlock::PositiveX; return true; }
    if (name == "negative_x") { output = DoorOneWayBlock::NegativeX; return true; }
    if (name == "positive_y") { output = DoorOneWayBlock::PositiveY; return true; }
    if (name == "negative_y") { output = DoorOneWayBlock::NegativeY; return true; }
    return false;
}

bool ReadDoorCurve(const Json& value, DoorCurve& output) {
    if (!value.is_array()) return false;
    output.keys.clear();
    float previous_time = 0.0f;
    bool first = true;
    for (const Json& item : value) {
        if (!item.is_array() || item.size() != 2 ||
            !item[0].is_number() || !item[1].is_number()) {
            return false;
        }
        const float time = item[0].get<float>();
        const float key_value = item[1].get<float>();
        if (time < 0.0f || (!first && time < previous_time)) return false;
        output.keys.push_back(DoorCurveKey{time, key_value});
        previous_time = time;
        first = false;
    }
    return true;
}

bool ReadDoorSpriteKeys(const Json& value, std::vector<DoorSpriteKey>& output) {
    if (!value.is_array()) return false;
    output.clear();
    float previous_time = 0.0f;
    bool first = true;
    for (const Json& item : value) {
        if (!item.is_array() || item.size() != 2 ||
            !item[0].is_number() || !item[1].is_string()) {
            return false;
        }
        const float time = item[0].get<float>();
        if (time < 0.0f || (!first && time < previous_time)) return false;
        output.push_back(DoorSpriteKey{time, item[1].get<std::string>()});
        previous_time = time;
        first = false;
    }
    return true;
}

bool ReadDoorAnimationClip(const Json& value, DoorAnimationClip& output) {
    if (!value.is_object()) return false;
    output = {};
    output.id = value.value("id", std::string{});
    if (!value.contains("duration") ||
        !ReadFloat(value["duration"], output.duration) ||
        output.duration <= 0.0f) {
        return false;
    }
    if (value.contains("sample_rate")) {
        if (!ReadFloat(value["sample_rate"], output.sample_rate)) return false;
    }
    output.loop = value.value("loop", false);

    const struct {
        const char* name;
        DoorCurve* curve;
    } curves[] = {
        {"leaf_extent", &output.leaf_extent},
        {"shadow_visible", &output.shadow_visible},
        {"ornament_visible", &output.ornament_visible},
        {"collider_trigger", &output.collider_trigger},
        {"collider_size_x", &output.collider_size_x},
        {"collider_size_y", &output.collider_size_y},
    };
    for (const auto& entry : curves) {
        if (!value.contains(entry.name)) continue;
        if (!ReadDoorCurve(value[entry.name], *entry.curve)) return false;
    }
    if (value.contains("leaf_sprites") &&
        !ReadDoorSpriteKeys(value["leaf_sprites"], output.leaf_sprites)) {
        return false;
    }
    return true;
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
    loaded.spawn = loaded.camera.position;
    if (document.contains("spawn") && !ReadVec2(document["spawn"], loaded.spawn)) {
        SetError(error, "scene spawn is invalid");
        return false;
    }
    if (document.contains("collision")) {
        const Json& collision = document["collision"];
        if (!collision.is_object() || !collision.contains("bounds") ||
            !ReadRect(collision["bounds"], loaded.collision_bounds) ||
            loaded.collision_bounds.w <= 0.0f ||
            loaded.collision_bounds.h <= 0.0f) {
            SetError(error, "scene collision bounds are invalid");
            return false;
        }
        if (collision.contains("interior_bounds")) {
            if (!ReadRect(collision["interior_bounds"], loaded.interior_bounds) ||
                loaded.interior_bounds.w <= 0.0f ||
                loaded.interior_bounds.h <= 0.0f) {
                SetError(error, "scene interior collision bounds are invalid");
                return false;
            }
        }
        if (collision.contains("solids")) {
            if (!collision["solids"].is_array()) {
                SetError(error, "scene collision solids is not an array");
                return false;
            }
            for (const Json& item : collision["solids"]) {
                CollisionBox solid;
                if (!ReadCollisionBox(item, solid)) {
                    SetError(error, "scene collision solid is invalid");
                    return false;
                }
                loaded.collision_solids.push_back(solid);
            }
        }
        if (collision.contains("doors")) {
            if (!collision["doors"].is_array()) {
                SetError(error, "scene collision doors is not an array");
                return false;
            }
            for (const Json& item : collision["doors"]) {
                if (!item.is_object() || !item.contains("id") ||
                    !item["id"].is_string() ||
                    !item.contains("barrier") || !item.contains("trigger")) {
                    SetError(error, "scene door entry is invalid");
                    return false;
                }
                SceneDoor door;
                door.id = item["id"].get<std::string>();
                if (loaded.FindDoorIndex(door.id) >= 0) {
                    SetError(error, "duplicate scene door id: " + door.id);
                    return false;
                }
                if (!ReadCollisionBox(item["barrier"], door.barrier) ||
                    !ReadCollisionBox(item["trigger"], door.trigger)) {
                    SetError(error, "scene door geometry is invalid: " + door.id);
                    return false;
                }
                if (item.contains("aisle") && item["aisle"].is_string()) {
                    door.aisle = item["aisle"].get<std::string>();
                }
                if (item.contains("door_path") && item["door_path"].is_string()) {
                    door.door_path_id = item["door_path"].get<std::string>();
                }
                if (item.contains("trigger_path") &&
                    item["trigger_path"].is_string()) {
                    door.trigger_path_id = item["trigger_path"].get<std::string>();
                }
                if (item.contains("direction") && item["direction"].is_number_integer()) {
                    door.direction = item["direction"].get<int32_t>();
                }
                if (item.contains("enter_exit_direction") &&
                    item["enter_exit_direction"].is_number_integer()) {
                    door.enter_exit_direction =
                        item["enter_exit_direction"].get<int32_t>();
                }
                if (item.contains("enabled") && item["enabled"].is_boolean()) {
                    door.enabled = item["enabled"].get<bool>();
                }
                if (item.contains("one_way_block")) {
                    if (!ReadDoorOneWayBlock(item["one_way_block"],
                                             door.one_way_block)) {
                        SetError(error, "scene door one_way_block is invalid: " +
                                        door.id);
                        return false;
                    }
                }
                if (item.contains("open") && item["open"].is_boolean()) {
                    door.open = item["open"].get<bool>();
                }
                if (item.contains("open_animation")) {
                    if (!ReadDoorAnimationClip(item["open_animation"],
                                               door.open_animation)) {
                        SetError(error, "scene door open_animation is invalid: " +
                                        door.id);
                        return false;
                    }
                }
                if (item.contains("close_animation")) {
                    if (!ReadDoorAnimationClip(item["close_animation"],
                                               door.close_animation)) {
                        SetError(error, "scene door close_animation is invalid: " +
                                        door.id);
                        return false;
                    }
                }
                loaded.doors.push_back(std::move(door));
            }
        }
        loaded.has_room_topology =
            !loaded.collision_solids.empty() || !loaded.doors.empty();
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

    if (document.contains("obstacles")) {
        if (!document["obstacles"].is_object()) {
            SetError(error, "scene obstacles is not an object");
            return false;
        }
        const Json& obstacles = document["obstacles"];
        if (!obstacles.contains("variants") || !obstacles["variants"].is_array()) {
            SetError(error, "scene obstacles.variants is not an array");
            return false;
        }
        for (const Json& item : obstacles["variants"]) {
            if (!item.is_object() || !item.contains("name") ||
                !item["name"].is_string() || !item.contains("visuals") ||
                !item["visuals"].is_array()) {
                SetError(error, "scene obstacle variant is invalid");
                return false;
            }
            ObstacleVariant variant;
            variant.name = item["name"].get<std::string>();
            variant.index = item.value("index", static_cast<int32_t>(
                loaded.obstacle_variants.size()));
            variant.path_id = item.value("path_id", std::string{});
            if (item.contains("collider") && !item["collider"].is_null()) {
                const Json& collider = item["collider"];
                if (!collider.is_object() || !collider.contains("size") ||
                    !collider.contains("offset")) {
                    SetError(error, "scene obstacle collider is invalid: " +
                                    variant.name);
                    return false;
                }
                Vec2 size;
                Vec2 offset;
                if (!ReadVec2(collider["size"], size) ||
                    !ReadVec2(collider["offset"], offset) ||
                    size.x <= 0.0f || size.y <= 0.0f) {
                    SetError(error, "scene obstacle collider geometry is invalid: " +
                                    variant.name);
                    return false;
                }
                variant.has_collider = true;
                variant.collider_trigger = collider.value("trigger", false);
                variant.collider.min = {
                    offset.x - size.x * 0.5f,
                    offset.y - size.y * 0.5f
                };
                variant.collider.max = {
                    offset.x + size.x * 0.5f,
                    offset.y + size.y * 0.5f
                };
            }
            for (const Json& visual_item : item["visuals"]) {
                if (!visual_item.is_object() ||
                    !visual_item.contains("sprite") ||
                    !visual_item["sprite"].is_string() ||
                    !visual_item.contains("offset") ||
                    !visual_item.contains("color")) {
                    SetError(error, "scene obstacle visual is invalid: " +
                                    variant.name);
                    return false;
                }
                ObstacleVisual visual;
                const auto sprite_it = sprite_ids.find(
                    visual_item["sprite"].get<std::string>());
                if (sprite_it == sprite_ids.end()) {
                    SetError(error, "obstacle visual references unknown sprite: " +
                                    visual_item["sprite"].get<std::string>());
                    return false;
                }
                visual.sprite_index = static_cast<int32_t>(sprite_it->second);
                if (!ReadVec2(visual_item["offset"], visual.offset) ||
                    !ReadColor(visual_item["color"], visual.color)) {
                    SetError(error, "scene obstacle visual fields are invalid: " +
                                    variant.name);
                    return false;
                }
                if (visual_item.contains("rotation") &&
                    !ReadFloat(visual_item["rotation"], visual.rotation)) {
                    SetError(error, "scene obstacle visual rotation is invalid");
                    return false;
                }
                if (visual_item.contains("scale") &&
                    !ReadVec2(visual_item["scale"], visual.scale)) {
                    SetError(error, "scene obstacle visual scale is invalid");
                    return false;
                }
                visual.sorting_layer =
                    visual_item.value("sorting_layer", 0);
                visual.sorting_order =
                    visual_item.value("sorting_order", 0);
                visual.flip_x = visual_item.value("flip_x", false);
                visual.flip_y = visual_item.value("flip_y", false);
                variant.visuals.push_back(visual);
            }
            loaded.obstacle_variants.push_back(std::move(variant));
        }
        if (!obstacles.contains("markers") || !obstacles["markers"].is_array()) {
            SetError(error, "scene obstacles.markers is not an array");
            return false;
        }
        for (const Json& item : obstacles["markers"]) {
            if (!item.is_object() || !item.contains("position")) {
                SetError(error, "scene obstacle marker is invalid");
                return false;
            }
            ObstacleMarker marker;
            if (!ReadVec2(item["position"], marker.position)) {
                SetError(error, "scene obstacle marker position is invalid");
                return false;
            }
            marker.variant = item.value("variant", item.value("obj_index", 0));
            if (marker.variant < 0 ||
                static_cast<std::size_t>(marker.variant) >=
                    loaded.obstacle_variants.size()) {
                SetError(error, "scene obstacle marker variant is invalid");
                return false;
            }
            marker.name = item.value("name", std::string{});
            marker.path = item.value("path", std::string{});
            loaded.obstacle_markers.push_back(std::move(marker));
        }
    }

    if (document.contains("animations")) {
        if (!document["animations"].is_array()) {
            SetError(error, "scene JSON animations is not an array");
            return false;
        }
        for (const Json& item : document["animations"]) {
            if (!item.is_string()) {
                SetError(error, "scene animation entry is not a path");
                return false;
            }
            const std::filesystem::path animation_path =
                scene_path.parent_path() / item.get<std::string>();
            engine::animation::AnimationClip clip;
            std::string animation_error;
            if (!engine::animation::AnimationClipLoader::Load(
                    filesystem, animation_path, clip, &animation_error)) {
                SetError(error, "animation load failed: " + animation_error);
                return false;
            }
            for (const engine::animation::SpriteTrack& track : clip.tracks) {
                for (const engine::animation::SpriteKeyframe& key : track.keys) {
                    if (sprite_ids.find(key.sprite_id) == sprite_ids.end()) {
                        SetError(error, "animation references unknown sprite: " +
                                        key.sprite_id);
                        return false;
                    }
                }
            }
            loaded.animations.push_back(std::move(clip));
        }
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
        if (item.contains("door") && item["door"].is_string()) {
            instance.door = item["door"].get<std::string>();
            if (loaded.FindDoorIndex(instance.door) < 0) {
                SetError(error, "instance references unknown door: " + instance.door);
                return false;
            }
        }
        if (item.contains("door_role") && item["door_role"].is_string()) {
            instance.door_role = item["door_role"].get<std::string>();
            if (instance.door_role != "leaf" &&
                instance.door_role != "shadow" &&
                instance.door_role != "ornament") {
                SetError(error, "instance has invalid door_role: " +
                                instance.door_role);
                return false;
            }
            if (instance.door.empty()) {
                SetError(error, "instance has door_role without door");
                return false;
            }
        }
        if (item.contains("door_leaf") && item["door_leaf"].is_string()) {
            instance.door_leaf = item["door_leaf"].get<std::string>();
            if (instance.door_leaf.empty()) {
                SetError(error, "instance has empty door_leaf");
                return false;
            }
            if (instance.door_role.empty()) {
                SetError(error, "instance has door_leaf without door_role");
                return false;
            }
        }
        loaded.instances.push_back(std::move(instance));
    }

    for (const engine::animation::AnimationClip& clip : loaded.animations) {
        for (const engine::animation::SpriteTrack& track : clip.tracks) {
            bool target_found = false;
            for (const SpriteInstance& instance : loaded.instances) {
                if (instance.path == track.target_path) {
                    target_found = true;
                    break;
                }
            }
            if (!target_found) {
                SetError(error, "animation target instance not found: " +
                                track.target_path);
                return false;
            }
        }
    }

    if (document.contains("characters")) {
        if (!document["characters"].is_array()) {
            SetError(error, "scene characters is not an array");
            return false;
        }
        for (const Json& item : document["characters"]) {
            if (!item.is_object() || !item.contains("id") ||
                !item["id"].is_string() || !item.contains("body_sprite") ||
                !item["body_sprite"].is_string() ||
                !item.contains("idle_animation") ||
                !item["idle_animation"].is_string() ||
                !item.contains("run_animation") ||
                !item["run_animation"].is_string()) {
                SetError(error, "scene character entry is invalid");
                return false;
            }
            SceneCharacter character;
            character.id = item["id"].get<std::string>();
            for (const SceneCharacter& existing : loaded.characters) {
                if (existing.id == character.id) {
                    SetError(error, "duplicate scene character id: " +
                                    character.id);
                    return false;
                }
            }
            character.controller_name =
                item.value("controller_name", std::string{});
            character.controller_path_id =
                item.value("controller_path_id", std::string{});
            character.source_path_id =
                item.value("source_path_id", std::string{});
            character.body_sprite_id =
                item["body_sprite"].get<std::string>();
            const auto sprite_it = sprite_ids.find(character.body_sprite_id);
            if (sprite_it == sprite_ids.end()) {
                SetError(error, "character references unknown body sprite: " +
                                character.body_sprite_id);
                return false;
            }
            character.body_sprite_index = static_cast<int32_t>(sprite_it->second);
            if (item.contains("dead_sprite") && item["dead_sprite"].is_string()) {
                character.dead_sprite_id =
                    item["dead_sprite"].get<std::string>();
                const auto dead_it = sprite_ids.find(character.dead_sprite_id);
                if (dead_it == sprite_ids.end()) {
                    SetError(error, "character references unknown dead sprite: " +
                                    character.dead_sprite_id);
                    return false;
                }
                character.dead_sprite_index =
                    static_cast<int32_t>(dead_it->second);
            }
            character.idle_animation_id =
                item["idle_animation"].get<std::string>();
            character.run_animation_id =
                item["run_animation"].get<std::string>();
            if (loaded.FindAnimationIndex(character.idle_animation_id) < 0 ||
                loaded.FindAnimationIndex(character.run_animation_id) < 0) {
                SetError(error, "character references unknown animation: " +
                                character.id);
                return false;
            }
            loaded.characters.push_back(std::move(character));
        }
    }

    if (document.contains("player_stats")) {
        if (!document["player_stats"].is_object()) {
            SetError(error, "scene player_stats is not an object");
            return false;
        }
        const Json& item = document["player_stats"];
        loaded.player_stats.character_id =
            item.value("character_id", std::string{});
        loaded.player_stats.max_hp = item.value("max_hp", 0);
        loaded.player_stats.hp = item.value("hp", loaded.player_stats.max_hp);
        loaded.player_stats.max_armor = item.value("max_armor", 0);
        loaded.player_stats.armor =
            item.value("armor", loaded.player_stats.max_armor);
        loaded.player_stats.camp = item.value("camp", 1);
        loaded.player_stats.atk = item.value("atk", 0);
        loaded.player_stats.critical = item.value("critical", 0);
        loaded.player_stats.speed = item.value("speed", 0.0f);
        loaded.player_stats.speed_rate = item.value("speed_rate", 0.0f);
        loaded.player_stats.deviation = item.value("deviation", 0);
        if (loaded.player_stats.max_hp <= 0) {
            SetError(error, "player_stats max_hp must be positive");
            return false;
        }
    }

    if (document.contains("weapons")) {
        if (!document["weapons"].is_array()) {
            SetError(error, "scene weapons is not an array");
            return false;
        }
        for (const Json& item : document["weapons"]) {
            if (!item.is_object() || !item.contains("id") ||
                !item["id"].is_string() || !item.contains("projectile") ||
                !item["projectile"].is_object()) {
                SetError(error, "scene weapon entry is invalid");
                return false;
            }
            WeaponDefinition weapon;
            weapon.id = item["id"].get<std::string>();
            const Json& projectile = item["projectile"];
            if (!projectile.contains("id") || !projectile["id"].is_string() ||
                !projectile.contains("sprite") || !projectile["sprite"].is_string()) {
                SetError(error, "scene projectile entry is invalid");
                return false;
            }
            weapon.projectile.id = projectile["id"].get<std::string>();
            const auto sprite_it =
                sprite_ids.find(projectile["sprite"].get<std::string>());
            if (sprite_it == sprite_ids.end()) {
                SetError(error, "projectile references unknown sprite: " +
                                projectile["sprite"].get<std::string>());
                return false;
            }
            weapon.projectile.sprite_index =
                static_cast<int32_t>(sprite_it->second);
            weapon.projectile.prefab_speed =
                projectile.value("prefab_speed", 0.0f);
            weapon.projectile.lifetime =
                projectile.value("lifetime", 5.0f);
            weapon.projectile.camp = projectile.value("camp", 1);
            weapon.projectile.can_rebound =
                projectile.value("can_rebound", false);
            if (projectile.contains("collider_size") &&
                projectile.contains("collider_offset")) {
                Vec2 size;
                Vec2 offset;
                if (!ReadVec2(projectile["collider_size"], size) ||
                    !ReadVec2(projectile["collider_offset"], offset) ||
                    size.x <= 0.0f || size.y <= 0.0f) {
                    SetError(error, "projectile collider is invalid");
                    return false;
                }
                weapon.projectile.collider_half_extents =
                    {size.x * 0.5f, size.y * 0.5f};
                weapon.projectile.collider_offset = offset;
            }
            weapon.projectile.sorting_layer =
                projectile.value("sorting_layer", 6);
            weapon.projectile.sorting_order =
                projectile.value("sorting_order", 0);
            weapon.bullet_speed = item.value("bullet_speed", 0.0f);
            weapon.size = item.value("size", 1.0f);
            weapon.damage = item.value("damage", 0);
            weapon.deviation = item.value("deviation", 0);
            weapon.owner_deviation = item.value("owner_deviation", 0);
            weapon.consume = item.value("consume", 0);
            weapon.weapon_speed = item.value("weapon_speed", 1.0f);
            weapon.atk_move_speed = item.value("atk_move_speed", 0.0f);
            weapon.repel = item.value("repel", 0);
            weapon.through = item.value("through", 0);
            weapon.attack_cycle_seconds =
                item.value("attack_cycle_seconds", 1.0f / 3.0f);
            weapon.first_shot_delay_seconds =
                item.value("first_shot_delay_seconds", 1.0f / 30.0f);
            if (item.contains("visual_sprite") &&
                item["visual_sprite"].is_string()) {
                const auto visual_it =
                    sprite_ids.find(item["visual_sprite"].get<std::string>());
                if (visual_it == sprite_ids.end()) {
                    SetError(error, "weapon references unknown visual sprite: " +
                                    item["visual_sprite"].get<std::string>());
                    return false;
                }
                weapon.visual_sprite_index =
                    static_cast<int32_t>(visual_it->second);
            }
            if (item.contains("visual_offset")) {
                if (!ReadVec2(item["visual_offset"], weapon.visual_offset)) {
                    SetError(error, "weapon visual_offset is invalid: " + weapon.id);
                    return false;
                }
            }
            if (item.contains("muzzle_offset")) {
                if (!ReadVec2(item["muzzle_offset"], weapon.muzzle_offset) ||
                    weapon.muzzle_offset.LengthSquared() <= 0.0f) {
                    SetError(error, "weapon muzzle_offset is invalid: " + weapon.id);
                    return false;
                }
            }
            weapon.visual_rotation = item.value("visual_rotation", 0.0f);
            weapon.sorting_layer = item.value("sorting_layer", 5);
            weapon.sorting_order = item.value("sorting_order", 1);
            if (weapon.attack_cycle_seconds <= 0.0f ||
                weapon.first_shot_delay_seconds < 0.0f) {
                SetError(error, "weapon attack timing is invalid: " + weapon.id);
                return false;
            }
            loaded.weapons.push_back(std::move(weapon));
        }
    }

    if (document.contains("enemy_definitions")) {
        if (!document["enemy_definitions"].is_array()) {
            SetError(error, "scene enemy_definitions is not an array");
            return false;
        }
        for (const Json& item : document["enemy_definitions"]) {
            if (!item.is_object() || !item.contains("id") ||
                !item["id"].is_string() || !item.contains("body_sprite") ||
                !item["body_sprite"].is_string()) {
                SetError(error, "scene enemy definition is invalid");
                return false;
            }
            EnemyDefinition enemy;
            enemy.id = item["id"].get<std::string>();
            const auto body_it =
                sprite_ids.find(item["body_sprite"].get<std::string>());
            if (body_it == sprite_ids.end()) {
                SetError(error, "enemy references unknown body sprite: " +
                                item["body_sprite"].get<std::string>());
                return false;
            }
            enemy.body_sprite_index = static_cast<int32_t>(body_it->second);
            if (item.contains("dead_sprite") && item["dead_sprite"].is_string()) {
                const auto dead_it =
                    sprite_ids.find(item["dead_sprite"].get<std::string>());
                if (dead_it == sprite_ids.end()) {
                    SetError(error, "enemy references unknown dead sprite: " +
                                    item["dead_sprite"].get<std::string>());
                    return false;
                }
                enemy.dead_sprite_index = static_cast<int32_t>(dead_it->second);
            }
            if (item.contains("shadow_sprite") &&
                item["shadow_sprite"].is_string()) {
                const auto shadow_it =
                    sprite_ids.find(item["shadow_sprite"].get<std::string>());
                if (shadow_it == sprite_ids.end()) {
                    SetError(error, "enemy references unknown shadow sprite: " +
                                    item["shadow_sprite"].get<std::string>());
                    return false;
                }
                enemy.shadow_sprite_index =
                    static_cast<int32_t>(shadow_it->second);
            }
            enemy.hp = item.value("hp", 0);
            enemy.speed = item.value("speed", 0);
            enemy.camp = item.value("camp", 0);
            enemy.contact_damage = item.value("contact_damage", 0);
            enemy.contact_cooldown = item.value("contact_cooldown", 1.5f);
            enemy.death_despawn_delay =
                item.value("death_despawn_delay", 4.0f);
            enemy.body_sorting_layer = item.value("body_sorting_layer", 5);
            enemy.body_sorting_order = item.value("body_sorting_order", 1);
            enemy.shadow_sorting_layer = item.value("shadow_sorting_layer", 3);
            enemy.shadow_sorting_order = item.value("shadow_sorting_order", 0);
            if (!item.contains("collider_size") ||
                !item.contains("collider_offset")) {
                SetError(error, "enemy collider is missing: " + enemy.id);
                return false;
            }
            Vec2 size;
            Vec2 offset;
            if (!ReadVec2(item["collider_size"], size) ||
                !ReadVec2(item["collider_offset"], offset) ||
                size.x <= 0.0f || size.y <= 0.0f) {
                SetError(error, "enemy collider is invalid: " + enemy.id);
                return false;
            }
            enemy.collider.min = {offset.x - size.x * 0.5f,
                                  offset.y - size.y * 0.5f};
            enemy.collider.max = {offset.x + size.x * 0.5f,
                                  offset.y + size.y * 0.5f};
            loaded.enemy_definitions.push_back(std::move(enemy));
        }
    }

    if (document.contains("enemy_fixtures")) {
        if (!document["enemy_fixtures"].is_array()) {
            SetError(error, "scene enemy_fixtures is not an array");
            return false;
        }
        for (const Json& item : document["enemy_fixtures"]) {
            if (!item.is_object() || !item.contains("definition") ||
                !item["definition"].is_string() || !item.contains("position")) {
                SetError(error, "scene enemy fixture is invalid");
                return false;
            }
            EnemyFixture fixture;
            fixture.definition = item["definition"].get<std::string>();
            if (loaded.FindEnemyDefinitionIndex(fixture.definition) < 0) {
                SetError(error, "enemy fixture references unknown definition: " +
                                fixture.definition);
                return false;
            }
            if (!ReadVec2(item["position"], fixture.position)) {
                SetError(error, "enemy fixture position is invalid");
                return false;
            }
            loaded.enemy_fixtures.push_back(std::move(fixture));
        }
    }

    // Build door leaf bindings from the explicit door_role/door_leaf fields.
    // A scene without role metadata (for example the synthetic loader fixture)
    // remains valid; an animated r1_1 scene supplies all three roles per leaf.
    for (std::size_t i = 0; i < loaded.instances.size(); ++i) {
        const SpriteInstance& instance = loaded.instances[i];
        if (instance.door_role.empty()) continue;
        const int32_t door_index = loaded.FindDoorIndex(instance.door);
        if (door_index < 0) continue;
        SceneDoor& door = loaded.doors[static_cast<std::size_t>(door_index)];
        const std::string& leaf_name = instance.door_leaf;
        if (leaf_name.empty()) {
            SetError(error, "door instance has no leaf name: " + instance.path);
            return false;
        }
        DoorLeafBinding* leaf = nullptr;
        for (DoorLeafBinding& candidate : door.leaves) {
            if (candidate.name == leaf_name) {
                leaf = &candidate;
                break;
            }
        }
        if (leaf == nullptr) {
            door.leaves.push_back(DoorLeafBinding{leaf_name, -1, -1, -1});
            leaf = &door.leaves.back();
        }
        const int32_t index = static_cast<int32_t>(i);
        if (instance.door_role == "leaf") {
            leaf->leaf_instance = index;
        } else if (instance.door_role == "shadow") {
            leaf->shadow_instance = index;
        } else {
            leaf->ornament_instance = index;
        }
    }

    for (const SceneDoor& door : loaded.doors) {
        for (const DoorAnimationClip* clip : {&door.open_animation,
                                              &door.close_animation}) {
            for (const DoorSpriteKey& key : clip->leaf_sprites) {
                if (sprite_ids.find(key.sprite_id) == sprite_ids.end()) {
                    SetError(error, "door animation references unknown sprite: " +
                                    key.sprite_id);
                    return false;
                }
            }
        }
    }

    scene = std::move(loaded);
    return true;
}

} // namespace soulknight::engine::assets
