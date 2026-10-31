#include "DevOverlay.hpp"

#include "engine/assets/SceneAsset.hpp"
#include "engine/locomotion/PlayerLocomotion.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/SpriteBatch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace soulknight::game::ui {

using engine::Color;
using engine::Vec2;
using engine::rendering::SortLayer;
using engine::rendering::SpriteBatch;
using engine::rendering::SpriteDraw;

namespace {

std::string Upper(std::string value) {
    for (char& character : value) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - ('a' - 'A'));
        }
    }
    return value;
}

void DrawSpriteFromScene(engine::rendering::SpriteBatch& batch,
                         const engine::assets::SceneAsset& scene,
                         int32_t sprite_index, const Vec2& position,
                         const Vec2& size, Color color, float rotation = 0.0f,
                         int32_t order = 95) {
    if (sprite_index < 0 ||
        static_cast<std::size_t>(sprite_index) >= scene.sprites.size()) {
        return;
    }
    const engine::assets::SpriteData& sprite =
        scene.sprites[static_cast<std::size_t>(sprite_index)];
    SpriteDraw draw;
    draw.position = position;
    draw.size = size;
    draw.uv = sprite.UvRect();
    draw.color = color;
    draw.layer = SortLayer::UI;
    draw.sorting_order = order;
    draw.texture_id = sprite.texture_id;
    draw.pivot = {0.5f, 0.5f};
    draw.rotation = rotation;
    (void)batch.DrawSprite(draw);
}

} // namespace

bool DevOverlay::BuildFont() {
    return text_.BuildAtlas();
}

void DevOverlay::OpenMenu(DevMenu menu, std::size_t initial_index,
                          std::size_t item_count) {
    menu_ = menu;
    item_count_ = item_count;
    selected_index_ = item_count == 0 ? 0 : std::min(initial_index, item_count - 1);
}

void DevOverlay::CloseMenu() {
    // Keep the selection so enemy spawn (F9) uses the last chosen definition
    // after the selector closes. OpenMenu() clamps it for other lists.
    menu_ = DevMenu::None;
}

void DevOverlay::Navigate(int delta) {
    if (!MenuOpen() || item_count_ == 0 || delta == 0) return;
    const int count = static_cast<int>(item_count_);
    int next = (static_cast<int>(selected_index_) + delta) % count;
    if (next < 0) next += count;
    selected_index_ = static_cast<std::size_t>(next);
}

void DevOverlay::Draw(SpriteBatch& batch, const DevOverlayContext& context) const {
    if (context.camera == nullptr) return;
    const float half_w = context.camera->HalfWidth();
    const float half_h = context.camera->HalfHeight();
    const float left = context.camera->position.x - half_w + 0.32f;
    const float top = context.camera->position.y + half_h - 0.32f;
    const float scale = std::max(0.015f, (2.0f * half_h) / 1080.0f * 1.35f);

    if (menu_ != DevMenu::None) {
        text_.DrawPanel(batch, {context.camera->position.x - 8.7f,
                                context.camera->position.y + 6.2f},
                        {17.4f, 12.4f}, Color{8, 12, 20, 232}, 90);
        text_.DrawText(batch, "LITEKNIGHT DEVELOPER SELECTOR",
                       {context.camera->position.x - 8.3f,
                        context.camera->position.y + 5.8f},
                       scale, Color{255, 224, 120, 255}, 96);
        switch (menu_) {
            case DevMenu::Characters:
                DrawCharacterMenu(batch, context, scale);
                break;
            case DevMenu::Weapons:
                DrawWeaponMenu(batch, context, scale);
                break;
            case DevMenu::Enemies:
                DrawEnemyMenu(batch, context, scale);
                break;
            case DevMenu::None:
                break;
        }
    }
    if (overlay_visible_) {
        DrawOverlay(batch, context, left, top, scale);
    }
}

