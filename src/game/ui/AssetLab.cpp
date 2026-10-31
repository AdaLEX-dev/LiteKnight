#include "AssetLab.hpp"

#include "DevOverlay.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/DebugText.hpp"
#include "engine/rendering/SpriteBatch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace soulknight::game::ui {
namespace {

using engine::Color;
using engine::Vec2;
using engine::rendering::SortLayer;

constexpr int kCategoryCount = static_cast<int>(AssetCategory::Count);

const char* MenuName(AssetCategory category) {
    switch (category) {
        case AssetCategory::Characters: return "CHARACTERS";
        case AssetCategory::Weapons: return "WEAPONS";
        case AssetCategory::Projectiles: return "PROJECTILES";
        case AssetCategory::Enemies: return "ENEMIES";
        case AssetCategory::Obstacles: return "OBSTACLES";
        case AssetCategory::Doors: return "DOORS";
        case AssetCategory::Textures: return "TEXTURES";
        case AssetCategory::Sprites: return "SPRITES";
        case AssetCategory::Animations: return "ANIMATIONS";
        case AssetCategory::Effects: return "EFFECTS";
        case AssetCategory::UI: return "UI";
        case AssetCategory::Items: return "ITEMS/PICKUPS";
        case AssetCategory::Count: break;
    }
    return "UNKNOWN";
}

std::string Upper(std::string value) {
    for (char& c : value) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - ('a' - 'A'));
    }
    return value;
}

std::size_t ProjectileCount(const engine::assets::SceneAsset& scene) {
    std::vector<std::string> ids;
    for (const auto& weapon : scene.weapons) {
        if (weapon.projectile.id.empty()) continue;
        if (std::find(ids.begin(), ids.end(), weapon.projectile.id) == ids.end()) {
            ids.push_back(weapon.projectile.id);
        }
    }
    return ids.size();
}

const engine::assets::ProjectileDefinition* ProjectileAt(
    const engine::assets::SceneAsset& scene, std::size_t index) {
    std::vector<const engine::assets::ProjectileDefinition*> seen;
    for (const auto& weapon : scene.weapons) {
        const auto* projectile = &weapon.projectile;
        if (std::find(seen.begin(), seen.end(), projectile) != seen.end()) continue;
        bool duplicate = false;
        for (const auto* prior : seen) {
            if (prior->id == projectile->id) { duplicate = true; break; }
        }
        if (duplicate) continue;
        if (seen.size() == index) return projectile;
        seen.push_back(projectile);
    }
    return nullptr;
}

void DrawPreviewSprite(engine::rendering::SpriteBatch& batch,
                       const engine::assets::SceneAsset& scene,
                       int32_t sprite_index, const Vec2& center, float max_size) {
    if (sprite_index < 0 ||
        static_cast<std::size_t>(sprite_index) >= scene.sprites.size()) {
        return;
    }
    const auto& sprite = scene.sprites[static_cast<std::size_t>(sprite_index)];
    const Vec2 world = sprite.WorldSize();
    const float scale = std::min(max_size / std::max(world.x, 0.01f),
                                 max_size / std::max(world.y, 0.01f));
    engine::rendering::SpriteDraw draw;
    draw.position = center;
    draw.size = world * scale;
    draw.uv = sprite.UvRect();
    draw.color = Color::White();
    draw.layer = SortLayer::UI;
    draw.sorting_order = 105;
    draw.texture_id = sprite.texture_id;
    draw.pivot = sprite.pivot;
    (void)batch.DrawSprite(draw);
}

void DrawTexture(engine::rendering::SpriteBatch& batch,
                 const engine::assets::TextureAsset& texture,
                 const Vec2& center, float max_size) {
    const float aspect = texture.data.height > 0
        ? static_cast<float>(texture.data.width) /
              static_cast<float>(texture.data.height)
        : 1.0f;
    const Vec2 size = aspect >= 1.0f ? Vec2{max_size, max_size / aspect}
                                     : Vec2{max_size * aspect, max_size};
    engine::rendering::SpriteDraw draw;
    draw.position = center;
    draw.size = size;
    draw.uv = {0.0f, 0.0f, 1.0f, 1.0f};
    draw.color = Color::White();
    draw.layer = SortLayer::UI;
    draw.sorting_order = 105;
    draw.texture_id = texture.id;
    draw.pivot = {0.5f, 0.5f};
    (void)batch.DrawSprite(draw);
}

} // namespace

void AssetLab::Open() {
    open_ = true;
    paused_ = false;
    time_ = 0.0;
}

