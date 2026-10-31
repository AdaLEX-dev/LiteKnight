// Soul Knight Linux — minimal idle/run sprite animation state
#pragma once

#include "AnimationPlayer.hpp"

#include <string>

namespace soulknight::engine::animation {

enum class LocomotionState {
    Idle,
    Run
};

// The recovered c00 controller has many states, but this milestone only needs
// the evidence-backed idle/run switch. Transition rules beyond movement presence
// are deliberately not modeled.
class LocomotionAnimator {
public:
    LocomotionAnimator() = default;

    // Pointers must outlive the animator; SceneAsset owns the clips.
    void Configure(const AnimationClip* idle, const AnimationClip* run);
    void Update(bool moving, float delta_seconds);
    void Reset();

    LocomotionState State() const { return state_; }
    const AnimationClip* CurrentClip() const { return player_.Clip(); }
    const AnimationClip* IdleClip() const { return idle_; }
    const AnimationClip* RunClip() const { return run_; }
    bool IsPlaying() const { return player_.IsPlaying(); }
    float Time() const { return player_.Time(); }
    uint64_t LoopCount() const { return player_.LoopCount(); }

    const SpriteKeyframe* Evaluate(const std::string& target_path) const {
        return player_.Evaluate(target_path);
    }

private:
    const AnimationClip* idle_ = nullptr;
    const AnimationClip* run_ = nullptr;
    LocomotionState state_ = LocomotionState::Idle;
    AnimationPlayer player_;
};

inline const char* LocomotionStateName(LocomotionState state) {
    return state == LocomotionState::Run ? "run" : "ide";
}

} // namespace soulknight::engine::animation
