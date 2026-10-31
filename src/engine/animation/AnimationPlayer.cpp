// Soul Knight Linux — deterministic sprite-animation playback state
#include "AnimationPlayer.hpp"

#include <cmath>

namespace soulknight::engine::animation {

void AnimationPlayer::Play(const AnimationClip& clip) {
    clip_ = &clip;
    time_ = 0.0;
    loop_count_ = 0;
    playing_ = true;
}

void AnimationPlayer::Stop() {
    playing_ = false;
}

void AnimationPlayer::Reset() {
    time_ = 0.0;
    loop_count_ = 0;
    playing_ = clip_ != nullptr;
}

void AnimationPlayer::Update(float delta_seconds) {
    if (clip_ == nullptr || !playing_ || !std::isfinite(delta_seconds) ||
        delta_seconds < 0.0f) {
        return;
    }

    const double duration = static_cast<double>(clip_->duration);
    if (duration <= 0.0) {
        playing_ = false;
        time_ = 0.0;
        return;
    }

    time_ += static_cast<double>(delta_seconds);
    // Accumulated fixed-step floats can land infinitesimally below an exact
    // clip boundary. Treat that as the boundary rather than leaking an extra
    // key interval; this is a sampling tolerance, not time quantization.
    constexpr double kBoundaryEpsilon = 1e-6;
    if (time_ + kBoundaryEpsilon < duration) return;
    if (time_ < duration) time_ = duration;

    if (!clip_->loop) {
        time_ = duration;
        playing_ = false;
        return;
    }

    const double completed_loops = std::floor(time_ / duration);
    loop_count_ += static_cast<uint64_t>(completed_loops);
    time_ = std::fmod(time_, duration);
    if (time_ < 0.0) time_ += duration;
    // At an exact duration boundary fmod returns 0, which is the start of the
    // next loop. Keep the accumulated loop count and the wrapped time.
}

float AnimationPlayer::NormalizedTime() const {
    if (clip_ == nullptr || clip_->duration <= 0.0f) return 0.0f;
    const float normalized = static_cast<float>(time_ / static_cast<double>(clip_->duration));
    if (clip_->loop) {
        return normalized >= 1.0f ? 0.0f : normalized;
    }
    return normalized > 1.0f ? 1.0f : normalized;
}

const SpriteKeyframe* AnimationPlayer::Evaluate(const std::string& target_path) const {
    if (clip_ == nullptr) return nullptr;
    const SpriteTrack* track = clip_->FindTrack(target_path);
    return track != nullptr ? Evaluate(*track) : nullptr;
}

const SpriteKeyframe* AnimationPlayer::Evaluate(const SpriteTrack& track) const {
    return track.Evaluate(static_cast<float>(time_));
}

} // namespace soulknight::engine::animation