void AssetLab::Close() { open_ = false; }

void AssetLab::Toggle() {
    if (open_) Close(); else Open();
}

void AssetLab::NextCategory(int delta) {
    int next = (static_cast<int>(category_) + delta) % kCategoryCount;
    if (next < 0) next += kCategoryCount;
    category_ = static_cast<AssetCategory>(next);
    item_ = 0;
    time_ = 0.0;
}

void AssetLab::NextItem(int delta) {
    // Count is resolved by the caller's scene. A large sentinel keeps this
    // method independent; Draw/Application clamp to the actual category size.
    const int next = static_cast<int>(item_) + delta;
    item_ = static_cast<std::size_t>(std::max(0, next));
    time_ = 0.0;
}

void AssetLab::TogglePause() { paused_ = !paused_; }

void AssetLab::StepFrame(int direction) {
    paused_ = true;
    const engine::animation::AnimationClip* clip = nullptr;
    // The current clip requires a scene; the caller normally uses StepFrame
    // after loading. A one-frame step is applied by Update's sample interval.
    (void)clip;
    const float step = direction >= 0 ? 1.0f / 8.0f : -1.0f / 8.0f;
    time_ = std::max(0.0, time_ + step);
}

void AssetLab::Update(float delta_seconds) {
    if (!open_ || paused_ || delta_seconds <= 0.0f) return;
    time_ += delta_seconds;
}

const char* AssetLab::CategoryName() const { return MenuName(category_); }

std::size_t AssetLab::ItemCount(const engine::assets::SceneAsset& scene) const {
    switch (category_) {
        case AssetCategory::Characters: return scene.characters.size();
        case AssetCategory::Weapons: return scene.weapons.size();
        case AssetCategory::Projectiles: return ProjectileCount(scene);
        case AssetCategory::Enemies: return scene.enemy_definitions.size();
        case AssetCategory::Obstacles: return scene.obstacle_variants.size();
        case AssetCategory::Doors: return scene.doors.size();
        case AssetCategory::Textures: return scene.textures.size();
        case AssetCategory::Sprites: return scene.sprites.size();
        case AssetCategory::Animations: return scene.animations.size();
        case AssetCategory::Effects:
        case AssetCategory::UI:
        case AssetCategory::Items:
        case AssetCategory::Count: return 0;
    }
    return 0;
}

std::string AssetLab::ItemId(const engine::assets::SceneAsset& scene) const {
    const std::size_t count = ItemCount(scene);
    if (count == 0) return "UNRECOVERED";
    const std::size_t index = std::min(item_, count - 1);
    switch (category_) {
        case AssetCategory::Characters: return scene.characters[index].id;
        case AssetCategory::Weapons: return scene.weapons[index].id;
        case AssetCategory::Projectiles: {
            const auto* projectile = ProjectileAt(scene, index);
            return projectile != nullptr ? projectile->id : "UNRECOVERED";
        }
        case AssetCategory::Enemies: return scene.enemy_definitions[index].id;
        case AssetCategory::Obstacles: return scene.obstacle_variants[index].name;
        case AssetCategory::Doors: return scene.doors[index].id;
        case AssetCategory::Textures: return scene.textures[index].name;
        case AssetCategory::Sprites: return scene.sprites[index].id;
        case AssetCategory::Animations:
            return scene.animations[index].id;
        case AssetCategory::Effects:
        case AssetCategory::UI:
        case AssetCategory::Items:
        case AssetCategory::Count: return "UNRECOVERED";
    }
    return "UNRECOVERED";
}

const engine::animation::AnimationClip* AssetLab::CurrentClip(
    const engine::assets::SceneAsset& scene) const {
    if (category_ == AssetCategory::Animations && !scene.animations.empty()) {
        const std::size_t index = std::min(item_, scene.animations.size() - 1);
        return &scene.animations[index];
    }
    if (category_ == AssetCategory::Characters && !scene.characters.empty()) {
        const std::size_t index = std::min(item_, scene.characters.size() - 1);
        const int32_t animation_index =
            scene.FindAnimationIndex(scene.characters[index].idle_animation_id);
        if (animation_index >= 0) {
            return &scene.animations[static_cast<std::size_t>(animation_index)];
        }
    }
    return nullptr;
}

