#include "WeaponRenderer.hpp"

#include "engine/rendering/SpriteBatch.hpp"
#include "engine/rendering/WorldDepth.hpp"

#include <cmath>

namespace soulknight::game::weapons {
namespace {

float SampleCurve(const std::vector<engine::assets::WeaponVisualKey>& keys,
                  float time, float fallback) {
    if (keys.empty()) return fallback;
    if (time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;
    for (std::size_t i = 1; i < keys.size(); ++i) {
        if (time > keys[i].time) continue;
        const auto& a = keys[i - 1];
        const auto& b = keys[i];
        const float span = b.time - a.time;
        if (span <= 1e-8f) return b.value;
        return a.value + (b.value - a.value) * ((time - a.time) / span);
    }
    return keys.back().value;
}

engine::Vec2 Rotate(const engine::Vec2& value, float degrees) {
    constexpr float kPi = 3.14159265358979323846f;
    const float radians = degrees * kPi / 180.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {value.x * c - value.y * s, value.x * s + value.y * c};
}

} // namespace

void WeaponRenderer::Draw(engine::rendering::SpriteBatch& batch,
                          const engine::assets::SceneAsset& scene,
                          const engine::assets::WeaponDefinition& weapon,
                          const engine::Vec2& player_position,
                          const engine::Vec2& aim_direction,
                          bool facing_left, bool player_dead,
                          float visual_time, bool visual_attack_active,
                          float effect_scale) {
    (void)facing_left;
    DrawAt(batch, scene, weapon, player_position, aim_direction,
           {0.0f, 0.5f}, player_dead, visual_time, visual_attack_active,
           effect_scale);
}

void WeaponRenderer::DrawAt(engine::rendering::SpriteBatch& batch,
                            const engine::assets::SceneAsset& scene,
                            const engine::assets::WeaponDefinition& weapon,
                            const engine::Vec2& player_position,
                            const engine::Vec2& aim_direction,
                            const engine::Vec2& hand_offset,
                            bool player_dead, float visual_time,
                            bool visual_attack_active, float effect_scale) {
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
    const float recoil_x = visual_attack_active
        ? SampleCurve(weapon.visual_position_x, visual_time, 0.0f) *
              effect_scale
        : 0.0f;
    const float recoil_angle = visual_attack_active
        ? SampleCurve(weapon.visual_rotation_z, visual_time, 0.0f) *
              effect_scale
        : 0.0f;
    engine::rendering::SpriteDraw draw;
    draw.position = player_position + hand_offset + weapon.visual_offset +
        Rotate({recoil_x, 0.0f}, aim_angle);
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
    draw.rotation = aim_angle + weapon.visual_rotation + recoil_angle;
    (void)batch.DrawSprite(draw);

    // Recovered w/gun_point is a SpriteRenderer toggled by the attack clip's
    // m_Enabled curve. Draw it at the recovered local muzzle offset.
    if (!weapon.muzzle_sprite.empty() &&
        (weapon.muzzle_enabled.empty() ||
         SampleCurve(weapon.muzzle_enabled, visual_time, 0.0f) > 0.5f)) {
        const int32_t muzzle_index =
            scene.FindSpriteIndex(weapon.muzzle_sprite);
        if (muzzle_index >= 0 &&
            static_cast<std::size_t>(muzzle_index) < scene.sprites.size()) {
            const engine::assets::SpriteData& muzzle =
                scene.sprites[static_cast<std::size_t>(muzzle_index)];
            engine::rendering::SpriteDraw flash;
            flash.position = draw.position +
                Rotate(weapon.muzzle_offset * weapon.visual_scale,
                       draw.rotation * 180.0f /
                           3.14159265358979323846f);
            flash.size = muzzle.WorldSize() * weapon.visual_scale *
                         effect_scale;
            flash.uv = muzzle.UvRect();
            flash.color = engine::Color::White();
            flash.layer = engine::rendering::SortLayer::Character;
            flash.sorting_order = draw.sorting_order + 1;
            flash.texture_id = muzzle.texture_id;
            flash.pivot = muzzle.pivot;
            flash.flip_y = draw.flip_y;
            flash.rotation = draw.rotation;
            (void)batch.DrawSprite(flash);
        }
    }
}

} // namespace soulknight::game::weapons
