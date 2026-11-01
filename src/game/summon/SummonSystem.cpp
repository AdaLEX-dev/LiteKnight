// Soul Knight Linux — recovered summon/funnel weapon actors
#include "SummonSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace soulknight::game::summon {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float Distance(const engine::Vec2& a, const engine::Vec2& b) {
    return (b - a).Length();
}

} // namespace

void SummonSystem::Clear() {
    summons_.clear();
}

int SummonSystem::Spawn(const engine::assets::WeaponSpecial& special,
                        const engine::Vec2& origin, std::size_t count,
                        bool orbit) {
    if (special.sprite_index < 0 || special.duration <= 0.0f) return 0;
    const std::size_t spawn_count = std::max<std::size_t>(1, count);
    for (std::size_t i = 0; i < spawn_count; ++i) {
        SummonInstance summon;
        summon.sprite_index = special.sprite_index;
        summon.damage = std::max(0, special.damage);
        if (special.value > 0.0f && special.value < 1.0f) {
            summon.damage = std::max(
                0, static_cast<int>(static_cast<float>(summon.damage) *
                                    special.value));
        }
        summon.speed = special.speed > 0.0f ? special.speed : 6.0f;
        summon.attack_range = special.range > 0.0f ? special.range : 2.0f;
        summon.attack_interval = special.interval > 0.0f
            ? special.interval : 2.0f;
        summon.attack_cooldown = summon.attack_interval;
        summon.lifetime = special.duration;
        summon.orbit = orbit;
        if (orbit) {
            // FunnelWeaponCurveContainer: recovered flySpeed 40 and a close
            // orbit. The serialized maxfunnelSwordCount controls the count.
            summon.orbit_radius = 1.45f;
            summon.orbit_angle = 2.0f * kPi *
                static_cast<float>(i) / static_cast<float>(spawn_count);
            summon.position = origin + engine::Vec2{
                std::cos(summon.orbit_angle) * summon.orbit_radius,
                std::sin(summon.orbit_angle) * summon.orbit_radius};
        } else {
            const float sign = (i % 2 == 0) ? -1.0f : 1.0f;
            summon.position = origin + engine::Vec2{sign * 1.1f, 0.0f};
        }
        summons_.push_back(summon);
    }
    return static_cast<int>(spawn_count);
}

int SummonSystem::Update(
        float delta_seconds, const engine::Vec2& player_position,
        enemy::EnemySystem& enemies,
        const engine::collision::AabbCollisionWorld* world) {
    if (delta_seconds <= 0.0f) return 0;
    int landed = 0;
    for (SummonInstance& summon : summons_) {
        if (!summon.active) continue;
        summon.age += delta_seconds;
        if (summon.age + 1e-4f >= summon.lifetime) {
            summon.active = false;
            continue;
        }
        summon.attack_cooldown =
            std::max(0.0f, summon.attack_cooldown - delta_seconds);

        // Scout nearest living enemy. The recovered NpcSummon01 and
        // FunnelWeaponCurveContainer both select the nearest target within
        // their atk_range/min_distance fields.
        std::size_t target = enemies.Enemies().size();
        float target_distance = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < enemies.Enemies().size(); ++i) {
            const enemy::EnemyInstance& candidate = enemies.Enemies()[i];
            if (!candidate.active || candidate.health.dead) continue;
            const float distance = Distance(summon.position, candidate.position);
            if (distance <= std::max(6.0f, summon.attack_range) &&
                distance < target_distance) {
                target = i;
                target_distance = distance;
            }
        }

        engine::Vec2 destination = player_position;
        float stop_distance = summon.orbit ? 100.0f : 1.6f;
        if (summon.orbit) {
            summon.orbit_angle += 2.2f * delta_seconds;
            destination = player_position + engine::Vec2{
                std::cos(summon.orbit_angle) * summon.orbit_radius,
                std::sin(summon.orbit_angle) * summon.orbit_radius};
            stop_distance = 0.12f;
        } else if (target < enemies.Enemies().size()) {
            destination = enemies.Enemies()[target].position;
            stop_distance = summon.attack_range;
        }
        const float distance = Distance(summon.position, destination);
        if (summon.orbit) {
            // Direct recovered orbit curve; collision is intentionally not
            // applied to the flying sword pet.
            summon.position = destination;
        } else if (distance > stop_distance + 0.05f && distance > 0.0001f) {
            const engine::Vec2 direction =
                (destination - summon.position) * (1.0f / distance);
            const engine::Vec2 delta =
                direction * (summon.speed * delta_seconds);
            if (world != nullptr) {
                engine::Vec2 resolved = summon.position;
                (void)world->Move(summon.position, {0.28f, 0.28f}, delta,
                                  resolved);
                summon.position = resolved;
            } else {
                summon.position += delta;
            }
            if (std::fabs(direction.x) > 0.01f) {
                summon.facing_left = direction.x < 0.0f;
            }
        }

        if (target < enemies.Enemies().size() &&
            target_distance <= summon.attack_range + 0.25f &&
            summon.attack_cooldown <= 0.0f && summon.damage > 0) {
            if (enemies.ApplyDamage(target, summon.damage, 1)) ++landed;
            summon.attack_cooldown =
                std::max(0.05f, summon.attack_interval);
        }
    }
    return landed;
}

} // namespace soulknight::game::summon
