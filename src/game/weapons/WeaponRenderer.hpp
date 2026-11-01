// Soul Knight Linux — recovered weapon visual binding
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/math/Vec2.hpp"

namespace soulknight::engine::rendering {
class SpriteBatch;
}

namespace soulknight::game::weapons {

// Draws the recovered weapon prefab `w` sprite at the player's hand. Rendering
// is intentionally separate from WeaponDefinition and Weapon attack behavior.
class WeaponRenderer {
public:
    static void Draw(engine::rendering::SpriteBatch& batch,
                     const engine::assets::SceneAsset& scene,
                     const engine::assets::WeaponDefinition& weapon,
                     const engine::Vec2& player_position,
                     const engine::Vec2& aim_direction,
                     bool facing_left, bool player_dead,
                     float visual_time = 0.0f,
                     bool visual_attack_active = false,
                     float effect_scale = 1.0f);
    // Draw at an explicit c00 hand mount (h1, h2, h3). The local offset is
    // mirrored with body facing, matching the flipped img transform.
    static void DrawAt(engine::rendering::SpriteBatch& batch,
                       const engine::assets::SceneAsset& scene,
                       const engine::assets::WeaponDefinition& weapon,
                       const engine::Vec2& player_position,
                       const engine::Vec2& aim_direction,
                       const engine::Vec2& hand_offset,
                       bool player_dead,
                       float visual_time = 0.0f,
                       bool visual_attack_active = false,
                       float effect_scale = 1.0f);
};

} // namespace soulknight::game::weapons
