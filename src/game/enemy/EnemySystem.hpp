// Soul Knight Linux — deterministic enemy test targets and damage
#pragma once

#include "game/combat/Damage.hpp"
#include "game/combat/ProjectileSystem.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
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
    bool facing_left = false;
    bool active = true;
};

// Minimal e_slime01 fixture behavior: static target, contact damage, projectile
// damage and death delay. Full EnemyAI04 movement/scout logic is out of scope.
class EnemySystem {
public:
    bool Configure(const engine::assets::SceneAsset& scene, std::string* error);
    void Reset();

    // Returns true when the player was damaged this tick. Enemies pursue the
    // player while outside contact range and slide against the supplied world
    // collision geometry. The chase is a deliberate first-slice approximation:
    // the recovered e_slime01 has speed/contact fields but no native pathfinding
    // yet, so no obstacle avoidance beyond collision sliding is implied.
    bool FixedUpdate(float delta_seconds,
                     const engine::Vec2& player_position,
                     combat::HealthState& player_health,
                     const engine::collision::AabbCollisionWorld* world = nullptr);
    void ResolveProjectileHits(combat::ProjectileSystem& projectiles);

    bool ApplyDamage(std::size_t index, int damage, int source_camp);

    // Developer spawner API. Spawned enemies are deterministic instances of a
    // recovered definition; Reset() restores only the scene fixtures.
    bool SpawnEnemy(std::size_t definition_index, const engine::Vec2& position);
    void ClearEnemies();
    // Room-state preservation: restore a previously captured instance list.
    // Definitions are owned by the scene and must match the same room.
    void RestoreEnemies(std::vector<EnemyInstance> enemies);
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
