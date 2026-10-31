// Soul Knight Linux — deterministic sprite-animation playback state
#pragma once

#include "AnimationClip.hpp"

#include <cstdint>
#include <string>

namespace soulknight::engine::animation {

// This is intentionally a small state holder, not a Unity Animator clone.
// State-machine transitions, layers, blend trees, IK and animation events are
// outside the recovered subset used by the selected c00 idle clip.
class AnimationPlayer {
public:
    AnimationPlayer() = default;

    void Play(const AnimationClip& clip);
    void Stop();
    void Reset();

    // Advances clip-local time by delta_seconds. Negative or non-finite deltas
    // are ignored. Looping wraps at duration and records completed loops;
    // non-looping clips clamp to duration and stop playing.
    void Update(float delta_seconds);

    const AnimationClip* Clip() const { return clip_; }
    bool IsPlaying() const { return playing_; }
    float Time() const { return static_cast<float>(time_); }
    float NormalizedTime() const;
    uint64_t LoopCount() const { return loop_count_; }

    const SpriteKeyframe* Evaluate(const std::string& target_path) const;
    const SpriteKeyframe* Evaluate(const SpriteTrack& track) const;

private:
    const AnimationClip* clip_ = nullptr;
    double time_ = 0.0;
    uint64_t loop_count_ = 0;
    bool playing_ = false;
};

} // namespace soulknight::engine::animation
