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

WeaponPresentation WeaponRenderer::ComputePresentation(
        const engine::assets::WeaponDefinition& weapon,
        const engine::Vec2& player_position,
        const engine::Vec2& aim_direction,
        const engine::Vec2& hand_offset,
        float visual_time,
        bool visual_attack_active,
        float effect_scale) {
    const float aim_angle = std::atan2(aim_direction.y, aim_direction.x);
    const float recoil_x = visual_attack_active
        ? SampleCurve(weapon.visual_position_x, visual_time, 0.0f) *
              effect_scale
        : 0.0f;
    const float recoil_angle = visual_attack_active
        ? SampleCurve(weapon.visual_rotation_z, visual_time, 0.0f) *
              effect_scale * 3.14159265358979323846f / 180.0f
        : 0.0f;
    WeaponPresentation result;
    result.position = player_position + hand_offset + weapon.visual_offset +
        Rotate({recoil_x, 0.0f}, aim_angle);
    result.rotation = aim_angle + weapon.visual_rotation + recoil_angle;
    const bool muzzle_enabled = visual_attack_active &&
        !weapon.muzzle_sprite.empty() &&
        (weapon.muzzle_enabled.empty() ||
         SampleCurve(weapon.muzzle_enabled, visual_time, 0.0f) > 0.5f);
    if (muzzle_enabled) {
        result.has_muzzle = true;
        result.muzzle_position = result.position +
            Rotate(weapon.muzzle_offset * weapon.visual_scale,
                   result.rotation * 180.0f / 3.14159265358979323846f);
        result.muzzle_scale = effect_scale;
    }
    return result;
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
    const WeaponPresentation presentation = ComputePresentation(
        weapon, player_position, aim_direction, hand_offset, visual_time,
        visual_attack_active, effect_scale);
    engine::rendering::SpriteDraw draw;
    draw.position = presentation.position;
    draw.size = sprite.WorldSize() * weapon.visual_scale;
    draw.uv = sprite.UvRect();
    draw.color = engine::Color::White();
    draw.layer = engine::rendering::SortLayer::Character;
    draw.sorting_order = engine::rendering::WorldDepthOrder(
        player_position.y, 30);
    draw.texture_id = sprite.texture_id;
    draw.pivot = sprite.pivot;
    draw.flip_y = aim_direction.x < -0.01f;
    draw.rotation = presentation.rotation;
    (void)batch.DrawSprite(draw);

    if (presentation.has_muzzle) {
        const int32_t muzzle_index =
            scene.FindSpriteIndex(weapon.muzzle_sprite);
        if (muzzle_index >= 0 &&
            static_cast<std::size_t>(muzzle_index) < scene.sprites.size()) {
            const engine::assets::SpriteData& muzzle =
                scene.sprites[static_cast<std::size_t>(muzzle_index)];
            engine::rendering::SpriteDraw flash;
            flash.position = presentation.muzzle_position;
            flash.size = muzzle.WorldSize() * weapon.visual_scale *
                         presentation.muzzle_scale;
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
