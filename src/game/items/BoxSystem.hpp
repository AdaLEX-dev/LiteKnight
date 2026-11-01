// Soul Knight Linux — recovered RGBox breakables and RGEnergy bubbles
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"
#include "engine/math/Vec2.hpp"
#include "game/combat/ProjectileSystem.hpp"
#include "game/room/ObstacleField.hpp"

#include <cstddef>
#include <vector>

namespace soulknight::game::items {

struct BreakableBox {
    std::size_t marker_index = 0;
    engine::Vec2 position{0.0f, 0.0f};
    int hp = 4;
    int max_hp = 4;
    bool alive = true;
    bool energy_pot = false;
    float hit_flash = 0.0f;
};

struct EnergyBubble {
    engine::Vec2 position{0.0f, 0.0f};
    engine::Vec2 velocity{0.0f, 0.0f};
    float age = 0.0f;
    bool active = true;
};

// Native RGBox/RGEnergy runtime. Boxes are the actual recovered obstacle
// markers whose MapManagerLevel.obstacle_list variant carries RGBox. Each break
// spawns the recovered RGEnergy pickup (value 8, speed 14, range 6).
class BoxSystem {
public:
    void Configure(const engine::assets::SceneAsset& scene,
                   const room::ObstacleField& obstacles);
    void Reset();

    // Returns the number of boxes hit.
    int ApplyMelee(const engine::Vec2& origin, const engine::Vec2& direction,
                   float range, float arc_degrees, int damage);
    int ApplyProjectiles(combat::ProjectileSystem& projectiles,
                         int player_camp);
    // Moves bubbles toward the player and returns the absorbed energy value.
    int Update(float delta_seconds, const engine::Vec2& player_position);

    const std::vector<BreakableBox>& Boxes() const { return boxes_; }
    const std::vector<EnergyBubble>& Bubbles() const { return bubbles_; }

private:
    bool Damage(std::size_t index, int damage);
    void SpawnBubble(const engine::Vec2& position);
    static engine::collision::Aabb BoxBounds(const BreakableBox& box);

    std::vector<BreakableBox> boxes_;
    std::vector<EnergyBubble> bubbles_;
};

} // namespace soulknight::game::items
