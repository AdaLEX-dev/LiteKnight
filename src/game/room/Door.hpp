// Soul Knight Linux — recovered room door state machine
#pragma once

#include "engine/assets/SceneAsset.hpp"

#include <string>

namespace soulknight::game::room {

enum class DoorState {
    Open,
    Closing,
    Closed,
    Opening,
};

const char* DoorStateName(DoorState state);

struct DoorVisualState {
    bool leaf_visible = true;
    std::string leaf_sprite_id;
    bool shadow_visible = true;
    bool ornament_visible = true;
    float ornament_extent = 1.0f; // local y of the dN/d child
};

// A single recovered door. The root and leaf roots never move; only the
// serialized sprite, ornament child and renderer visibility are animated.
// Collision blocking is sampled from the recovered m_IsTrigger curve.
class Door {
public:
    Door() = default;
    explicit Door(engine::assets::SceneDoor data);

    void Open();
    void Close();
    // Returns true when the collision blocking state changed during this step.
    bool Update(float delta_seconds);

    DoorState State() const { return state_; }
    bool IsBlocking() const;
    bool IsEnabled() const { return data_.enabled; }
    bool IsOpening() const { return state_ == DoorState::Opening; }
    bool IsClosing() const { return state_ == DoorState::Closing; }
    float AnimationTime() const { return time_; }
    const engine::assets::SceneDoor& Data() const { return data_; }
    DoorVisualState Visuals() const;

private:
    const engine::assets::DoorAnimationClip& TransitionClip() const;
    float SampleTime() const;

    engine::assets::SceneDoor data_{};
    DoorState state_ = DoorState::Open;
    float time_ = 0.0f;
};

} // namespace soulknight::game::room
