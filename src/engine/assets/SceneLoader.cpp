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
        if (collision.contains("obstacle_solids")) {
            if (!collision["obstacle_solids"].is_array()) {
                SetError(error, "scene obstacle_solids is not an array");
                return false;
            }
            for (const Json& item : collision["obstacle_solids"]) {
                CollisionBox solid;
                if (!ReadCollisionBox(item, solid)) {
                    SetError(error, "scene obstacle solid is invalid");
                    return false;
                }
                loaded.obstacle_solids.push_back(solid);
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
