// Soul Knight Linux — deterministic projectile simulation
#include "ProjectileSystem.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::game::combat {

void ProjectileSystem::Spawn(
    const engine::assets::ProjectileDefinition& definition,
    const Vec2& position,
    const Vec2& direction,
    float speed,
    int damage,
    int camp,
    int through,
    int repel) {
    Projectile projectile;
    projectile.position = position;
    projectile.previous_position = position;
    projectile.velocity = direction * speed;
    projectile.collider_half_extents = definition.collider_half_extents;
    projectile.collider_offset = definition.collider_offset;
    projectile.damage = damage;
    projectile.camp = camp;
    projectile.through = through;
    projectile.repel = repel;
    projectile.lifetime = definition.lifetime;
    projectile.sprite_index = definition.sprite_index;
    projectile.hit_world_this_step = false;
    projectile.world_hit_time = 1.0f;
    projectiles_.push_back(projectile);
}

bool ProjectileSystem::SegmentHitsAabb(
    const Vec2& start, const Vec2& end,
    const engine::collision::Aabb& box,
    const Vec2& inflation,
    float& hit_time) {
    const engine::collision::Aabb expanded{
        {box.min.x - inflation.x, box.min.y - inflation.y},
        {box.max.x + inflation.x, box.max.y + inflation.y}
    };
    const Vec2 delta = end - start;
    float t_min = 0.0f;
    float t_max = 1.0f;
    const auto test_axis = [&](float origin, float direction,
                               float minimum, float maximum) {
        if (std::abs(direction) < 1e-8f) {
            return origin >= minimum && origin <= maximum;
        }
        float t1 = (minimum - origin) / direction;
        float t2 = (maximum - origin) / direction;
        if (t1 > t2) std::swap(t1, t2);
        t_min = std::max(t_min, t1);
        t_max = std::min(t_max, t2);
        return t_min <= t_max;
    };
    if (!test_axis(start.x, delta.x, expanded.min.x, expanded.max.x)) {
        return false;
    }
    if (!test_axis(start.y, delta.y, expanded.min.y, expanded.max.y)) {
        return false;
    }
    hit_time = t_min;
    return t_max >= 0.0f && t_min <= 1.0f;
}

Vec2 ProjectileSystem::RotatedColliderOffset(const Projectile& projectile) {
    const float length = projectile.velocity.Length();
    if (length <= 1e-8f) return projectile.collider_offset;
    const float c = projectile.velocity.x / length;
    const float s = projectile.velocity.y / length;
    return {
        projectile.collider_offset.x * c - projectile.collider_offset.y * s,
        projectile.collider_offset.x * s + projectile.collider_offset.y * c
    };
}

void ProjectileSystem::FixedUpdate(
    float delta_seconds,
    const engine::collision::AabbCollisionWorld& world) {
    if (delta_seconds <= 0.0f) return;
    world_hits_this_step_ = 0;
    lifetime_expirations_this_step_ = 0;

    for (Projectile& projectile : projectiles_) {
        projectile.hit_world_this_step = false;
        projectile.world_hit_time = 1.0f;
        if (!projectile.active) continue;
        projectile.previous_position = projectile.position;
        const Vec2 next = projectile.position + projectile.velocity * delta_seconds;
        const Vec2 offset = RotatedColliderOffset(projectile);
        const Vec2 collider_start = projectile.position + offset;
        const Vec2 collider_end = next + offset;

        float earliest = 1.0f;
        bool blocked = false;
        const auto test_solids = [&](const std::vector<engine::collision::Aabb>& boxes) {
            for (const engine::collision::Aabb& box : boxes) {
                float hit = 0.0f;
                if (SegmentHitsAabb(collider_start, collider_end, box,
                                    projectile.collider_half_extents, hit) &&
                    hit <= earliest) {
                    earliest = hit;
                    blocked = true;
                }
            }
        };
        test_solids(world.Solids());
        for (const engine::collision::OneWaySolid& solid : world.OneWaySolids()) {
            float hit = 0.0f;
            if (SegmentHitsAabb(collider_start, collider_end, solid.box,
                                projectile.collider_half_extents, hit) &&
                hit <= earliest) {
                earliest = hit;
                blocked = true;
            }
        }

        if (blocked) {
            projectile.position = projectile.position +
                (next - projectile.position) * earliest;
            projectile.active = false;
            projectile.hit_world_this_step = true;
            projectile.world_hit_time = earliest;
            ++world_hits_this_step_;
            continue;
        }

        projectile.position = next;
        projectile.age += delta_seconds;
        // Fixed-step float accumulation can land a hair below an exact
        // boundary (e.g. 250 ticks at 0.02 s); treat that as the boundary.
        if (projectile.age + 1e-4f >= projectile.lifetime) {
            projectile.active = false;
            ++lifetime_expirations_this_step_;
        }
    }
}

std::size_t ProjectileSystem::ActiveCount() const {
    std::size_t count = 0;
    for (const Projectile& projectile : projectiles_) {
        if (projectile.active) ++count;
    }
    return count;
}

bool ProjectileSystem::ResolvePlayerHits(const Vec2& player_position,
                                         HealthState& health) {
    bool damaged = false;
    const engine::collision::Aabb body{
        {player_position.x - 0.4f, player_position.y + 0.1f},
        {player_position.x + 0.4f, player_position.y + 0.9f}
    };
    for (std::size_t i = 0; i < projectiles_.size(); ++i) {
        Projectile& projectile = projectiles_[i];
        if (!projectile.active || projectile.camp == health.camp ||
            projectile.damage <= 0) {
            continue;
        }
        const engine::Vec2 offset = projectile.position +
            RotatedColliderOffset(projectile);
        const engine::collision::Aabb box{
            {offset.x - projectile.collider_half_extents.x,
             offset.y - projectile.collider_half_extents.y},
            {offset.x + projectile.collider_half_extents.x,
             offset.y + projectile.collider_half_extents.y}
        };
        if (box.max.x < body.min.x || box.min.x > body.max.x ||
            box.max.y < body.min.y || box.min.y > body.max.y) {
            continue;
        }
        DamageEvent event;
        event.damage = projectile.damage;
        event.camp = projectile.camp;
        event.through = projectile.through;
        event.repel = projectile.repel;
        if (health.ApplyDamage(event)) {
            damaged = true;
            projectile.active = false;
        }
    }
    return damaged;
}

void ProjectileSystem::Deactivate(std::size_t index) {
    if (index < projectiles_.size()) projectiles_[index].active = false;
}

} // namespace soulknight::game::combat