void DevOverlay::DrawOverlay(SpriteBatch& batch, const DevOverlayContext& context,
                             float left, float top, float scale) const {
    const float panel_w = 12.2f;
    const float panel_h = 6.6f;
    text_.DrawPanel(batch, {left - 0.15f, top + 0.15f},
                    {panel_w, panel_h}, Color{8, 12, 20, 205}, 100);
    float y = top;
    const auto line = [&](const std::string& value) {
        text_.DrawText(batch, value, {left, y}, scale,
                       Color{235, 240, 248, 255}, 101);
        y -= text_.LineHeight(scale);
    };
    const float fps = context.frame_ms > 0.0f ? 1000.0f / context.frame_ms : 0.0f;
    line("LITEKNIGHT");
    line(std::string("VERSION: ") + context.version + "  BUILD: " + context.build);
    char buffer[192];
    std::snprintf(buffer, sizeof(buffer), "FPS: %.1f  FRAME: %.2f MS",
                  fps, context.frame_ms);
    line(buffer);
    std::snprintf(buffer, sizeof(buffer), "FIXED TICKS: %llu  FRAMES: %llu",
                  static_cast<unsigned long long>(context.simulation_ticks),
                  static_cast<unsigned long long>(context.render_frames));
    line(buffer);
    line(std::string("CHARACTER: ") +
         (context.characters != nullptr && context.characters->Count() > 0
              ? Upper(context.characters->Active().id) : "NONE"));
    line(std::string("WEAPON: ") +
         (context.weapons != nullptr && context.weapons->Count() > 0
              ? Upper(context.weapons->Active().id) : "NONE"));
    std::snprintf(buffer, sizeof(buffer), "HP: %d/%d  ARMOR: %d",
                  context.health != nullptr ? context.health->hp : 0,
                  context.health != nullptr ? context.health->max_hp : 0,
                  context.health != nullptr ? context.health->armor : 0);
    line(buffer);
    std::snprintf(buffer, sizeof(buffer), "ENEMIES: %zu  PROJECTILES: %zu",
                  context.enemies != nullptr ? context.enemies->AliveCount() : 0,
                  context.active_projectiles);
    line(buffer);
    if (context.player != nullptr) {
        std::snprintf(buffer, sizeof(buffer), "POS: %.3f, %.3f  FACING: %s",
                      context.player->Position().x, context.player->Position().y,
                      context.facing);
        line(buffer);
    }
    line(std::string("ROOM: ") + context.room_id);
    line(std::string("RENDERER: ") + context.renderer + "  PLATFORM: " +
         context.platform);
    line("F6 CHARACTERS  F7 WEAPONS  F8 ENEMIES  F10 OVERLAY");
    line("F9 SPAWN  F11 SPAWN X5  F12 CLEAR  R RESET");
}

void DevOverlay::DrawCharacterMenu(SpriteBatch& batch,
                                   const DevOverlayContext& context,
                                   float scale) const {
    if (context.scene == nullptr || context.characters == nullptr) return;
    const float center_x = context.camera->position.x;
    const float start_y = context.camera->position.y + 3.6f;
    constexpr int columns = 5;
    constexpr float cell_w = 2.45f;
    constexpr float cell_h = 2.9f;
    char label[32];
    for (std::size_t i = 0; i < context.characters->Count(); ++i) {
        const int column = static_cast<int>(i) % columns;
        const int row = static_cast<int>(i) / columns;
        const float x = center_x + (static_cast<float>(column) - 2.0f) * cell_w;
        const float y = start_y - static_cast<float>(row) * cell_h;
        if (i == selected_index_) {
            text_.DrawPanel(batch, {x - 1.1f, y + 1.4f}, {2.2f, 2.6f},
                            Color{70, 105, 160, 210}, 94);
        }
        const auto& character = context.characters->CharacterAt(i);
        DrawSpriteFromScene(batch, *context.scene, character.body_sprite_index,
                            {x, y}, {1.6f, 1.6f}, Color::White(), 0.0f, 95);
        std::snprintf(label, sizeof(label), "%s", Upper(character.id).c_str());
        text_.DrawText(batch, label, {x - 0.45f, y - 0.25f}, scale * 0.9f,
                       Color{230, 235, 245, 255}, 101);
    }
    text_.DrawText(batch, "A/D OR ARROWS: NAVIGATE   ENTER: SELECT   ESC: RETURN",
                   {context.camera->position.x - 7.5f,
                    context.camera->position.y - 5.7f},
                   scale * 0.9f, Color{180, 200, 225, 255}, 101);
}

