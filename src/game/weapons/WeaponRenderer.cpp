#include "WeaponRenderer.hpp"

#include "engine/rendering/SpriteBatch.hpp"
#include "engine/rendering/WorldDepth.hpp"

#include <cmath>

namespace soulknight::game::weapons {

void WeaponRenderer::Draw(engine::rendering::SpriteBatch& batch,
                          const engine::assets::SceneAsset& scene,
                          const engine::assets::WeaponDefinition& weapon,
                          const engine::Vec2& player_position,
                          const engine::Vec2& aim_direction,
                          bool facing_left, bool player_dead) {
    if (player_dead || weapon.visual_sprite_index < 0 ||
        static_cast<std::size_t>(weapon.visual_sprite_index) >=
            scene.sprites.size()) {
        return;
    }
    const engine::assets::SpriteData& sprite =
        scene.sprites[static_cast<std::size_t>(weapon.visual_sprite_index)];
    const float aim_angle = std::atan2(aim_direction.y, aim_direction.x);
    // The recovered weapon prefab has scale 1. The exact serialized Knight
    // hand object is not available, so this native anchor uses the recovered
    // hand region (0.2,0.55) on the body-facing side. The grip stays fixed in
    // character space while the weapon rotates; it no longer orbits with aim,
    // which removes the walking left/right wobble.
    const engine::Vec2 hand_offset{facing_left ? -0.2f : 0.2f, 0.5f};
    engine::rendering::SpriteDraw draw;
    draw.position = player_position + hand_offset + weapon.visual_offset;
    draw.size = sprite.WorldSize() * weapon.visual_scale;
    draw.uv = sprite.UvRect();
    draw.color = engine::Color::White();
    // Recovered weapon Renderers use Unity sorting layer 5, which maps to the
    // native Character layer.
    draw.layer = engine::rendering::SortLayer::Character;
    draw.sorting_order = engine::rendering::WorldDepthOrder(
        draw.position.y, 30);
    draw.texture_id = sprite.texture_id;
    draw.pivot = sprite.pivot;
    draw.rotation = aim_angle + weapon.visual_rotation;
    (void)batch.DrawSprite(draw);
}

} // namespace soulknight::game::weapons
