// Soul Knight Linux — native animation clip loader implementation
#include "AnimationClipLoader.hpp"

#include "platform/filesystem/FileSystem.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace soulknight::engine::animation {
namespace {

using Json = nlohmann::json;

void SetError(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
}

bool ReadString(const Json& object, const char* key, std::string& output) {
    if (!object.contains(key) || !object[key].is_string()) return false;
    output = object[key].get<std::string>();
    return true;
}

bool ReadFloat(const Json& object, const char* key, float& output) {
    if (!object.contains(key) || !object[key].is_number()) return false;
    output = object[key].get<float>();
    return std::isfinite(output);
}

bool ReadBool(const Json& object, const char* key, bool& output) {
    if (!object.contains(key) || !object[key].is_boolean()) return false;
    output = object[key].get<bool>();
    return true;
}

bool ReadStringArray(const Json& value, std::vector<std::string>& output) {
    if (!value.is_array()) return false;
    output.clear();
    for (const Json& element : value) {
        if (!element.is_string()) return false;
        output.push_back(element.get<std::string>());
    }
    return true;
}

bool ReadOptionalSourceString(const Json& source, const char* key, std::string& output) {
    if (!source.contains(key)) return true;
    return ReadString(source, key, output);
}

bool ParseSource(const Json& document, AnimationSource& source, std::string* error) {
    if (!document.contains("source")) return true;
    const Json& value = document["source"];
    if (!value.is_object()) {
        SetError(error, "animation source is not an object");
        return false;
    }

    if (!ReadOptionalSourceString(value, "apk_sha256", source.apk_sha256) ||
        !ReadOptionalSourceString(value, "bundle", source.bundle) ||
        !ReadOptionalSourceString(value, "bundle_sha256", source.bundle_sha256) ||
        !ReadOptionalSourceString(value, "animator_game_object_path_id",
                                  source.animator_game_object_path_id) ||
        !ReadOptionalSourceString(value, "animator_component_path_id",
                                  source.animator_component_path_id) ||
        !ReadOptionalSourceString(value, "controller_path_id", source.controller_path_id) ||
        !ReadOptionalSourceString(value, "controller_name", source.controller_name) ||
        !ReadOptionalSourceString(value, "state_machine_index", source.state_machine_index) ||
        !ReadOptionalSourceString(value, "state_machine_layer", source.state_machine_layer) ||
        !ReadOptionalSourceString(value, "state_path_id", source.state_path_id) ||
        !ReadOptionalSourceString(value, "state_name", source.state_name) ||
        !ReadOptionalSourceString(value, "clip_path_id", source.clip_path_id) ||
        !ReadOptionalSourceString(value, "clip_name", source.clip_name) ||
        !ReadOptionalSourceString(value, "binding_path", source.binding_path) ||
        !ReadOptionalSourceString(value, "binding_property", source.binding_property) ||
        !ReadOptionalSourceString(value, "binding_path_hash", source.binding_path_hash) ||
        !ReadOptionalSourceString(value, "pptr_curve_index", source.pptr_curve_index)) {
        SetError(error, "animation source contains an invalid string field");
        return false;
    }
    if (value.contains("sprite_mapping_path_ids") &&
        !ReadStringArray(value["sprite_mapping_path_ids"],
                         source.sprite_mapping_path_ids)) {
        SetError(error, "animation source sprite_mapping_path_ids is invalid");
        return false;
    }
    return true;
}

bool ParseUpdateMode(const Json& document, AnimationUpdateMode& mode,
                     std::string* error) {
    mode = AnimationUpdateMode::Normal;
    if (!document.contains("update_mode")) return true;

    const Json& value = document["update_mode"];
    if (value.is_number_integer()) {
        const int integer = value.get<int>();
        if (integer < 0 || integer > 2) {
            SetError(error, "animation update_mode is out of range");
            return false;
        }
        mode = static_cast<AnimationUpdateMode>(integer);
        return true;
    }
    if (!value.is_string()) {
        SetError(error, "animation update_mode is invalid");
        return false;
    }

    const std::string name = value.get<std::string>();
    if (name == "normal") {
        mode = AnimationUpdateMode::Normal;
    } else if (name == "animate_physics") {
        mode = AnimationUpdateMode::AnimatePhysics;
    } else if (name == "unscaled_time") {
        mode = AnimationUpdateMode::UnscaledTime;
    } else {
        SetError(error, "animation update_mode is unknown: " + name);
        return false;
    }
    return true;
}

bool ParseTrack(const Json& value, float duration, SpriteTrack& track,
                std::string* error) {
    if (!value.is_object() || !value.contains("target_path") ||
        !value["target_path"].is_string() || !value.contains("keys") ||
        !value["keys"].is_array()) {
        SetError(error, "animation track is missing target_path/keys");
        return false;
    }

    track = SpriteTrack{};
    track.target_path = value["target_path"].get<std::string>();
    if (value.contains("property")) {
        if (!value["property"].is_string()) {
            SetError(error, "animation track property is invalid");
            return false;
        }
        track.property = value["property"].get<std::string>();
    }
    if (track.property != "m_Sprite") {
        SetError(error, "animation track property is unsupported: " + track.property);
        return false;
    }

    for (const Json& key_value : value["keys"]) {
        if (!key_value.is_object() || !key_value.contains("time") ||
            !key_value.contains("sprite")) {
            SetError(error, "animation key is missing time/sprite");
            return false;
        }
        SpriteKeyframe key;
        if (!ReadFloat(key_value, "time", key.time) ||
            !ReadString(key_value, "sprite", key.sprite_id) ||
            key.time < 0.0f || key.time > duration) {
            SetError(error, "animation key is invalid");
            return false;
        }
        track.keys.push_back(std::move(key));
    }

    if (!track.IsValid()) {
        SetError(error, "animation track is invalid: " + track.target_path);
        return false;
    }
    return true;
}

} // namespace

