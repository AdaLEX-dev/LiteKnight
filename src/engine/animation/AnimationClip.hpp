// Soul Knight Linux — native sprite animation data
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace soulknight::engine::animation {

// A discrete sprite reference at a clip-local time. The sprite ID resolves to
// engine::assets::SpriteData; it deliberately carries no texture or transform
// semantics of its own.
struct SpriteKeyframe {
    float time = 0.0f;
    std::string sprite_id;
};

// One Unity SpriteRenderer.m_Sprite binding, expressed without Unity objects.
struct SpriteTrack {
    std::string target_path;
    std::string property = "m_Sprite";
    std::vector<SpriteKeyframe> keys;

    bool IsValid() const;
    const SpriteKeyframe* Evaluate(float time) const;
};

// Provenance captured from the source Unity assets. These IDs are metadata and
// are not required for sampling.
struct AnimationSource {
    std::string apk_sha256;
    std::string bundle;
    std::string bundle_sha256;
    std::string animator_game_object_path_id;
    std::string animator_component_path_id;
    std::string controller_path_id;
    std::string controller_name;
    std::string state_machine_index;
    std::string state_machine_layer;
    std::string state_path_id;
    std::string state_name;
    std::string clip_path_id;
    std::string clip_name;
    std::string binding_path;
    std::string binding_property;
    std::string binding_path_hash;
    std::string pptr_curve_index;
    std::vector<std::string> sprite_mapping_path_ids;
};

// Unity Animator update mode values: Normal=0, AnimatePhysics=1, UnscaledTime=2.
enum class AnimationUpdateMode : uint8_t {
    Normal = 0,
    AnimatePhysics = 1,
    UnscaledTime = 2
};

struct AnimationClip {
    std::string id;
    float duration = 0.0f;
    float sample_rate = 0.0f;
    bool loop = false;
    AnimationUpdateMode update_mode = AnimationUpdateMode::Normal;
    std::vector<SpriteTrack> tracks;
    AnimationSource source;

    bool IsValid() const;
    const SpriteTrack* FindTrack(const std::string& target_path) const;
};

} // namespace soulknight::engine::animation
