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

    // APPROXIMATION: the reference player has a non-trigger CircleCollider2D
    // radius 0.4 at offset (0, 0.5) on child "collider" (layer Body). The native
    // swept-AABB test uses the circle's enclosing square; the feet-anchored
    // transform remains the authoritative player position.
    Vec2 collider_half_extents{0.4f, 0.4f};
    Vec2 collider_offset{0.0f, 0.5f};

    // RGWeapon.AttackStart adds atk_move_speed to RoleAttributePlayer.speed_rate
    // while the weapon attack state is active. Zero outside attack.
    float speed_rate_bonus = 0.0f;

    float EffectiveSpeed() const {
        return speed * (speed_rate + speed_rate_bonus + 1.0f) + speed_bonus;
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
    void SetSpeedRateBonus(float bonus) { config_.speed_rate_bonus = bonus; }
    void SetSpeedProfile(float speed, float speed_rate) {
        config_.speed = speed;
        config_.speed_rate = speed_rate;
    }

private:
    LocomotionConfig config_{};
    Vec2 position_{0.0f, 0.0f};
    Vec2 velocity_{0.0f, 0.0f};
    Vec2 desired_direction_{0.0f, 0.0f};
    float desired_weight_ = 0.0f;
    collision::CollisionResult last_collision_{};
};

} // namespace soulknight::engine::locomotion
