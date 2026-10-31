// Soul Knight Linux — native developer overlay and selectors
#pragma once

#include "engine/assets/TextureData.hpp"
#include "engine/rendering/DebugText.hpp"
#include "platform/filesystem/FileSystem.hpp"
#include "game/combat/Damage.hpp"
#include "game/player/Energy.hpp"
#include "game/weapons/WeaponInventory.hpp"
#include "game/enemy/EnemySystem.hpp"
#include "game/player/PlayerCharacterSet.hpp"
#include "game/weapons/WeaponRegistry.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace soulknight::engine {
struct Camera2D;
struct Vec2;
}
namespace soulknight::engine::locomotion {
class PlayerLocomotion;
}
namespace soulknight::engine::rendering {
class SpriteBatch;
}
namespace soulknight::engine::assets {
struct SceneAsset;
}

namespace soulknight::game::ui {

enum class DevMenu {
    None,
    Characters,
    Weapons,
    Enemies,
};

struct DevOverlayContext {
    const engine::assets::SceneAsset* scene = nullptr;
    const engine::Camera2D* camera = nullptr;
    const player::PlayerCharacterSet* characters = nullptr;
    const weapons::WeaponRegistry* weapons = nullptr;
    const enemy::EnemySystem* enemies = nullptr;
    const engine::locomotion::PlayerLocomotion* player = nullptr;
    const combat::HealthState* health = nullptr;
    const player::EnergyState* energy = nullptr;
    const weapons::WeaponInventory* weapon_inventory = nullptr;
    bool hud_visible = true;
    const char* room_id = "r1_1_visual";
    const char* facing = "Right";
    const char* version = "0.1.0-dev";
    const char* build = "unknown";
    const char* platform = "Wayland";
    const char* renderer = "Vulkan";
    float frame_ms = 0.0f;
    std::uint64_t simulation_ticks = 0;
    std::uint64_t render_frames = 0;
    std::size_t active_projectiles = 0;
    std::size_t pickups = 0;
    bool room_clear_pending = false;
    const char* room_graph = "hero_room <-> r1_1_visual";
    int logical_width = 1920;
    int logical_height = 1080;
    int framebuffer_width = 1920;
    int framebuffer_height = 1080;
    float framebuffer_scale = 1.0f;
};

// The developer UI is deliberately a native overlay, not the production menu.
// It uses recovered character/weapon/enemy sprites and a generated placeholder
// 5x7 text atlas until a real recovered UI font is integrated.
class DevOverlay {
public:
    bool BuildFont();
    bool LoadPixelFont(const platform::FileSystem& filesystem,
                       const std::string& json_path, std::string* error);
    bool HasPixelFont() const { return text_.HasPixelFont(); }
    const engine::assets::TextureData& FontTexture() const {
        return text_.Atlas();
    }
    const engine::rendering::DebugTextRenderer& Text() const { return text_; }
    engine::rendering::DebugTextRenderer& Text() { return text_; }

    bool OverlayVisible() const { return overlay_visible_; }
    void ToggleOverlay() { overlay_visible_ = !overlay_visible_; }

    bool MenuOpen() const { return menu_ != DevMenu::None; }
    DevMenu CurrentMenu() const { return menu_; }
    bool CapturesInput() const { return MenuOpen(); }

    void OpenMenu(DevMenu menu, std::size_t initial_index,
                  std::size_t item_count);
    void CloseMenu();
    void Navigate(int delta);
    std::size_t SelectedIndex() const { return selected_index_; }

    void Draw(engine::rendering::SpriteBatch& batch,
              const DevOverlayContext& context) const;

private:
    void DrawOverlay(engine::rendering::SpriteBatch& batch,
                     const DevOverlayContext& context,
                     float left, float top, float scale) const;
    void DrawCharacterMenu(engine::rendering::SpriteBatch& batch,
                           const DevOverlayContext& context,
                           float scale) const;
    void DrawWeaponMenu(engine::rendering::SpriteBatch& batch,
                        const DevOverlayContext& context,
                        float scale) const;
    void DrawEnemyMenu(engine::rendering::SpriteBatch& batch,
                       const DevOverlayContext& context,
                       float scale) const;
    void DrawHud(engine::rendering::SpriteBatch& batch,
                 const DevOverlayContext& context,
                 float scale) const;

    engine::rendering::DebugTextRenderer text_;
    bool overlay_visible_ = false;
    DevMenu menu_ = DevMenu::None;
    std::size_t selected_index_ = 0;
    std::size_t item_count_ = 0;
};

} // namespace soulknight::game::ui
