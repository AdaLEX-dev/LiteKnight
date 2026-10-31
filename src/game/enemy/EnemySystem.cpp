// Soul Knight Linux — deterministic enemy test targets and damage
#include "EnemySystem.hpp"

#include "engine/collision/AabbCollision.hpp"

#include <algorithm>

namespace soulknight::game::enemy {
namespace {

bool Overlaps(const engine::collision::Aabb& a,
              const engine::collision::Aabb& b) {
    return a.min.x < b.max.x && a.max.x > b.min.x &&
           a.min.y < b.max.y && a.max.y > b.min.y;
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
                              combat::HealthState& player_health) {
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
            enemy.death_timer = std::max(0.0f, enemy.death_timer - delta_seconds);
            if (enemy.death_timer <= 0.0f) enemy.active = false;
            continue;
        }

        enemy.contact_timer = std::max(0.0f, enemy.contact_timer - delta_seconds);
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
