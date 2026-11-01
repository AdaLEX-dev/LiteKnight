// Soul Knight Linux — minimal idle/run sprite animation state
#pragma once

#include "AnimationPlayer.hpp"

#include <string>

namespace soulknight::engine::animation {

enum class LocomotionState {
    Idle,
    Run,
    Dead,
    Hit
};

// The recovered c00 controller has many states, but this milestone only needs
// the evidence-backed idle/run switch. Transition rules beyond movement presence
// are deliberately not modeled.
class LocomotionAnimator {
public:
    LocomotionAnimator() = default;

    // Pointers must outlive the animator; SceneAsset owns the clips.
    void Configure(const AnimationClip* idle, const AnimationClip* run,
                   const AnimationClip* death = nullptr,
                   const AnimationClip* hit = nullptr);
    void Update(bool moving, float delta_seconds);
    bool PlayDead();
    bool PlayHit();
    void Reset();

    LocomotionState State() const { return state_; }
    const AnimationClip* CurrentClip() const { return player_.Clip(); }
    const AnimationClip* IdleClip() const { return idle_; }
    const AnimationClip* RunClip() const { return run_; }
    const AnimationClip* DeathClip() const { return death_; }
    const AnimationClip* HitClip() const { return hit_; }
    bool DeadPlaying() const { return dead_playing_; }
    bool IsPlaying() const { return player_.IsPlaying(); }
    float Time() const { return player_.Time(); }
    uint64_t LoopCount() const { return player_.LoopCount(); }

    const SpriteKeyframe* Evaluate(const std::string& target_path) const {
        return player_.Evaluate(target_path);
    }

private:
    const AnimationClip* idle_ = nullptr;
    const AnimationClip* run_ = nullptr;
    const AnimationClip* death_ = nullptr;
    LocomotionState state_ = LocomotionState::Idle;
    bool dead_playing_ = false;
    bool hit_playing_ = false;
    const AnimationClip* hit_ = nullptr;
    AnimationPlayer player_;
};

inline const char* LocomotionStateName(LocomotionState state) {
    if (state == LocomotionState::Run) return "run";
    if (state == LocomotionState::Dead) return "dead";
    if (state == LocomotionState::Hit) return "hit";
    return "ide";
}

} // namespace soulknight::engine::animation
