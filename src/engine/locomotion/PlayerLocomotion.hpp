// Soul Knight Linux — deterministic fixed-step player locomotion
#pragma once

#include "engine/collision/AabbCollision.hpp"
#include "engine/math/Vec2.hpp"

namespace soulknight::engine::locomotion {

struct LocomotionConfig {
    // CONFIRMED c00 RoleAttributePlayer values. The reference SetVelocity path
    // computes speed * (speed_rate + 1.0) + bonus when inertial velocity is low.
    float speed = 6.5f;
    float speed_rate = 0.5f;
    float speed_bonus = 0.0f;

    // NATIVE DESIGN: the serialized c00 collider is a trigger hurtbox, not the
    // room collision footprint. This small AABB at the feet is used only for the
    // native test chamber's wall/boundary collision.
    Vec2 collider_half_extents{0.25f, 0.25f};
    Vec2 collider_offset{0.0f, 0.0f};

    float EffectiveSpeed() const {
        return speed * (speed_rate + 1.0f) + speed_bonus;
    }
};

class PlayerLocomotion {
public:
    PlayerLocomotion() = default;
    explicit PlayerLocomotion(const Vec2& spawn, const LocomotionConfig& config = {});

    collision::CollisionResult FixedUpdate(const Vec2& input_direction,
                                           float input_weight,
                                           float delta_seconds,
                                           const collision::AabbCollisionWorld& world);

    const Vec2& Position() const { return position_; }
    const Vec2& Velocity() const { return velocity_; }
    const Vec2& DesiredDirection() const { return desired_direction_; }
    float DesiredWeight() const { return desired_weight_; }
    bool IsMoving() const { return desired_weight_ > 0.0f; }
    const collision::CollisionResult& LastCollision() const { return last_collision_; }
    const LocomotionConfig& Config() const { return config_; }

    void SetPosition(const Vec2& position) { position_ = position; }

private:
    LocomotionConfig config_{};
    Vec2 position_{0.0f, 0.0f};
    Vec2 velocity_{0.0f, 0.0f};
    Vec2 desired_direction_{0.0f, 0.0f};
    float desired_weight_ = 0.0f;
    collision::CollisionResult last_collision_{};
};

} // namespace soulknight::engine::locomotion
