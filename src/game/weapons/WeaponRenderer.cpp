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
                          bool player_dead) {
    if (player_dead || weapon.visual_sprite_index < 0 ||
        static_cast<std::size_t>(weapon.visual_sprite_index) >=
            scene.sprites.size()) {
        return;
    }
    const engine::assets::SpriteData& sprite =
        scene.sprites[static_cast<std::size_t>(weapon.visual_sprite_index)];
    const float aim_angle = std::atan2(aim_direction.y, aim_direction.x);
    const float c = std::cos(aim_angle);
    const float s = std::sin(aim_angle);
    const engine::Vec2 rotated_offset{
        weapon.visual_offset.x * c - weapon.visual_offset.y * s,
        weapon.visual_offset.x * s + weapon.visual_offset.y * c
    };
    engine::rendering::SpriteDraw draw;
    draw.position = player_position + engine::Vec2{0.0f, 0.5f} + rotated_offset;
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
