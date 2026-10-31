// Soul Knight Linux — minimal idle/run sprite animation state
#include "LocomotionAnimator.hpp"

namespace soulknight::engine::animation {

void LocomotionAnimator::Configure(const AnimationClip* idle,
                                   const AnimationClip* run,
                                   const AnimationClip* death) {
    idle_ = idle;
    run_ = run;
    death_ = death;
    dead_playing_ = false;
    state_ = LocomotionState::Idle;
    player_.Stop();

    const AnimationClip* initial = idle_ != nullptr ? idle_ : run_;
    if (initial != nullptr) {
        player_.Play(*initial);
        if (initial == run_ && idle_ == nullptr) state_ = LocomotionState::Run;
    }
}

bool LocomotionAnimator::PlayDead() {
    if (death_ == nullptr) return false;
    dead_playing_ = true;
    state_ = LocomotionState::Dead;
    player_.Play(*death_);
    return true;
}

void LocomotionAnimator::Update(bool moving, float delta_seconds) {
    if (dead_playing_) {
        player_.Update(delta_seconds);
        return;
    }
    const AnimationClip* desired = moving && run_ != nullptr ? run_ : idle_;
    if (desired == nullptr) {
        player_.Stop();
        return;
    }

    const LocomotionState desired_state =
        desired == run_ ? LocomotionState::Run : LocomotionState::Idle;
    if (player_.Clip() != desired) {
        state_ = desired_state;
        player_.Play(*desired);
    } else {
        state_ = desired_state;
    }
    player_.Update(delta_seconds);
}

void LocomotionAnimator::Reset() {
    state_ = LocomotionState::Idle;
    dead_playing_ = false;
    player_.Stop();
    const AnimationClip* initial = idle_ != nullptr ? idle_ : run_;
    if (initial != nullptr) {
        player_.Play(*initial);
        state_ = initial == run_ && idle_ == nullptr
            ? LocomotionState::Run
            : LocomotionState::Idle;
    }
}

} // namespace soulknight::engine::animation
