// Soul Knight Linux — deterministic fixed-step player locomotion
#include "PlayerLocomotion.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::engine::locomotion {

PlayerLocomotion::PlayerLocomotion(const Vec2& spawn,
                                   const LocomotionConfig& config)
    : config_(config), position_(spawn) {}

collision::CollisionResult PlayerLocomotion::FixedUpdate(
    const Vec2& input_direction, float input_weight, float delta_seconds,
    const collision::AabbCollisionWorld& world) {
    last_collision_ = {};
    velocity_ = {0.0f, 0.0f};
    desired_direction_ = {0.0f, 0.0f};
    desired_weight_ = 0.0f;

    if (delta_seconds <= 0.0f || input_weight <= 0.0f) return last_collision_;

    const float length = input_direction.Length();
    if (length <= 0.0f || !std::isfinite(length)) return last_collision_;

    desired_direction_ = input_direction / length;
    desired_weight_ = std::clamp(input_weight, 0.0f, 1.0f);

    const Vec2 requested_delta = desired_direction_ *
        (config_.EffectiveSpeed() * desired_weight_ * delta_seconds);
    Vec2 center = position_ + config_.collider_offset;
    Vec2 resolved_center = center;
    last_collision_ = world.Move(center, config_.collider_half_extents,
                                 requested_delta, resolved_center);

    const Vec2 new_position = resolved_center - config_.collider_offset;
    velocity_ = (new_position - position_) / delta_seconds;
    position_ = new_position;
    return last_collision_;
}

} // namespace soulknight::engine::locomotion
