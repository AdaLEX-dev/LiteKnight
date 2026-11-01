// Soul Knight Linux — deterministic enemy test targets and damage
#include "EnemySystem.hpp"

#include <utility>

#include "engine/collision/AabbCollision.hpp"

#include <algorithm>

namespace soulknight::game::enemy {
namespace {

bool Overlaps(const engine::collision::Aabb& a,
              const engine::collision::Aabb& b) {
    return a.min.x < b.max.x && a.max.x > b.min.x &&
           a.min.y < b.max.y && a.max.y > b.min.y;
}

void FireVolley(const engine::assets::EnemyDefinition& definition,
                const engine::Vec2& origin, const engine::Vec2& direction,
                enemy::EnemySystem* system,
                combat::ProjectileSystem* projectiles) {
    (void)system;
    if (projectiles == nullptr || definition.projectile.sprite_index < 0) return;
    const int count = std::max(1, definition.projectile_count);
    const float fan = definition.fan_angle;
    for (int i = 0; i < count; ++i) {
        float offset = 0.0f;
        if (count > 1 && fan > 0.0f) {
            offset = (static_cast<float>(i) -
                      static_cast<float>(count) * 0.5f) * fan;
        }
        const float radians = offset * 3.14159265358979323846f / 180.0f;
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        const engine::Vec2 rotated{direction.x * c - direction.y * s,
                                   direction.x * s + direction.y * c};
        projectiles->Spawn(
            definition.projectile, origin, rotated,
            definition.projectile_speed > 0.0f
                ? definition.projectile_speed
                : definition.projectile.prefab_speed,
            definition.projectile_damage, definition.camp, 0, 0);
    }
}

} // namespace

bool EnemySystem::Configure(const engine::assets::SceneAsset& scene,
                            std::string* error) {
    definitions_ = scene.enemy_definitions;
    enemies_.clear();
    fixture_definitions_.clear();
    fixture_positions_.clear();
    configured_ = false;
    if (error != nullptr) error->clear();

    if (definitions_.empty() && scene.enemy_fixtures.empty()) {
        configured_ = true;
        return true;
    }
    if (definitions_.empty()) {
        if (error != nullptr) *error = "enemy fixtures have no definitions";
        return false;
    }
    for (const engine::assets::EnemyFixture& fixture : scene.enemy_fixtures) {
        const int32_t definition = scene.FindEnemyDefinitionIndex(fixture.definition);
        if (definition < 0) {
            if (error != nullptr) {
                *error = "enemy fixture references unknown definition: " +
                         fixture.definition;
            }
            return false;
        }
        fixture_definitions_.push_back(static_cast<std::size_t>(definition));
        fixture_positions_.push_back(fixture.position);
    }
    Reset();
    configured_ = true;
    return true;
}

bool EnemySystem::SpawnEnemy(std::size_t definition_index,
                             const engine::Vec2& position) {
    if (definition_index >= definitions_.size()) return false;
    EnemyInstance enemy;
    enemy.definition = definition_index;
    enemy.position = position;
    enemy.health.Reset(definitions_[definition_index].hp);
    enemy.health.camp = definitions_[definition_index].camp;
    enemies_.push_back(enemy);
    return true;
}

void EnemySystem::ClearEnemies() {
    enemies_.clear();
}

void EnemySystem::RestoreEnemies(std::vector<EnemyInstance> enemies) {
    enemies_ = std::move(enemies);
}

void EnemySystem::Reset() {
    enemies_.clear();
    for (std::size_t i = 0; i < fixture_definitions_.size() &&
                            i < fixture_positions_.size(); ++i) {
        (void)SpawnEnemy(fixture_definitions_[i], fixture_positions_[i]);
    }
}

engine::collision::Aabb EnemySystem::WorldCollider(
    const engine::assets::EnemyDefinition& definition,
    const engine::Vec2& position) {
    return engine::collision::Aabb{
        {position.x + definition.collider.min.x,
         position.y + definition.collider.min.y},
        {position.x + definition.collider.max.x,
         position.y + definition.collider.max.y}
    };
}

bool EnemySystem::FixedUpdate(float delta_seconds,
                              const engine::Vec2& player_position,
                              combat::HealthState& player_health,
                              const engine::collision::AabbCollisionWorld* world,
                              combat::ProjectileSystem* projectiles,
                              bool player_visible) {
    if (delta_seconds <= 0.0f) return false;
    bool player_damaged = false;
    const engine::collision::Aabb player_box{
        {player_position.x - 0.4f, player_position.y + 0.1f},
        {player_position.x + 0.4f, player_position.y + 0.9f}
    };

    for (EnemyInstance& enemy : enemies_) {
        if (!enemy.active) continue;
        const engine::assets::EnemyDefinition& definition =
            definitions_[enemy.definition];
        enemy.idle_time += delta_seconds;

        if (enemy.health.dead) {
            if (!enemy.death_exploded &&
                definition.death_explosion_radius > 0.0f &&
                definition.death_explosion_damage > 0) {
                enemy.death_exploded = true;
                const float distance =
                    (player_position - enemy.position).Length();
                if (distance <= definition.death_explosion_radius) {
                    combat::DamageEvent event;
                    event.damage = definition.death_explosion_damage;
                    event.camp = definition.camp;
                    if (player_health.ApplyDamage(event)) player_damaged = true;
                }
            }
            enemy.death_timer = std::max(0.0f, enemy.death_timer - delta_seconds);
            if (enemy.death_timer <= 0.0f) enemy.active = false;
            continue;
        }

        if (enemy.death_mark_timer > 0.0f) {
            enemy.death_mark_timer =
                std::max(0.0f, enemy.death_mark_timer - delta_seconds);
            if (enemy.death_mark_timer <= 0.0f) {
                enemy.health.hp = 0;
                enemy.health.dead = true;
                enemy.health.can_hurt = false;
                enemy.death_timer = definition.death_despawn_delay;
                continue;
            }
        }
        enemy.contact_timer = std::max(0.0f, enemy.contact_timer - delta_seconds);
        enemy.shoot_timer = std::max(0.0f, enemy.shoot_timer - delta_seconds);
        if (!player_visible) {
            // BuffStealth: enemies lose the player target while the recovered
            // 10 s stealth buff is active.
            continue;
        }

        // Chase slice: approach the player when outside contact distance and
        // inside the aggro radius. The speed is the recovered RoleAttribute
        // speed (3 for e_slime01). No pathfinding is claimed; collision sliding
        // keeps movement safe inside the recovered room geometry.
        const engine::Vec2 to_player = player_position - enemy.position;
        const float distance = to_player.Length();
        // HYPOTHESIS: the recovered prefab does not expose a native aggro radius.
        // A short radius keeps the devroom safe at spawn and makes approach
        // deliberate; full EnemyAI04 scouting/pathfinding remains UNKNOWN.
        constexpr float kAggroRadius = 5.0f;
        constexpr float kContactRange = 0.75f;
        const bool in_ranged_band = definition.ranged &&
            distance <= definition.attack_range && distance > 0.0001f;
        const engine::Vec2 direction = distance > 0.0001f
            ? to_player * (1.0f / distance) : engine::Vec2{1.0f, 0.0f};
        const engine::Vec2 center_offset{
            (definition.collider.min.x + definition.collider.max.x) * 0.5f,
            (definition.collider.min.y + definition.collider.max.y) * 0.5f};
        const engine::Vec2 spawn = enemy.position + center_offset +
            direction * 0.45f;

        // Dash/jump family: AI09, AIWitch and AIFrog use a directed high-speed
        // movement window (EnemyAI09 8.0/1.5 s; AIWitch 10.0/1.5 s;
        // AIFrog 0.5 s). Contact still uses the recovered damage/cooldown.
        if (definition.dash_speed > 0.0f && !enemy.dashing &&
            distance <= std::max(definition.attack_range, 6.0f) &&
            distance > kContactRange && enemy.shoot_timer <= 0.0f &&
            distance > 0.0001f) {
            enemy.dashing = true;
            enemy.dash_timer = std::max(0.1f, definition.dash_duration);
            enemy.dash_direction = direction;
            enemy.shoot_timer = std::max(0.1f, definition.shoot_cooldown);
            enemy.facing_left = direction.x < 0.0f;
            if (definition.projectile.sprite_index >= 0 &&
                projectiles != nullptr) {
                enemy.burst_remaining = std::max(1, definition.burst_count);
                enemy.burst_timer = 0.0f;
            }
        }
        if (enemy.dashing) {
            enemy.dash_timer -= delta_seconds;
            const engine::Vec2 step = enemy.dash_direction *
                (definition.dash_speed * delta_seconds);
            if (world != nullptr) {
                engine::Vec2 resolved = enemy.position;
                (void)world->Move(enemy.position, {0.30f, 0.30f}, step,
                                  resolved);
                enemy.position = resolved;
            } else {
                enemy.position += step;
            }
            if (definition.projectile.sprite_index >= 0 &&
                projectiles != nullptr && enemy.burst_remaining > 0) {
                enemy.burst_timer -= delta_seconds;
                if (enemy.burst_timer <= 0.0f) {
                    FireVolley(definition, enemy.position, direction, this,
                               projectiles);
                    --enemy.burst_remaining;
                    enemy.burst_timer = definition.burst_interval > 0.0f
                        ? definition.burst_interval : 0.2f;
                }
            }
            if (enemy.dash_timer <= 0.0f) enemy.dashing = false;
        } else if (definition.ranged && projectiles != nullptr &&
                   in_ranged_band &&
                   definition.projectile.sprite_index >= 0) {
            if (enemy.burst_remaining > 0) {
                enemy.burst_timer -= delta_seconds;
                if (enemy.burst_timer <= 0.0f) {
                    FireVolley(definition, spawn, direction, this, projectiles);
                    --enemy.burst_remaining;
                    enemy.burst_timer = definition.burst_interval > 0.0f
                        ? definition.burst_interval : 0.2f;
                }
            } else if (enemy.shoot_timer <= 0.0f) {
                FireVolley(definition, spawn, direction, this, projectiles);
                enemy.burst_remaining =
                    std::max(0, definition.burst_count - 1);
                enemy.burst_timer = definition.burst_interval > 0.0f
                    ? definition.burst_interval : 0.2f;
                enemy.shoot_timer =
                    std::max(0.1f, definition.shoot_cooldown);
            }
        }
        const bool hold_position = definition.stationary ||
            (definition.ranged &&
             (definition.stand_in_shooting || definition.lock_in_shooting) &&
             in_ranged_band);
        if (!enemy.dashing && !hold_position && !in_ranged_band &&
            distance > kContactRange && distance < kAggroRadius &&
            distance > 0.0001f) {
            enemy.facing_left = direction.x < 0.0f;
            const float speed = static_cast<float>(std::max(0, definition.speed));
            if (speed > 0.0f) {
                const engine::Vec2 step = direction * (speed * delta_seconds);
                if (world != nullptr) {
                    const engine::Vec2 half_extents{
                        (definition.collider.max.x - definition.collider.min.x) * 0.5f,
                        (definition.collider.max.y - definition.collider.min.y) * 0.5f};
                    engine::Vec2 resolved = enemy.position + center_offset;
                    (void)world->Move(enemy.position + center_offset,
                                      half_extents, step, resolved);
                    enemy.position = resolved - center_offset;
                } else {
                    enemy.position += step;
                }
            }
        } else if (distance > 0.0001f) {
            enemy.facing_left = to_player.x < 0.0f;
        }

        if (definition.contact_damage > 0 && enemy.contact_timer <= 0.0f &&
            player_health.Alive() &&
            Overlaps(WorldCollider(definition, enemy.position), player_box)) {
            combat::DamageEvent event;
            event.damage = definition.contact_damage;
            event.camp = definition.camp;
            if (player_health.ApplyDamage(event)) {
                player_damaged = true;
                enemy.contact_timer = definition.contact_cooldown;
            }
        }
    }
    return player_damaged;
}

void EnemySystem::ResolveProjectileHits(combat::ProjectileSystem& projectiles) {
    for (combat::Projectile& projectile : projectiles.Projectiles()) {
        // A projectile deactivated by a wall this tick may still have crossed
        // an enemy before the wall; only discount targets behind the recorded
        // wall-hit fraction. Bullets deactivated on earlier ticks are ignored.
        if (!projectile.active && !projectile.hit_world_this_step) continue;

        EnemyInstance* target = nullptr;
        float target_time = 2.0f;
        for (EnemyInstance& enemy : enemies_) {
            if (!enemy.active || enemy.health.dead) continue;
            const engine::assets::EnemyDefinition& definition =
                definitions_[enemy.definition];
            if (projectile.camp == enemy.health.camp) continue;
            const engine::Vec2 offset =
                combat::ProjectileSystem::RotatedColliderOffset(projectile);
            float hit_time = 0.0f;
            if (!combat::ProjectileSystem::SegmentHitsAabb(
                    projectile.previous_position + offset,
                    projectile.position + offset,
                    WorldCollider(definition, enemy.position),
                    projectile.collider_half_extents, hit_time)) {
                continue;
            }
            if (projectile.hit_world_this_step &&
                hit_time > projectile.world_hit_time) {
                continue;
            }
            if (hit_time < target_time) {
                target_time = hit_time;
                target = &enemy;
            }
        }
        if (target == nullptr) continue;

        const engine::assets::EnemyDefinition& definition =
            definitions_[target->definition];
        // Clip the render/simulation position to the first target hit so the
        // bullet never appears past the enemy it damaged.
        projectile.position = projectile.previous_position +
            (projectile.position - projectile.previous_position) * target_time;

        combat::DamageEvent event;
        event.damage = projectile.damage;
        event.camp = projectile.camp;
        event.through = projectile.through;
        event.repel = projectile.repel;
        if (target->health.ApplyEnemyDamage(event) && target->health.dead) {
            target->death_timer = definition.death_despawn_delay;
        }
        if (projectile.through > 0) {
            --projectile.through;
        } else {
            projectile.active = false;
        }
    }
}

int EnemySystem::ApplyMelee(const engine::Vec2& origin,
                            const engine::Vec2& direction,
                            float range, float arc_degrees,
                            int damage, int source_camp) {
    constexpr float kPi = 3.14159265358979323846f;
    const engine::Vec2 aim = direction.LengthSquared() > 1e-8f
        ? direction.Normalized() : engine::Vec2{1.0f, 0.0f};
    const float half_arc = std::clamp(arc_degrees, 1.0f, 360.0f) * 0.5f;
    int hits = 0;
    for (std::size_t i = 0; i < enemies_.size(); ++i) {
        EnemyInstance& enemy = enemies_[i];
        if (!enemy.active || enemy.health.dead) continue;
        const engine::Vec2 offset = enemy.position - origin;
        const float distance = offset.Length();
        if (distance > range + 0.8f) continue;
        if (distance > 1e-4f) {
            const float angle = std::atan2(offset.y, offset.x) * 180.0f / kPi;
            const float aim_angle = std::atan2(aim.y, aim.x) * 180.0f / kPi;
            float delta = std::fabs(angle - aim_angle);
            if (delta > 180.0f) delta = 360.0f - delta;
            if (delta > half_arc) continue;
        }
        if (ApplyDamage(i, damage, source_camp)) ++hits;
    }
    return hits;
}

bool EnemySystem::MarkForDeath(std::size_t index, float delay) {
    if (index >= enemies_.size()) return false;
    EnemyInstance& enemy = enemies_[index];
    if (!enemy.active || enemy.health.dead) return false;
    enemy.death_mark_timer = std::max(0.01f, delay);
    return true;
}

int EnemySystem::ApplyAreaDamage(const engine::Vec2& center, float radius,
                                 int damage, int source_camp) {
    int hits = 0;
    for (std::size_t i = 0; i < enemies_.size(); ++i) {
        EnemyInstance& enemy = enemies_[i];
        if (!enemy.active || enemy.health.dead) continue;
        if ((enemy.position - center).LengthSquared() > radius * radius) continue;
        if (ApplyDamage(i, damage, source_camp)) ++hits;
    }
    return hits;
}

std::size_t EnemySystem::NearestEnemy(const engine::Vec2& position,
                                      float max_range) const {
    std::size_t best = enemies_.size();
    float best_distance = max_range * max_range;
    for (std::size_t i = 0; i < enemies_.size(); ++i) {
        const EnemyInstance& enemy = enemies_[i];
        if (!enemy.active || enemy.health.dead) continue;
        const float distance = (enemy.position - position).LengthSquared();
        if (distance <= best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    return best;
}

int EnemySystem::ApplyPull(const engine::Vec2& center, float radius,
                           float strength, float delta_seconds) {
    if (delta_seconds <= 0.0f || radius <= 0.0f || strength <= 0.0f) return 0;
    int pulled = 0;
    for (EnemyInstance& enemy : enemies_) {
        if (!enemy.active || enemy.health.dead) continue;
        const engine::Vec2 delta = center - enemy.position;
        const float distance = delta.Length();
        if (distance > radius || distance <= 0.01f) continue;
        const engine::Vec2 step = delta * (strength * delta_seconds / distance);
        enemy.position += step;
        ++pulled;
    }
    return pulled;
}

bool EnemySystem::ApplyDamage(std::size_t index, int damage, int source_camp) {
    if (index >= enemies_.size()) return false;
    EnemyInstance& enemy = enemies_[index];
    if (!enemy.active) return false;
    combat::DamageEvent event;
    event.damage = damage;
    event.camp = source_camp;
    if (!enemy.health.ApplyEnemyDamage(event)) return false;
    if (enemy.health.dead) {
        enemy.death_timer = definitions_[enemy.definition].death_despawn_delay;
    }
    return true;
}

std::size_t EnemySystem::AliveCount() const {
    std::size_t count = 0;
    for (const EnemyInstance& enemy : enemies_) {
        if (enemy.active && !enemy.health.dead) ++count;
    }
    return count;
}

} // namespace soulknight::game::enemy
