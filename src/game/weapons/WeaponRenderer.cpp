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
    (void)facing_left;
    DrawAt(batch, scene, weapon, player_position, aim_direction,
           {0.0f, 0.5f}, player_dead);
}

void WeaponRenderer::DrawAt(engine::rendering::SpriteBatch& batch,
                            const engine::assets::SceneAsset& scene,
                            const engine::assets::WeaponDefinition& weapon,
                            const engine::Vec2& player_position,
                            const engine::Vec2& aim_direction,
                            const engine::Vec2& hand_offset,
                            bool player_dead) {
    if (player_dead || weapon.visual_sprite_index < 0 ||
        static_cast<std::size_t>(weapon.visual_sprite_index) >=
            scene.sprites.size()) {
        return;
    }
    const engine::assets::SpriteData& sprite =
        scene.sprites[static_cast<std::size_t>(weapon.visual_sprite_index)];
    const float aim_angle = std::atan2(aim_direction.y, aim_direction.x);
    // CONFIRMED c00 prefab hierarchy: img/h1 is the primary RGHand at local
    // (0, 0.5); h2 (0.55, 0.6) and h3 (0.15, 0.85) are the off-hand mounts.
    // The caller supplies the mount; the grip receives no guessed sideways
    // offset. Rotate around the sprite pivot near the handle.
    engine::rendering::SpriteDraw draw;
    draw.position = player_position + hand_offset + weapon.visual_offset;
    draw.size = sprite.WorldSize() * weapon.visual_scale;
    draw.uv = sprite.UvRect();
    draw.color = engine::Color::White();
    // Recovered weapon Renderers use Unity sorting layer 5, which maps to the
    // native Character layer.
    draw.layer = engine::rendering::SortLayer::Character;
    // Sort against the character's feet, not the hand position. The recovered
    // weapon root is at hand height (player.y + 0.5); using that Y lowered its
    // depth band by 50 and hid the gun behind the body.
    draw.sorting_order = engine::rendering::WorldDepthOrder(
        player_position.y, 30);
    draw.texture_id = sprite.texture_id;
    draw.pivot = sprite.pivot;
    // HYPOTHESIS: the reference flips rotating sprites through FlipWhenRotate
    // (confirmed on bullet `b`); the same convention keeps a left-aimed weapon
    // upright instead of presenting it upside-down.
    draw.flip_y = aim_direction.x < -0.01f;
    draw.rotation = aim_angle + weapon.visual_rotation;
    (void)batch.DrawSprite(draw);
}

} // namespace soulknight::game::weapons