void DevOverlay::DrawWeaponMenu(SpriteBatch& batch,
                                const DevOverlayContext& context,
                                float scale) const {
    if (context.scene == nullptr || context.weapons == nullptr) return;
    const float center_x = context.camera->position.x;
    const float start_y = context.camera->position.y + 3.5f;
    char line[160];
    for (std::size_t i = 0; i < context.weapons->Count(); ++i) {
        const auto& weapon = context.weapons->Definitions()[i];
        const float y = start_y - static_cast<float>(i) * 1.85f;
        if (i == selected_index_) {
            text_.DrawPanel(batch, {center_x - 5.7f, y + 0.85f}, {11.4f, 1.7f},
                            Color{70, 105, 160, 210}, 94);
        }
        DrawSpriteFromScene(batch, *context.scene, weapon.visual_sprite_index,
                            {center_x - 4.7f, y}, {1.5f, 1.5f},
                            Color::White(), 0.0f, 95);
        std::snprintf(line, sizeof(line), "%s  DMG %d  SPEED %.0f  CYCLE %.3f",
                      Upper(weapon.id).c_str(), weapon.damage,
                      weapon.bullet_speed, weapon.attack_cycle_seconds);
        text_.DrawText(batch, line, {center_x - 3.6f, y + 0.35f}, scale * 0.9f,
                       Color{230, 235, 245, 255}, 101);
        std::snprintf(line, sizeof(line), "PROJECTILE: %s  CONSUME: %d",
                      Upper(weapon.projectile.id).c_str(), weapon.consume);
        text_.DrawText(batch, line, {center_x - 3.6f, y - 0.1f}, scale * 0.75f,
                       Color{175, 195, 220, 255}, 101);
    }
    text_.DrawText(batch, "A/D OR ARROWS: NAVIGATE   ENTER: EQUIP   ESC: RETURN",
                   {context.camera->position.x - 7.5f,
                    context.camera->position.y - 5.7f},
                   scale * 0.9f, Color{180, 200, 225, 255}, 101);
}

void DevOverlay::DrawEnemyMenu(SpriteBatch& batch,
                               const DevOverlayContext& context,
                               float scale) const {
    if (context.scene == nullptr || context.enemies == nullptr) return;
    const float center_x = context.camera->position.x;
    const float start_y = context.camera->position.y + 3.6f;
    char line[160];
    for (std::size_t i = 0; i < context.enemies->DefinitionCount(); ++i) {
        const auto& enemy = context.enemies->DefinitionAt(i);
        const float y = start_y - static_cast<float>(i) * 2.0f;
        if (i == selected_index_) {
            text_.DrawPanel(batch, {center_x - 5.7f, y + 1.0f}, {11.4f, 1.9f},
                            Color{70, 105, 160, 210}, 94);
        }
        DrawSpriteFromScene(batch, *context.scene, enemy.body_sprite_index,
                            {center_x - 4.7f, y}, {1.6f, 1.6f},
                            Color::White(), 0.0f, 95);
        std::snprintf(line, sizeof(line), "%s  HP %d  CONTACT %d",
                      Upper(enemy.id).c_str(), enemy.hp, enemy.contact_damage);
        text_.DrawText(batch, line, {center_x - 3.6f, y + 0.35f}, scale * 0.9f,
                       Color{230, 235, 245, 255}, 101);
        std::snprintf(line, sizeof(line), "SPAWN POSITION: PLAYER");
        text_.DrawText(batch, line, {center_x - 3.6f, y - 0.1f}, scale * 0.75f,
                       Color{175, 195, 220, 255}, 101);
        text_.DrawText(batch, "F9 SPAWN   F11 SPAWN X5   F12 CLEAR",
                       {center_x - 3.6f, y - 0.45f}, scale * 0.75f,
                       Color{175, 195, 220, 255}, 101);
    }
    std::snprintf(line, sizeof(line), "ACTIVE ENEMIES: %zu",
                  context.enemies->Enemies().size());
    text_.DrawText(batch, line, {center_x - 5.7f,
                                 context.camera->position.y - 4.7f},
                   scale * 0.9f, Color{180, 200, 225, 255}, 101);
    text_.DrawText(batch, "A/D OR ARROWS: NAVIGATE   ENTER: CLOSE   ESC: RETURN",
                   {context.camera->position.x - 7.5f,
                    context.camera->position.y - 5.7f},
                   scale * 0.9f, Color{180, 200, 225, 255}, 101);
}

} // namespace soulknight::game::ui
