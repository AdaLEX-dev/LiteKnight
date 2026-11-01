// Soul Knight Linux — deterministic projectile simulation
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "engine/math/Vec2.hpp"
#include "game/combat/Damage.hpp"

#include <cstddef>
#include <vector>

namespace soulknight::game::combat {

using engine::Vec2;

struct Projectile {
    Vec2 position{0.0f, 0.0f};
    Vec2 previous_position{0.0f, 0.0f};
    Vec2 velocity{0.0f, 0.0f};
    Vec2 collider_half_extents{0.25f, 0.20f};
    // Recovered child `b` BoxCollider2D offset; rotated with the projectile.
    Vec2 collider_offset{0.25f, 0.0f};
    int damage = 0;
    int camp = 0;
    int through = 0;
    int repel = 0;
    float lifetime = 5.0f;
    float age = 0.0f;
    int32_t sprite_index = -1;
    bool active = true;
    // Set by FixedUpdate when this tick's sweep ended at a static solid.
    // EnemySystem uses it to reject targets behind the wall-hit point.
    bool hit_world_this_step = false;
    float world_hit_time = 1.0f;
};

class ProjectileSystem {
public:
    void Clear() { projectiles_.clear(); }

    void Spawn(const engine::assets::ProjectileDefinition& definition,
               const Vec2& position,
               const Vec2& direction,
               float speed,
               int damage,
               int camp,
               int through,
               int repel);

    // Deterministic 50 Hz movement: segment sweep against static room,
    // obstacle and door colliders, then lifetime expiration.
    void FixedUpdate(float delta_seconds,
                     const engine::collision::AabbCollisionWorld& world);

    std::vector<Projectile>& Projectiles() { return projectiles_; }
    const std::vector<Projectile>& Projectiles() const { return projectiles_; }

    std::size_t ActiveCount() const;
    void Deactivate(std::size_t index);
    // Enemy bullets (camp 0) damage the player. Returns true on a hit.
    bool ResolvePlayerHits(const Vec2& player_position, HealthState& health);
    int WorldHitsThisStep() const { return world_hits_this_step_; }
    int LifetimeExpirationsThisStep() const {
        return lifetime_expirations_this_step_;
    }

    static Vec2 RotatedColliderOffset(const Projectile& projectile);

    // Exposed for deterministic collision tests.
    static bool SegmentHitsAabb(const Vec2& start, const Vec2& end,
                                const engine::collision::Aabb& box,
                                const Vec2& inflation,
                                float& hit_time);

private:
    std::vector<Projectile> projectiles_;
    int world_hits_this_step_ = 0;
    int lifetime_expirations_this_step_ = 0;
};

} // namespace soulknight::game::combat
