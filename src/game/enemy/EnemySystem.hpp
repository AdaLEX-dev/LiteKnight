// Soul Knight Linux — deterministic enemy test targets and damage
#pragma once

#include "game/combat/Damage.hpp"
#include "game/combat/ProjectileSystem.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/math/Vec2.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace soulknight::game::enemy {

struct EnemyInstance {
    std::size_t definition = 0;
    engine::Vec2 position{0.0f, 0.0f};
    combat::HealthState health;
    float idle_time = 0.0f;
    float death_timer = 0.0f;
    float contact_timer = 0.0f;
    bool active = true;
};

// Minimal e_slime01 fixture behavior: static target, contact damage, projectile
// damage and death delay. Full EnemyAI04 movement/scout logic is out of scope.
class EnemySystem {
public:
    bool Configure(const engine::assets::SceneAsset& scene, std::string* error);
    void Reset();

    // Returns true when the player was damaged this tick.
    bool FixedUpdate(float delta_seconds,
                     const engine::Vec2& player_position,
                     combat::HealthState& player_health);
    void ResolveProjectileHits(combat::ProjectileSystem& projectiles);

    bool ApplyDamage(std::size_t index, int damage, int source_camp);

    // Developer spawner API. Spawned enemies are deterministic instances of a
    // recovered definition; Reset() restores only the scene fixtures.
    bool SpawnEnemy(std::size_t definition_index, const engine::Vec2& position);
    void ClearEnemies();
    std::size_t DefinitionCount() const { return definitions_.size(); }
    const engine::assets::EnemyDefinition& DefinitionAt(std::size_t index) const {
        return definitions_[index];
    }

    const std::vector<EnemyInstance>& Enemies() const { return enemies_; }
    const std::vector<engine::assets::EnemyDefinition>& Definitions() const {
        return definitions_;
    }
    std::size_t AliveCount() const;

    static engine::collision::Aabb WorldCollider(
        const engine::assets::EnemyDefinition& definition,
        const engine::Vec2& position);

private:
    std::vector<engine::assets::EnemyDefinition> definitions_;
    std::vector<EnemyInstance> enemies_;
    std::vector<std::size_t> fixture_definitions_;
    std::vector<engine::Vec2> fixture_positions_;
    bool configured_ = false;
};

} // namespace soulknight::game::enemy