int32_t AssetLab::CurrentSpriteIndex(const engine::assets::SceneAsset& scene) const {
    const std::size_t count = ItemCount(scene);
    if (count == 0) return -1;
    const std::size_t index = std::min(item_, count - 1);
    switch (category_) {
        case AssetCategory::Characters: {
            const auto* clip = CurrentClip(scene);
            if (clip != nullptr && !clip->tracks.empty()) {
                const auto* key = clip->tracks.front().Evaluate(
                    static_cast<float>(time_));
                if (key != nullptr) {
                    const int32_t animated = scene.FindSpriteIndex(key->sprite_id);
                    if (animated >= 0) return animated;
                }
            }
            return scene.characters[index].body_sprite_index;
        }
        case AssetCategory::Weapons: return scene.weapons[index].visual_sprite_index;
        case AssetCategory::Projectiles: {
            const auto* projectile = ProjectileAt(scene, index);
            return projectile != nullptr ? projectile->sprite_index : -1;
        }
        case AssetCategory::Enemies: return scene.enemy_definitions[index].body_sprite_index;
        case AssetCategory::Obstacles:
            return scene.obstacle_variants[index].visuals.empty()
                ? -1 : scene.obstacle_variants[index].visuals.front().sprite_index;
        case AssetCategory::Doors: {
            if (scene.doors[index].leaves.empty()) return -1;
            const int32_t instance =
                scene.doors[index].leaves.front().leaf_instance;
            if (instance < 0 ||
                static_cast<std::size_t>(instance) >= scene.instances.size()) {
                return -1;
            }
            return static_cast<int32_t>(
                scene.instances[static_cast<std::size_t>(instance)].sprite_index);
        }
        case AssetCategory::Sprites: return static_cast<int32_t>(index);
        case AssetCategory::Animations: {
            const auto* clip = CurrentClip(scene);
            if (clip == nullptr || clip->tracks.empty()) return -1;
            const auto* key = clip->tracks.front().Evaluate(static_cast<float>(time_));
            if (key == nullptr) return -1;
            return scene.FindSpriteIndex(key->sprite_id);
        }
        default: return -1;
    }
}

float AssetLab::CurrentPPU(const engine::assets::SceneAsset& scene) const {
    const int32_t sprite_index = CurrentSpriteIndex(scene);
    if (sprite_index < 0 ||
        static_cast<std::size_t>(sprite_index) >= scene.sprites.size()) {
        return 0.0f;
    }
    return scene.sprites[static_cast<std::size_t>(sprite_index)].pixels_per_unit;
}

Vec2 AssetLab::CurrentPivot(const engine::assets::SceneAsset& scene) const {
    const int32_t sprite_index = CurrentSpriteIndex(scene);
    if (sprite_index < 0 ||
        static_cast<std::size_t>(sprite_index) >= scene.sprites.size()) {
        return {0.0f, 0.0f};
    }
    return scene.sprites[static_cast<std::size_t>(sprite_index)].pivot;
}

