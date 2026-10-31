// Soul Knight Linux — recovered door animation sampling
#include "DoorAnimation.hpp"

#include <algorithm>

namespace soulknight::engine::assets {

float DoorCurve::Evaluate(float time) const {
    if (keys.empty()) return 0.0f;
    if (!(time > keys.front().time)) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;

    for (std::size_t i = 1; i < keys.size(); ++i) {
        const DoorCurveKey& next = keys[i];
        if (time > next.time) continue;
        const DoorCurveKey& previous = keys[i - 1];
        const float span = next.time - previous.time;
        if (span <= 0.0f) return next.value;
        const float t = std::clamp((time - previous.time) / span, 0.0f, 1.0f);
        return previous.value + (next.value - previous.value) * t;
    }
    return keys.back().value;
}

float DoorCurve::EvaluateStep(float time) const {
    if (keys.empty()) return 0.0f;
    const float sample = time > 0.0f ? time : 0.0f;
    const DoorCurveKey* selected = &keys.front();
    for (const DoorCurveKey& key : keys) {
        if (key.time > sample) break;
        selected = &key;
    }
    return selected->value;
}

float DoorAnimationClip::SampleTime(float time) const {
    if (!(time > 0.0f)) return 0.0f;
    if (duration <= 0.0f) return 0.0f;
    return std::min(time, duration);
}

const std::string& DoorAnimationClip::SpriteAt(float time) const {
    static const std::string empty;
    if (leaf_sprites.empty()) return empty;
    const float sample = SampleTime(time);
    const DoorSpriteKey* selected = &leaf_sprites.front();
    for (const DoorSpriteKey& key : leaf_sprites) {
        if (key.time > sample) break;
        selected = &key;
    }
    return selected->sprite_id;
}

} // namespace soulknight::engine::assets
