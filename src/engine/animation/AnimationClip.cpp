// Soul Knight Linux — native sprite animation sampling
#include "AnimationClip.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::engine::animation {

bool SpriteTrack::IsValid() const {
    if (target_path.empty() || property.empty()) return false;

    bool have_previous_time = false;
    float previous_time = 0.0f;
    for (const SpriteKeyframe& key : keys) {
        if (!std::isfinite(key.time) || key.time < 0.0f || key.sprite_id.empty()) {
            return false;
        }
        if (have_previous_time && key.time < previous_time) return false;
        previous_time = key.time;
        have_previous_time = true;
    }
    return true;
}

const SpriteKeyframe* SpriteTrack::Evaluate(float time) const {
    if (keys.empty() || !std::isfinite(time)) return nullptr;

    // Sprite references use discrete Unity PPtr curves. A sample at an exact
    // key time selects that key; a sample between keys selects the preceding
    // key. upper_bound/-- also makes duplicate-time keys deterministic: the
    // last serialized key wins.
    const auto next = std::upper_bound(
        keys.begin(), keys.end(), time,
        [](float sample_time, const SpriteKeyframe& key) {
            return sample_time < key.time;
        });
    if (next == keys.begin()) return &keys.front();
    return &*(next - 1);
}

bool AnimationClip::IsValid() const {
    if (id.empty() || !std::isfinite(duration) || duration <= 0.0f ||
        !std::isfinite(sample_rate) || sample_rate <= 0.0f) {
        return false;
    }

    for (const SpriteTrack& track : tracks) {
        if (!track.IsValid()) return false;
        if (!track.keys.empty()) {
            for (const SpriteKeyframe& key : track.keys) {
                if (key.time > duration) return false;
            }
        }
    }
    return true;
}

const SpriteTrack* AnimationClip::FindTrack(const std::string& target_path) const {
    for (const SpriteTrack& track : tracks) {
        if (track.target_path == target_path && track.property == "m_Sprite") {
            return &track;
        }
    }
    return nullptr;
}

} // namespace soulknight::engine::animation
