// Soul Knight Linux — recovered summon/funnel weapon actors
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "engine/math/Vec2.hpp"
#include "game/enemy/EnemySystem.hpp"

#include <cstddef>
#include <vector>

namespace soulknight::game::summon {

struct SummonInstance {
    engine::Vec2 position{0.0f, 0.0f};
    int32_t sprite_index = -1;
    int damage = 0;
    float speed = 0.0f;
    float attack_range = 0.0f;
    float attack_cooldown = 0.0f;
    float attack_interval = 2.0f;
    float lifetime = 0.0f;
    float age = 0.0f;
    float orbit_radius = 0.0f;
    float orbit_angle = 0.0f;
    bool orbit = false;
    bool facing_left = false;
    bool active = true;
};

// Native subset of the recovered NpcSummon01 / NpcMercenaryController phantom /
// FunnelWeaponCurveContainer families. The serialized prefab fields decide
// lifetime, HP, damage, speed, attack cadence and sprite; no generic
// projectile substitute is used.
class SummonSystem {
public:
    void Clear();
    // `count` spawns for summon weapons; funnel weapons pass orbit=true.
    int Spawn(const engine::assets::WeaponSpecial& special,
              const engine::Vec2& origin, std::size_t count, bool orbit);

    // Follow/orbit the player and attack the nearest living enemy. Returns the
    // number of attacks that landed this tick.
    int Update(float delta_seconds, const engine::Vec2& player_position,
               enemy::EnemySystem& enemies,
               const engine::collision::AabbCollisionWorld* world);

    const std::vector<SummonInstance>& Summons() const { return summons_; }

private:
    std::vector<SummonInstance> summons_;
};

} // namespace soulknight::game::summon
