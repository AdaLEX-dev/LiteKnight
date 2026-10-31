// Soul Knight Linux — recovered room door state machine
#include "Door.hpp"

#include <algorithm>
#include <utility>

namespace soulknight::game::room {

const char* DoorStateName(DoorState state) {
    switch (state) {
        case DoorState::Open: return "OPEN";
        case DoorState::Closing: return "CLOSING";
        case DoorState::Closed: return "CLOSED";
        case DoorState::Opening: return "OPENING";
    }
    return "UNKNOWN";
}

Door::Door(engine::assets::SceneDoor data) : data_(std::move(data)) {
    // RGDoor's serialized bool open defaults to true and the Animator starts in
    // its open state. A test configuration may serialize closed.
    state_ = data_.open ? DoorState::Open : DoorState::Closed;
    const engine::assets::DoorAnimationClip& clip =
        data_.open ? data_.open_animation : data_.close_animation;
    time_ = clip.IsValid() ? clip.duration : 0.0f;
}

void Door::Open() {
    if (!data_.enabled) return;
    if (state_ == DoorState::Open) return;
    state_ = DoorState::Opening;
    time_ = 0.0f;
}

void Door::Close() {
    if (!data_.enabled) return;
    if (state_ == DoorState::Closed) return;
    state_ = DoorState::Closing;
    time_ = 0.0f;
}

bool Door::Update(float delta_seconds) {
    if (state_ != DoorState::Opening && state_ != DoorState::Closing) {
        return false;
    }
    const bool was_blocking = IsBlocking();
    const engine::assets::DoorAnimationClip& clip = TransitionClip();
    if (!clip.IsValid()) {
        state_ = state_ == DoorState::Opening ? DoorState::Open
                                              : DoorState::Closed;
        return was_blocking != IsBlocking();
    }
    time_ += std::max(delta_seconds, 0.0f);
    if (time_ >= clip.duration) {
        time_ = clip.duration;
        state_ = state_ == DoorState::Opening ? DoorState::Open
                                              : DoorState::Closed;
    }
    return was_blocking != IsBlocking();
}

bool Door::IsBlocking() const {
    if (!data_.enabled) return false;
    if (state_ == DoorState::Open) return false;
    if (state_ == DoorState::Closed) return true;
    const engine::assets::DoorAnimationClip& clip = TransitionClip();
    if (clip.collider_trigger.Empty()) return true;
    return clip.collider_trigger.EvaluateStep(time_) < 0.5f;
}

const engine::assets::DoorAnimationClip& Door::TransitionClip() const {
    if (state_ == DoorState::Opening) return data_.open_animation;
    if (state_ == DoorState::Closing) return data_.close_animation;
    return state_ == DoorState::Open ? data_.open_animation
                                     : data_.close_animation;
}

float Door::SampleTime() const {
    const engine::assets::DoorAnimationClip& clip = TransitionClip();
    if (state_ == DoorState::Open || state_ == DoorState::Closed) {
        return clip.IsValid() ? clip.duration : 0.0f;
    }
    return time_;
}

DoorVisualState Door::Visuals() const {
    DoorVisualState visuals;
    const engine::assets::DoorAnimationClip& clip = TransitionClip();
    const float sample = SampleTime();
    visuals.leaf_sprite_id = clip.SpriteAt(sample);
    const float closed_extent = 1.0f;
    const float open_extent = 0.1f;
    const float extent_fallback =
        state_ == DoorState::Open ? open_extent : closed_extent;
    visuals.ornament_extent = clip.leaf_extent.Empty()
        ? extent_fallback
        : clip.leaf_extent.Evaluate(sample);

    // Unity animations only key renderer m_Enabled at the recovered times.
    // Between keys the value is held; the native sampler treats >= 0.5 as
    // visible to preserve the discrete renderer switch.
    const float shadow_fallback = state_ == DoorState::Open ? 0.0f : 1.0f;
    const float shadow_value = clip.shadow_visible.Empty()
        ? shadow_fallback
        : clip.shadow_visible.EvaluateStep(sample);
    visuals.shadow_visible = shadow_value >= 0.5f;

    const float ornament_value = clip.ornament_visible.Empty()
        ? shadow_fallback
        : clip.ornament_visible.EvaluateStep(sample);
    visuals.ornament_visible = ornament_value >= 0.5f;
    return visuals;
}

} // namespace soulknight::game::room