bool AnimationClipLoader::Parse(const std::string& text, AnimationClip& clip,
                                std::string* error) {
    if (error != nullptr) error->clear();

    const Json document = Json::parse(text, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        SetError(error, "animation JSON is malformed");
        return false;
    }

    AnimationClip loaded;
    if (!ReadString(document, "id", loaded.id) ||
        !ReadFloat(document, "duration", loaded.duration) ||
        !ReadFloat(document, "sample_rate", loaded.sample_rate) ||
        !ReadBool(document, "loop", loaded.loop)) {
        SetError(error, "animation JSON is missing id/duration/sample_rate/loop");
        return false;
    }
    if (!ParseUpdateMode(document, loaded.update_mode, error)) return false;
    if (!ParseSource(document, loaded.source, error)) return false;

    if (!document.contains("tracks") || !document["tracks"].is_array()) {
        SetError(error, "animation JSON has no tracks array");
        return false;
    }
    for (const Json& track_value : document["tracks"]) {
        SpriteTrack track;
        if (!ParseTrack(track_value, loaded.duration, track, error)) return false;
        for (const SpriteTrack& existing : loaded.tracks) {
            if (existing.target_path == track.target_path &&
                existing.property == track.property) {
                SetError(error, "duplicate animation track: " + track.target_path);
                return false;
            }
        }
        loaded.tracks.push_back(std::move(track));
    }

    if (!loaded.IsValid()) {
        SetError(error, "animation clip metadata is invalid");
        return false;
    }

    clip = std::move(loaded);
    return true;
}

bool AnimationClipLoader::Load(const platform::FileSystem& filesystem,
                               const std::filesystem::path& path,
                               AnimationClip& clip,
                               std::string* error) {
    if (error != nullptr) error->clear();
    const auto text = filesystem.ReadTextFile(path);
    if (!text) {
        SetError(error, "animation JSON could not be read: " + path.string());
        return false;
    }
    return Parse(*text, clip, error);
}

} // namespace soulknight::engine::animation