void AssetLab::Draw(engine::rendering::SpriteBatch& batch,
                    const engine::assets::SceneAsset& scene,
                    const engine::Camera2D& camera,
                    const DevOverlay& overlay) const {
    if (!open_) return;
    const auto& text = overlay.Text();
    const float scale = std::max(0.018f, (2.0f * camera.HalfHeight()) / 1080.0f * 1.9f);
    const Vec2 center = camera.position;
    text.DrawPanel(batch, {center.x - 11.4f, center.y + 6.4f},
                   {22.8f, 12.8f}, Color{8, 12, 20, 238}, 90);
    text.DrawText(batch, "LITEKNIGHT ASSET LABORATORY",
                  {center.x - 10.9f, center.y + 6.0f}, scale,
                  Color{255, 224, 120, 255}, 106);
    text.DrawText(batch, std::string("[ / ] CATEGORY: ") + CategoryName(),
                  {center.x - 10.9f, center.y + 5.55f}, scale * 0.85f,
                  Color{190, 210, 240, 255}, 106);
    text.DrawText(batch, "COMMA/DOT: ITEM   SPACE: PLAY/PAUSE   F: STEP   R: RESTART",
                  {center.x - 10.9f, center.y + 5.2f}, scale * 0.65f,
                  Color{150, 175, 205, 255}, 106);
    text.DrawText(batch, "ENTER: USE   ESC: CLOSE",
                  {center.x - 10.9f, center.y + 4.9f}, scale * 0.65f,
                  Color{150, 175, 205, 255}, 106);

    const std::size_t count = ItemCount(scene);
    if (count == 0) {
        text.DrawText(batch, "STATUS: UNRECOVERED / NOT INTEGRATED",
                      {center.x - 8.0f, center.y}, scale,
                      Color{255, 110, 110, 255}, 106);
        return;
    }
    const std::size_t index = std::min(item_, count - 1);
    char line[256];
    std::snprintf(line, sizeof(line), "ITEM %zu/%zu  %s", index + 1, count,
                  Upper(ItemId(scene)).c_str());
    text.DrawText(batch, line, {center.x - 10.9f, center.y + 4.4f},
                  scale, Color{235, 240, 248, 255}, 106);

    float y = center.y + 3.85f;
    const auto row = [&](const std::string& value) {
        text.DrawText(batch, value, {center.x - 10.9f, y}, scale * 0.76f,
                      Color{190, 210, 235, 255}, 106);
        y -= text.LineHeight(scale * 0.76f);
    };
    row("SOURCE: LOCAL REFERENCE / LOCAL_ONLY");
    row("STATUS: INTEGRATED / DEVROOM TESTED");
    const float ppu = CurrentPPU(scene);
    const Vec2 pivot = CurrentPivot(scene);
    std::snprintf(line, sizeof(line), "PPU: %.2f  PIVOT: %.3f, %.3f",
                  ppu, pivot.x, pivot.y);
    row(line);
    if (const auto* clip = CurrentClip(scene); clip != nullptr) {
        std::snprintf(line, sizeof(line), "ANIMATION: %s",
                      Upper(clip->id).c_str());
        row(line);
        std::snprintf(line, sizeof(line),
                      "FRAME: %zu/%zu  TIME: %.3f/%.3f S",
                      clip->tracks.empty() ? 0U :
                        clip->tracks.front().keys.size(),
                      clip->tracks.empty() ? 0U :
                        clip->tracks.front().keys.size(),
                      static_cast<float>(time_), clip->duration);
        row(line);
        std::snprintf(line, sizeof(line),
                      "SAMPLE: %.1f HZ  LOOP: %s  MODE: ACTIVE",
                      clip->sample_rate, clip->loop ? "YES" : "NO");
        row(line);
    }
    if (category_ == AssetCategory::Weapons) {
        const auto& weapon = scene.weapons[index];
        std::snprintf(line, sizeof(line), "DAMAGE: %d  SPEED: %.1f  CYCLE: %.4f",
                      weapon.damage, weapon.bullet_speed,
                      weapon.attack_cycle_seconds);
        row(line);
        std::snprintf(line, sizeof(line), "PROJECTILE: %s  CONSUME: %d",
                      Upper(weapon.projectile.id).c_str(), weapon.consume);
        row(line);
    } else if (category_ == AssetCategory::Enemies) {
        const auto& enemy = scene.enemy_definitions[index];
        std::snprintf(line, sizeof(line), "HP: %d  CAMP: %d  CONTACT: %d / %.2f S",
                      enemy.hp, enemy.camp, enemy.contact_damage,
                      enemy.contact_cooldown);
        row(line);
    } else if (category_ == AssetCategory::Obstacles) {
        const auto& obstacle = scene.obstacle_variants[index];
        std::snprintf(line, sizeof(line), "VISUALS: %zu  SOLID: %s",
                      obstacle.visuals.size(), obstacle.has_collider ? "YES" : "NO");
        row(line);
    } else if (category_ == AssetCategory::Textures) {
        const auto& texture = scene.textures[index];
        std::snprintf(line, sizeof(line), "DIMENSIONS: %ux%u  RUNTIME: RGBA8",
                      texture.data.width, texture.data.height);
        row(line);
    }

    const Vec2 preview_center{center.x + 5.9f, center.y + 1.5f};
    if (category_ == AssetCategory::Textures) {
        DrawTexture(batch, scene.textures[index], preview_center, 3.8f);
    } else {
        const int32_t sprite_index = CurrentSpriteIndex(scene);
        if (sprite_index >= 0) {
            DrawPreviewSprite(batch, scene, sprite_index, preview_center, 3.8f);
        } else {
            text.DrawText(batch, "NO SPRITE", {preview_center.x - 1.5f,
                                                preview_center.y}, scale,
                          Color{255, 130, 130, 255}, 106);
        }
    }
    text.DrawText(batch, paused_ ? "PLAYBACK: PAUSED" : "PLAYBACK: PLAYING",
                  {center.x + 1.0f, center.y - 4.3f}, scale * 0.8f,
                  paused_ ? Color{255, 220, 120, 255} : Color{140, 235, 160, 255},
                  106);
}

} // namespace soulknight::game::ui
