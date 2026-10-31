// Soul Knight Linux — native pixel UI screens (HUD, pause, settings, main menu)
#pragma once

#include "UIAssets.hpp"
#include "UIRenderer.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "game/combat/Damage.hpp"
#include "game/player/Energy.hpp"
#include "game/player/PlayerCharacterSet.hpp"
#include "game/weapons/WeaponInventory.hpp"
#include "game/weapons/WeaponRegistry.hpp"

#include <cstddef>
#include <string>

namespace soulknight::game::ui {

enum class UIScreen {
    Gameplay,
    Pause,
    Settings,
    MainMenu,
    CharacterSelect,
    WeaponSelect
};

enum class UIAction {
    None,
    Resume,
    Home,
    NewGame,
    ContinueGame,
    Settings,
    Back,
    Quit,
    ToggleFullscreen,
    ToggleDebugOverlay,
    ResetSave,
    SelectCharacter,
    SelectWeapon,
    ToggleAimMode
};

struct GameUIData {
    const engine::assets::SceneAsset* scene = nullptr;
    const player::PlayerCharacterSet* characters = nullptr;
    const weapons::WeaponRegistry* weapons = nullptr;
    const weapons::WeaponInventory* inventory = nullptr;
    const combat::HealthState* health = nullptr;
    const player::EnergyState* energy = nullptr;
    bool paused = false;
    bool dead = false;
    bool save_exists = false;
    bool fullscreen = false;
    bool debug_overlay = false;
    bool has_pixel_font = false;
    bool auto_aim = false;
    int ability_state = 0; // 0 empty, 1 ready, 2 active, 3 cooldown
    float ability_cooldown = 0.0f;
    const UIAssets* ui_assets = nullptr;
    bool pickup_nearby = false;
    std::string pickup_name;
    int pickup_damage = 0;
    int pickup_energy_cost = 0;
    float pickup_cycle_seconds = 0.0f;
    engine::Vec2 pickup_screen{0.0f, 0.0f};
    bool interaction_nearby = false;
    std::string interaction_label;
    engine::Vec2 interaction_screen{0.0f, 0.0f};
};

class GameUI {
public:
    void OpenPause();
    void Resume();
    void OpenSettings();
    void OpenCharacterSelect(int initial_index = 0);
    void OpenWeaponSelect(int initial_index = 0);
    void CloseSelection();
    int CharacterSelection() const { return character_select_index_; }
    int WeaponSelection() const { return weapon_select_index_; }
    void Back();
    void OpenMainMenu();
    void CloseMainMenu();
    void OpenGameplay();

    bool IsPaused() const {
        return screen_ == UIScreen::Pause || screen_ == UIScreen::Settings ||
               screen_ == UIScreen::CharacterSelect ||
               screen_ == UIScreen::WeaponSelect;
    }
    bool IsMainMenu() const { return screen_ == UIScreen::MainMenu; }
    UIScreen Screen() const { return screen_; }

    void Navigate(int delta);
    void Confirm();
    UIAction TakeAction();

    void Draw(UIRenderer& ui, const GameUIData& data) const;

private:
    void DrawHud(UIRenderer& ui, const GameUIData& data) const;
    void DrawPause(UIRenderer& ui, const GameUIData& data) const;
    void DrawSettings(UIRenderer& ui, const GameUIData& data) const;
    void DrawMainMenu(UIRenderer& ui, const GameUIData& data) const;
    void DrawCharacterSelect(UIRenderer& ui, const GameUIData& data) const;
    void DrawWeaponSelect(UIRenderer& ui, const GameUIData& data) const;
    void DrawButton(UIRenderer& ui, const GameUIData& data,
                    const engine::Rect& rect, const std::string& label,
                    bool selected, bool enabled, float size = 28.0f) const;
    void DrawPowerSlots(UIRenderer& ui, const GameUIData& data,
                        const engine::Rect& start, int count,
                        int occupied) const;
    void DrawIcon(UIRenderer& ui, const GameUIData& data, int kind,
                  const engine::Rect& rect, int32_t order) const;
    void DrawAsset(UIRenderer& ui, const GameUIData& data,
                   const std::string& name, const engine::Rect& rect,
                   engine::Color color, int32_t order,
                   bool nine_slice = false) const;

    UIScreen screen_ = UIScreen::Gameplay;
    UIScreen settings_parent_ = UIScreen::Pause;
    int pause_index_ = 1;
    int main_index_ = 0;
    int settings_index_ = 0;
    int character_select_index_ = 0;
    int weapon_select_index_ = 0;
    UIAction action_ = UIAction::None;
};

} // namespace soulknight::game::ui
