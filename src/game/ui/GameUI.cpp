#include "GameUI.hpp"

#include "game/player/Ability.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>

namespace soulknight::game::ui {
namespace {

using engine::Color;
using engine::Rect;

constexpr Color kPanelDark{20, 24, 34, 245};
constexpr Color kPanelNavy{27, 33, 49, 250};
constexpr Color kFrameBrown{119, 79, 45, 255};
constexpr Color kFrameGold{201, 158, 83, 255};
constexpr Color kFrameLight{239, 205, 132, 255};
constexpr Color kInk{18, 16, 18, 255};
constexpr Color kCream{245, 225, 185, 255};
constexpr Color kMuted{174, 168, 151, 255};
constexpr Color kRed{205, 40, 47, 255};
constexpr Color kHealth{225, 57, 62, 255};
constexpr Color kArmor{171, 184, 194, 255};
constexpr Color kEnergy{45, 153, 238, 255};
constexpr Color kGold{227, 186, 83, 255};
constexpr Color kGreen{93, 190, 107, 255};

float ClearCubic(float tau, float a, float b, float c) {
    return a * tau * tau * tau + b * tau * tau + c * tau;
}

float ClearBannerY(float elapsed) {
    if (elapsed <= 0.5f) return 0.0f;
    if (elapsed <= 0.833333f) {
        const float tau = elapsed - 0.5f;
        return -90.0f * 0.0f + 1.5f * ClearCubic(tau, 810.0f, -540.0f, -90.0f);
    }
    if (elapsed <= 1.166667f) {
        const float tau = elapsed - 0.833333f;
        return -90.0f + 1.5f * ClearCubic(tau, -810.0f, 540.0f, -90.0f);
    }
    return -90.0f;
}

float ClearAlpha(float elapsed, float peak) {
    if (elapsed <= 0.0f) return 0.003f * peak;
    if (elapsed < 0.166667f) {
        const float t = elapsed / 0.166667f;
        return (0.003f + (peak - 0.003f) * t);
    }
    if (elapsed <= 0.833333f) return peak;
    if (elapsed <= 1.166667f) {
        const float tau = elapsed - 0.833333f;
        return std::clamp(peak + ClearCubic(tau, -10.588235f, 3.529412f, 0.0f),
                          0.0f, peak);
    }
    if (elapsed < 1.5f) {
        const float tau = elapsed - 1.166667f;
        return std::clamp(peak + ClearCubic(tau, 10.588227f, -7.058823f,
                                            -1.176471f), 0.0f, peak);
    }
    return 0.0f;
}

float ClearTextAlpha(float elapsed) {
    if (elapsed <= 0.0f) return 0.0f;
    if (elapsed < 0.166667f) return elapsed / 0.166667f;
    if (elapsed <= 1.166667f) return 1.0f;
    if (elapsed < 1.5f) {
        const float tau = elapsed - 1.166667f;
        return std::clamp(1.0f + ClearCubic(tau, 13.499995f, -8.999998f,
                                            -1.5f), 0.0f, 1.0f);
    }
    return 0.0f;
}

const std::array<const char*, 3> kPauseItems{"HOME", "RESUME", "SETTINGS"};
const std::array<const char*, 4> kMainItems{"NEW GAME", "CONTINUE", "SETTINGS", "QUIT"};
const std::array<const char*, 6> kSettingsItems{
    "AIM MODE", "FULLSCREEN", "DEBUG OVERLAY", "UI DESIGN MODE",
    "RESET SAVE", "BACK"};

// One authoritative main-menu layout. Drawing and pointer hit-testing both use
// these helpers so a visual rect can never drift away from its click target.
Rect MainMenuDroomRect() { return {1636.0f, 64.0f, 220.0f, 58.0f}; }
Rect MainMenuButtonRect(int index) {
    return {120.0f, 410.0f + static_cast<float>(index) * 162.0f,
            560.0f, 148.0f};
}
Rect MainMenuSaveLoadRect() { return {704.0f, 410.0f, 150.0f, 148.0f}; }

Rect MenuRect(const HudLayout* layout, const char* id,
              const Rect& fallback) {
    if (layout == nullptr) return fallback;
    const Rect rect = layout->RectFor(id);
    return rect.w > 0.0f && rect.h > 0.0f ? rect : fallback;
}

void DrawBar(UIRenderer& ui, const Rect& rect, float fraction, Color fill,
             int32_t order) {
    ui.Rect(rect, {18, 17, 22, 255}, order);
    ui.Rect({rect.x + 3, rect.y + 3, rect.w - 6, rect.h - 6},
            {56, 50, 54, 255}, order + 1);
    const float clamped = std::clamp(fraction, 0.0f, 1.0f);
    if (clamped > 0.0f) {
        ui.Rect({rect.x + 3, rect.y + 3,
                 (rect.w - 6) * clamped, rect.h - 6},
                fill, order + 2);
        ui.Rect({rect.x + 3, rect.y + 4,
                 (rect.w - 6) * clamped, 3},
                {255, 255, 255, 48}, order + 3);
    }
}

} // namespace

void GameUI::OpenPause() {
    if (screen_ == UIScreen::MainMenu) return;
    screen_ = UIScreen::Pause;
    pause_index_ = 1;
    action_ = UIAction::None;
}

void GameUI::Resume() {
    screen_ = UIScreen::Gameplay;
    action_ = UIAction::None;
}

void GameUI::OpenSettings() {
    settings_parent_ = screen_;
    screen_ = UIScreen::Settings;
    settings_index_ = 0;
    action_ = UIAction::None;
}

void GameUI::OpenCharacterSelect(int initial_index) {
    screen_ = UIScreen::CharacterSelect;
    character_select_index_ = std::max(0, initial_index);
    action_ = UIAction::None;
}

void GameUI::OpenWeaponSelect(int initial_index) {
    screen_ = UIScreen::WeaponSelect;
    weapon_select_index_ = std::max(0, initial_index);
    action_ = UIAction::None;
}

void GameUI::OpenSaveLoad() {
    screen_ = UIScreen::SaveLoad;
    action_ = UIAction::None;
}

void GameUI::OpenPetSelect(int initial_index, int count) {
    screen_ = UIScreen::PetSelect;
    pet_select_count_ = std::max(0, count);
    pet_select_index_ = pet_select_count_ > 0
        ? std::clamp(initial_index, 0, pet_select_count_ - 1)
        : std::max(0, initial_index);
    action_ = UIAction::None;
}

void GameUI::OpenWorkshop(int initial_index, int count) {
    screen_ = UIScreen::Workshop;
    workshop_count_ = std::max(0, count);
    workshop_index_ = workshop_count_ > 0
        ? std::clamp(initial_index, 0, workshop_count_ - 1)
        : std::max(0, initial_index);
    action_ = UIAction::None;
}

void GameUI::OpenTrader() {
    screen_ = UIScreen::Trader;
    trader_selected_ = 0;
    action_ = UIAction::None;
}

void GameUI::CloseSelection() {
    screen_ = UIScreen::Gameplay;
    action_ = UIAction::None;
}

void GameUI::Back() {
    if (screen_ == UIScreen::CharacterSelect ||
        screen_ == UIScreen::WeaponSelect ||
        screen_ == UIScreen::PetSelect ||
        screen_ == UIScreen::Workshop) {
        CloseSelection();
        return;
    }
    if (screen_ == UIScreen::Settings) {
        screen_ = settings_parent_;
        if (screen_ == UIScreen::Pause) pause_index_ = 2;
    } else if (screen_ == UIScreen::Pause) {
        Resume();
    } else if (screen_ == UIScreen::SaveLoad) {
        OpenMainMenu();
    } else if (screen_ == UIScreen::Trader) {
        OpenGameplay();
    }
    action_ = UIAction::None;
}

void GameUI::OpenMainMenu() {
    screen_ = UIScreen::MainMenu;
    main_index_ = 0;
    action_ = UIAction::None;
}

void GameUI::CloseMainMenu() {
    screen_ = UIScreen::Gameplay;
    action_ = UIAction::None;
}

void GameUI::OpenGameplay() {
    screen_ = UIScreen::Gameplay;
    action_ = UIAction::None;
}

void GameUI::Navigate(int delta) {
    if (delta == 0) return;
    if (screen_ == UIScreen::Pause) {
        pause_index_ = (pause_index_ + delta + 3) % 3;
    } else if (screen_ == UIScreen::MainMenu) {
        main_index_ = (main_index_ + delta + 6) % 6;
    } else if (screen_ == UIScreen::SaveLoad) {
        save_load_index_ = (save_load_index_ + delta + 4) % 4;
    } else if (screen_ == UIScreen::Trader) {
        trader_selected_ = (trader_selected_ + delta + 3) % 3;
    } else if (screen_ == UIScreen::PetSelect && pet_select_count_ > 0) {
        pet_select_index_ = (pet_select_index_ + delta + pet_select_count_) %
                            pet_select_count_;
    } else if (screen_ == UIScreen::Workshop && workshop_count_ > 0) {
        workshop_index_ = (workshop_index_ + delta + workshop_count_) %
                          workshop_count_;
    } else if (screen_ == UIScreen::Settings) {
        settings_index_ = (settings_index_ + delta + 6) % 6;
    } else if (screen_ == UIScreen::CharacterSelect &&
               character_select_count_ > 0) {
        character_select_index_ = (character_select_index_ + delta +
                                   character_select_count_) %
                                  character_select_count_;
    } else if (screen_ == UIScreen::WeaponSelect &&
               weapon_select_count_ > 0) {
        weapon_select_index_ = (weapon_select_index_ + delta +
                                weapon_select_count_) %
                               weapon_select_count_;
    }
}

void GameUI::Confirm() {
    if (screen_ == UIScreen::Pause) {
        if (pause_index_ == 0) action_ = UIAction::Home;
        else if (pause_index_ == 1) action_ = UIAction::Resume;
        else action_ = UIAction::Settings;
        return;
    }
    if (screen_ == UIScreen::MainMenu) {
        if (main_index_ == 0) action_ = UIAction::NewGame;
        else if (main_index_ == 1) action_ = UIAction::ContinueGame;
        else if (main_index_ == 2) action_ = UIAction::Settings;
        else if (main_index_ == 3) action_ = UIAction::Quit;
        else if (main_index_ == 4) action_ = UIAction::OpenDevroom;
        else action_ = UIAction::OpenSaveLoad;
        return;
    }
    if (screen_ == UIScreen::SaveLoad) {
        if (save_load_index_ == 0) action_ = UIAction::SaveSlot;
        else if (save_load_index_ == 1) action_ = UIAction::LoadSlot;
        else if (save_load_index_ == 2) action_ = UIAction::DeleteSlot;
        else action_ = UIAction::Back;
        return;
    }
    if (screen_ == UIScreen::Trader) {
        action_ = UIAction::BuyTraderItem;
        return;
    }
    if (screen_ == UIScreen::CharacterSelect) {
        action_ = UIAction::SelectCharacter;
        return;
    }
    if (screen_ == UIScreen::WeaponSelect) {
        action_ = UIAction::SelectWeapon;
        return;
    }
    if (screen_ == UIScreen::PetSelect) {
        action_ = UIAction::SelectPet;
        return;
    }
    if (screen_ == UIScreen::Workshop) {
        action_ = UIAction::CraftWeapon;
        return;
    }
    if (screen_ == UIScreen::Settings) {
        if (settings_index_ == 0) action_ = UIAction::ToggleAimMode;
        else if (settings_index_ == 1) action_ = UIAction::ToggleFullscreen;
        else if (settings_index_ == 2) action_ = UIAction::ToggleDebugOverlay;
        else if (settings_index_ == 3) action_ = UIAction::OpenUiDesignMode;
        else if (settings_index_ == 4) action_ = UIAction::ResetSave;
        else action_ = UIAction::Back;
    }
}

UIAction GameUI::TakeAction() {
    const UIAction result = action_;
    action_ = UIAction::None;
    return result;
}

bool GameUI::HandlePointerClick(const engine::Vec2& logical,
                                const HudLayout* layout) {
    if (screen_ != UIScreen::MainMenu) return false;
    const Rect droom = MenuRect(layout, "menu_droom", MainMenuDroomRect());
    if (logical.x >= droom.x && logical.x <= droom.x + droom.w &&
        logical.y >= droom.y && logical.y <= droom.y + droom.h) {
        main_index_ = 4;
        Confirm();
        return true;
    }
    const Rect save_load =
        MenuRect(layout, "menu_save_load", MainMenuSaveLoadRect());
    if (logical.x >= save_load.x && logical.x <= save_load.x + save_load.w &&
        logical.y >= save_load.y && logical.y <= save_load.y + save_load.h) {
        main_index_ = 5;
        Confirm();
        return true;
    }
    static const char* kIds[4] = {"menu_new_game", "menu_continue",
                                  "menu_settings", "menu_quit"};
    for (int i = 0; i < 4; ++i) {
        const Rect r = MenuRect(layout, kIds[i], MainMenuButtonRect(i));
        if (logical.x >= r.x && logical.x <= r.x + r.w &&
            logical.y >= r.y && logical.y <= r.y + r.h) {
            main_index_ = i;
            Confirm();
            return true;
        }
    }
    return false;
}

void GameUI::DrawAsset(UIRenderer& ui, const GameUIData& data,
                       const std::string& name, const Rect& rect,
                       Color color, int32_t order, bool nine_slice) const {
    if (data.ui_assets == nullptr) return;
    const UIAssetSprite* sprite = data.ui_assets->Find(name);
    if (sprite == nullptr) return;
    if (nine_slice) {
        ui.NineSlice(*sprite, rect, color, order);
    } else {
        ui.Image(sprite->texture_id, sprite->uv, rect, color, order);
    }
}

void GameUI::DrawIcon(UIRenderer& ui, const GameUIData& data, int kind,
                      const Rect& rect, int32_t order) const {
    static const char* kAssetNames[3] = {"ui_33", "ui_34", "ui_67"};
    if (kind >= 0 && kind < 3) {
        const UIAssetSprite* sprite = data.ui_assets != nullptr
            ? data.ui_assets->Find(kAssetNames[kind]) : nullptr;
        if (sprite != nullptr) {
            ui.Image(sprite->texture_id, sprite->uv, rect, Color::White(),
                     order);
            return;
        }
    }
    const float u = rect.w / 16.0f;
    const auto px = [&](float x, float y, float w, float h, Color c) {
        ui.Rect({rect.x + x * u, rect.y + y * u, w * u, h * u}, c,
                order);
    };
    if (kind == 0) {
        // Heart
        px(2, 3, 4, 3, kRed);
        px(10, 3, 4, 3, kRed);
        px(1, 5, 14, 4, kRed);
        px(3, 9, 10, 3, kRed);
        px(5, 12, 6, 2, kRed);
        px(7, 14, 2, 1, kRed);
        px(3, 4, 2, 2, {255, 130, 130, 255});
    } else if (kind == 1) {
        // Shield
        px(3, 1, 10, 2, {216, 224, 231, 255});
        px(2, 3, 12, 7, {183, 196, 208, 255});
        px(4, 10, 8, 3, {151, 167, 183, 255});
        px(6, 13, 4, 2, {123, 140, 158, 255});
        px(7, 15, 2, 1, {103, 120, 139, 255});
        px(4, 3, 2, 5, {242, 247, 250, 255});
    } else {
        // Energy crystal
        px(7, 1, 2, 2, {121, 219, 255, 255});
        px(5, 3, 6, 3, kEnergy);
        px(4, 6, 8, 4, {36, 125, 214, 255});
        px(5, 10, 6, 3, {31, 98, 184, 255});
        px(7, 13, 2, 2, {27, 74, 147, 255});
        px(5, 5, 2, 4, {175, 239, 255, 255});
    }
}

void GameUI::DrawButton(UIRenderer& ui, const GameUIData& data,
                        const Rect& rect, const std::string& label,
                        bool selected, bool enabled, float size) const {
    const char* frame = label == "NEW GAME" ? "ui_btn5"
        : label == "CONTINUE" ? "ui_btn7"
        : label == "QUIT" ? "ui_btn4"
        : "ui_btn1";
    const char* icon = label == "HOME" ? "ui_98"
        : label == "RESUME" ? "ui_92"
        : (label == "SETTINGS" && screen_ != UIScreen::MainMenu) ? "ui_93"
        : label == "CLOUD/SAVE" ? "ui_116" : nullptr;
    const bool have_asset = data.ui_assets != nullptr &&
        data.ui_assets->Find(frame) != nullptr;
    if (have_asset) {
        DrawAsset(ui, data, frame, rect,
                  enabled ? Color::White() : Color{130, 120, 108, 255},
                  2010, true);
        if (icon != nullptr) {
            DrawAsset(ui, data, icon,
                      {rect.x + 14, rect.y + (rect.h - 30) * 0.5f, 30, 30},
                      Color::White(), 2012);
        }
        if (selected) {
            ui.Panel(rect, {255, 230, 150, 0}, kFrameLight, 3.0f, 2011);
            ui.Rect({rect.x + 5, rect.y + 5, rect.w - 10, 3},
                    {255, 240, 180, 220}, 2012);
        }
    } else {
        const Color fill = !enabled ? Color{77, 67, 55, 255}
            : selected ? Color{156, 104, 45, 255} : Color{111, 81, 48, 255};
        ui.Rect({rect.x + 3, rect.y + 3, rect.w, rect.h},
                {20, 16, 17, 255}, 2010);
        ui.Panel(rect, fill, selected ? kFrameLight : kFrameGold, 3.0f, 2011);
    }
    // All button labels use the same centered content rectangle. The optional
    // icon is painted independently at the left edge and never shifts text.
    const Rect text_rect = rect;
    const Color text_color = !enabled ? Color{142, 130, 112, 255}
        : (selected || label == "NEW GAME" ? Color::White() : kCream);
    ui.TextCentered(label, text_rect, text_color, 2013, size, true);
}

void GameUI::DrawPowerSlots(UIRenderer& ui, const GameUIData& data,
                            const Rect& start, int count,
                            int occupied) const {
    const float slot = 43.0f;
    const float gap = 5.0f;
    for (int i = 0; i < count; ++i) {
        const Rect rect{start.x + static_cast<float>(i) * (slot + gap),
                        start.y, slot, slot};
        ui.Rect({rect.x + 2, rect.y + 2, slot, slot}, {13, 17, 25, 255}, 2001);
        DrawAsset(ui, data, "ui_btn1", rect, Color::White(), 2002, true);
        if (i >= occupied) {
            DrawAsset(ui, data, "ui_99", {rect.x + 10, rect.y + 10,
                                          rect.w - 20, rect.h - 20},
                      Color::White(), 2003);
        }
    }
}

void GameUI::DrawHud(UIRenderer& ui, const GameUIData& data) const {
    if (data.health == nullptr || data.energy == nullptr) return;
    const auto element = [&](const char* id) {
        static const HudLayout fallback;
        return data.layout != nullptr ? data.layout->RectFor(id)
                                      : fallback.RectFor(id);
    };
    const auto has_asset = [&](const char* name) {
        return data.ui_assets != nullptr && data.ui_assets->Find(name) != nullptr;
    };

    const Rect hp_rect = element("hp");
    const Rect armor_rect = element("armor");
    const Rect energy_rect = element("energy");
    // ui_15 is the actual reference status plate (heart/shield/gem icons and
    // three dark bar slots). Derive its bounds from the editable row rects so
    // UI Design Mode still moves the real HP/armor/energy elements.
    const float plate_left = std::min({hp_rect.x, armor_rect.x, energy_rect.x}) - 74.0f;
    const float plate_top = std::min({hp_rect.y, armor_rect.y, energy_rect.y}) - 14.0f;
    const float plate_right = std::max({hp_rect.x + hp_rect.w,
                                        armor_rect.x + armor_rect.w,
                                        energy_rect.x + energy_rect.w}) + 10.0f;
    const float plate_bottom = std::max({hp_rect.y + hp_rect.h,
                                         armor_rect.y + armor_rect.h,
                                         energy_rect.y + energy_rect.h}) + 14.0f;
    const Rect plate{plate_left, plate_top, plate_right - plate_left,
                     plate_bottom - plate_top};
    const bool plate_asset = has_asset("ui_15");
    if (plate_asset) {
        DrawAsset(ui, data, "ui_15", plate, Color::White(), 1900);
    }

    const struct {
        const char* id;
        int icon;
        int value;
        int maximum;
        Color color;
        Rect rect;
    } rows[] = {
        {"hp", 0, data.health->hp, data.health->max_hp, kHealth, hp_rect},
        {"armor", 1, data.health->armor, data.health->max_armor, kArmor,
         armor_rect},
        {"energy", 2, data.energy->energy, data.energy->max_energy, kEnergy,
         energy_rect},
    };
    for (const auto& row : rows) {
        if (!plate_asset) {
            ui.Panel(row.rect, {23, 21, 28, 220}, {102, 76, 46, 255}, 1.0f,
                     1900);
            DrawIcon(ui, data, row.icon,
                     {row.rect.x + 3, row.rect.y + 4, 30, 30}, 1904);
        }
        const float fraction = row.maximum > 0
            ? std::clamp(static_cast<float>(row.value) /
                         static_cast<float>(row.maximum), 0.0f, 1.0f)
            : 0.0f;
        if (fraction > 0.0f && has_asset("ui_12")) {
            // ui_12 is the reference white bar fill, tinted by the actual
            // HP/armor/energy colors.
            DrawAsset(ui, data, "ui_12",
                      {row.rect.x, row.rect.y + 3,
                       row.rect.w * fraction, row.rect.h - 6},
                      row.color, 1904);
        } else if (fraction > 0.0f) {
            DrawBar(ui, row.rect, fraction, row.color, 1904);
        }
        char text[48];
        std::snprintf(text, sizeof(text), "%d/%d", row.value, row.maximum);
        ui.TextCentered(text, row.rect, Color::White(), 1910, 28.0f, true);
    }

    // Coin and pause stay together at the absolute upper-right edge, using the
    // recovered coin and pause-button art from the reference HUD.
    if (data.show_orbs) {
        const Rect orb = element("orb");
        if (has_asset("ui_52")) {
            DrawAsset(ui, data, "ui_52", {orb.x, orb.y + 6, 36, 36},
                      Color::White(), 1905);
        }
        char orbs_text[32];
        std::snprintf(orbs_text, sizeof(orbs_text), "%d", data.orbs);
        ui.TextFitted(orbs_text, {orb.x + 44, orb.y, orb.w - 44, orb.h},
                      kCream, 1906, engine::rendering::TextAlign::Left,
                      30.0f, 20.0f, true);
    }
    const Rect coin = element("coin");
    if (has_asset("ui_31")) {
        DrawAsset(ui, data, "ui_31", {coin.x, coin.y + 6, 36, 33},
                  Color::White(), 1905);
    } else {
        DrawAsset(ui, data, "ui_5", {coin.x, coin.y + 7, 30, 32},
                  Color::White(), 1905);
    }
    char coins[32];
    std::snprintf(coins, sizeof(coins), "%d", data.coins);
    ui.TextFitted(coins, {coin.x + 44, coin.y, coin.w - 44, coin.h},
                  kCream, 1906, engine::rendering::TextAlign::Left,
                  30.0f, 20.0f, true);

    const Rect pause = element("pause");
    if (has_asset("ui_btn1")) {
        DrawAsset(ui, data, "ui_btn1", pause, Color::White(), 1900, true);
    } else {
        ui.Panel(pause, {42, 29, 22, 235}, kFrameBrown, 2.0f, 1900);
    }
    DrawAsset(ui, data, "ui_94",
              {pause.x + (pause.w - 42) * 0.5f,
               pause.y + (pause.h - 42) * 0.5f - 4.0f, 42, 42},
              Color::White(), 1905);

    DrawMinimap(ui, data, element("minimap"), false);

    const Rect stage = element("stage");
    std::string stage_text = data.room_label;
    if (stage_text.empty()) {
        stage_text = data.scene != nullptr ? data.scene->id : "?";
    }
    if (data.room_cleared) {
        stage_text += "   clear";
    }
    ui.TextFitted(stage_text, stage, kMuted, 1903,
                  engine::rendering::TextAlign::Left, 28.0f, 18.0f, true);

    if (data.transition_flash > 0.0f) {
        const float alpha = std::clamp(data.transition_flash / 0.45f,
                                       0.0f, 1.0f);
        ui.Rect({0, 0, 1920, 1080},
                {0, 0, 0, static_cast<unsigned char>(235.0f * alpha)},
                2000);
    }
    // Recovered UIMap map_info: a 300 px panel sliding in from the right
    // (Unity anchoredPosition 200 -> -20 at 1280x720, scaled to 1920x1080).
    // The Animator transition duration is 0.2 s; the label is the current
    // MapManager.level and the font is LockClock 32 (48 px at this scale).
    if (data.room_title_timer > 0.0f && !data.room_label.empty()) {
        constexpr float kShowSeconds = 3.0f;
        constexpr float kSlideSeconds = 0.2f;
        const float elapsed = kShowSeconds - data.room_title_timer;
        float slide = 0.0f;
        if (elapsed < kSlideSeconds) {
            slide = 1.0f - std::clamp(elapsed / kSlideSeconds, 0.0f, 1.0f);
        } else if (data.room_title_timer < kSlideSeconds) {
            slide = 1.0f - std::clamp(data.room_title_timer / kSlideSeconds,
                                      0.0f, 1.0f);
        }
        const float offset = 330.0f * slide;
        const Rect info{1590.0f + offset, 60.0f, 300.0f, 300.0f};
        ui.Rect(info, {32, 32, 32, 64}, 1910);
        const Rect label_rect{info.x + 30.0f, info.y + 300.0f, 240.0f, 60.0f};
        ui.TextCentered(data.room_label, label_rect, Color::White(), 1912,
                        48.0f, true);
    }

    // Recovered UIMessageBar show_signpost clip (1.5 s): black bar, gold
    // ui_90 line, ui_89 banner and pixel_bold "clear" text.
    if (data.room_clear_timer > 0.0f) {
        const float elapsed = 1.5f - std::clamp(data.room_clear_timer, 0.0f,
                                                1.5f);
        const float banner_alpha = std::clamp(ClearAlpha(elapsed, 0.784314f),
                                              0.0f, 1.0f);
        const float text_alpha = ClearTextAlpha(elapsed);
        const float banner_y = ClearBannerY(elapsed);
        const float center_y = 315.0f;
        ui.Rect({0.0f, center_y - 120.0f, 1920.0f, 240.0f},
                {0, 0, 0, static_cast<unsigned char>(160.0f * banner_alpha)},
                1930);
        if (data.ui_assets != nullptr &&
            data.ui_assets->Find("ui_90") != nullptr) {
            ui.Image(data.ui_assets->Find("ui_90")->texture_id,
                     data.ui_assets->Find("ui_90")->uv,
                     {960.0f - 210.0f, center_y - 140.0f, 420.0f, 280.0f},
                     {255, 174, 28,
                      static_cast<unsigned char>(200.0f * banner_alpha)},
                     1931);
        }
        if (data.ui_assets != nullptr &&
            data.ui_assets->Find("ui_89") != nullptr) {
            ui.Image(data.ui_assets->Find("ui_89")->texture_id,
                     data.ui_assets->Find("ui_89")->uv,
                     {960.0f - 150.0f, center_y - 110.0f + banner_y,
                      300.0f, 220.0f},
                     {255, 174, 28,
                      static_cast<unsigned char>(200.0f * banner_alpha)},
                     1932);
        }
        ui.TextCentered("clear",
                        {960.0f - 250.0f, center_y - 75.0f, 500.0f, 150.0f},
                        {255, 255, 255,
                         static_cast<unsigned char>(255.0f * text_alpha)},
                        1933, 144.0f, true);
    }

    // Character power, deliberately larger than a weapon slot. ui_joy_0 is the
    // recovered joystick base and ui_joy_3 is the actual white lightning icon
    // used by the reference skill button.
    const Rect power = element("ability");
    const bool power_asset = has_asset("ui_joy_0") && has_asset("ui_joy_3");
    if (power_asset) {
        DrawAsset(ui, data, "ui_joy_0", power, Color::White(), 1900);
        const float icon_h = power.h * 0.54f;
        const float icon_w = icon_h * (53.0f / 91.0f);
        const Rect icon_rect{power.x + (power.w - icon_w) * 0.5f,
                             power.y + (power.h - icon_h) * 0.5f - 4.0f,
                             icon_w, icon_h};
        // Reference UICanvas.UpdateSkillMask uses Image.fillAmount on
        // skill_icon_mask and enables skill_mask_ready at 100%. Reproduce the
        // same vertical fill with the actual dark/white lightning sprites.
        const auto ability_state = static_cast<
            soulknight::game::player::AbilityState>(data.ability_state);
        const float charge = soulknight::game::player::AbilityChargeFraction(
            ability_state, data.ability_cooldown);
        DrawAsset(ui, data, "ui_joy_2", icon_rect,
                  data.ability_state == 0 ? Color{100, 100, 100, 255}
                                         : Color{45, 52, 60, 255}, 1904);
        const UIAssetSprite* lightning =
            data.ui_assets != nullptr ? data.ui_assets->Find("ui_joy_3")
                                      : nullptr;
        if (lightning != nullptr && charge > 0.0f) {
            const float clipped_h = icon_h * charge;
            const Rect uv{lightning->uv.x,
                          lightning->uv.y + lightning->uv.h * (1.0f - charge),
                          lightning->uv.w, lightning->uv.h * charge};
            ui.Image(lightning->texture_id, uv,
                     {icon_rect.x, icon_rect.y + (icon_h - clipped_h),
                      icon_w, clipped_h},
                     data.ability_state == 2 ? Color{120, 205, 255, 255}
                                             : Color{55, 155, 255, 255},
                     1905);
        }
        if (data.ability_state == 1) {
            // Ready glow: a subtle blue ring around the recovered base.
            ui.Panel(power, {0, 0, 0, 0}, {78, 174, 255, 230}, 3.0f, 1903);
        }
    } else {
        ui.Panel(power, {32, 29, 40, 235},
                 data.ability_state == 1 ? kFrameLight : kFrameBrown,
                 2.0f, 1900);
        DrawAsset(ui, data, "ui_skill01",
                  {power.x + 8, power.y + 8, power.w - 16, power.h - 16},
                  data.ability_state == 0 ? Color{90, 90, 90, 255}
                                         : Color::White(), 1905);
    }
    ui.TextCentered("Q", {power.x, power.y + power.h - 26, power.w, 22},
                    kCream, 1907, 18.0f);
    // Recovered blue-diamond charge marker.
    if (has_asset("ui_joy_7")) {
        DrawAsset(ui, data, "ui_joy_7",
                  {power.x + power.w - 34, power.y - 8, 38, 38},
                  data.ability_state == 0 ? Color{84, 84, 84, 255}
                                         : Color::White(), 1906);
    }

    // Weapon strip and active-slot indicator.
    if (data.weapons != nullptr && data.inventory != nullptr) {
        const Rect slots = element("weapon_slots");
        const float gap = 8.0f;
        const float slot_w = (slots.w - gap) * 0.5f;
        for (std::size_t slot = 0; slot < 2; ++slot) {
            const Rect slot_rect{slots.x + static_cast<float>(slot) *
                                               (slot_w + gap),
                                 slots.y, slot_w, slots.h};
            const bool active = data.inventory->ActiveSlot() == slot;
            const bool occupied = data.inventory->SlotOccupied(slot);
            ui.Rect(slot_rect, {20, 24, 33, 235}, 1900);
            const char* frame = active ? "ui_btn3" : "ui_btn1";
            if (has_asset(frame)) {
                DrawAsset(ui, data, frame, slot_rect, Color::White(), 1901,
                          true);
            } else {
                ui.Panel(slot_rect, {25, 29, 39, 232},
                         active ? kFrameLight : kFrameBrown,
                         active ? 3.0f : 2.0f, 1901);
            }
            if (occupied) {
                const std::size_t index = data.inventory->Slot(slot);
                if (index < data.weapons->Count()) {
                    const auto& weapon = data.weapons->Definitions()[index];
                    if (weapon.visual_sprite_index >= 0) {
                        ui.SceneSprite(weapon.visual_sprite_index,
                                       {slot_rect.x + 6, slot_rect.y + 6,
                                        slot_rect.w - 12, slot_rect.h - 26},
                                       Color::White(), 1904);
                    }
                }
            } else {
                ui.TextCentered("-", {slot_rect.x, slot_rect.y + 14,
                                       slot_rect.w, 30},
                                {130, 128, 122, 255}, 1904, 28.0f);
            }
            ui.TextCentered(slot == 0 ? "1" : "2",
                            {slot_rect.x, slot_rect.y + slot_rect.h - 24,
                             slot_rect.w, 22},
                            active ? kGold : kMuted, 1905, 16.0f);
        }
        const Rect active_rect = element("active_weapon");
        ui.TextFitted(data.inventory->ActiveSlot() == 0 ? "PRIMARY" : "SECONDARY",
                      active_rect, kGold, 1905,
                      engine::rendering::TextAlign::Left, 28.0f, 18.0f, true);
    }

    const Rect prompt = element("pickup_prompt");
    const bool card_asset = has_asset("ui_191");
    if (data.chest_nearby) {
        if (card_asset) {
            DrawAsset(ui, data, "ui_191", prompt, Color::White(), 1920, true);
        } else {
            ui.Panel(prompt, {26, 25, 30, 246}, kFrameGold, 2.0f, 1920);
        }
        ui.TextCentered("E  OPEN CHEST",
                        {prompt.x + 16, prompt.y + 14, prompt.w - 32, 34},
                        Color::White(), 1921, 30.0f, true);
        ui.TextCentered(data.chest_label,
                        {prompt.x + 16, prompt.y + 58, prompt.w - 32, 30},
                        kGold, 1921, 28.0f, false);
    } else if (data.pickup_nearby) {
        if (card_asset) {
            DrawAsset(ui, data, "ui_191", prompt, Color::White(), 1920, true);
        } else {
            ui.Panel(prompt, {26, 25, 30, 246}, kFrameGold, 2.0f, 1920);
        }
        float text_x = prompt.x + 18;
        float text_w = prompt.w - 36;
        if (data.pickup_sprite_index >= 0) {
            ui.SceneSprite(data.pickup_sprite_index,
                           {prompt.x + 18, prompt.y + 38, 92, 66},
                           Color::White(), 1921);
            text_x += 104;
            text_w -= 104;
        }
        ui.TextCentered("E  PICK UP",
                        {text_x, prompt.y + 10, text_w, 30},
                        Color::White(), 1921, 28.0f, true);
        if (!data.pickup_name.empty()) {
            ui.TextCentered(data.pickup_name,
                            {text_x, prompt.y + 46, text_w, 32},
                            kGold, 1921, 30.0f, true);
        }
        char stats[96];
        std::snprintf(stats, sizeof(stats), "DMG %d    ENERGY %d",
                      data.pickup_damage, data.pickup_energy_cost);
        ui.TextCentered(stats,
                        {text_x, prompt.y + 88, text_w, 34},
                        kMuted, 1921, 26.0f, false);
    } else if (data.interaction_nearby) {
        if (card_asset) {
            DrawAsset(ui, data, "ui_191", prompt, Color::White(), 1920, true);
        } else {
            ui.Panel(prompt, {32, 27, 25, 242}, kFrameGold, 2.0f, 1920);
        }
        ui.TextCentered("E  INTERACT",
                        {prompt.x + 16, prompt.y + 18, prompt.w - 32, 36},
                        Color::White(), 1921, 30.0f, true);
        ui.TextCentered(data.interaction_label,
                        {prompt.x + 16, prompt.y + 62, prompt.w - 32, 32},
                        kGold, 1921, 28.0f, false);
    }

    DrawDialogue(ui, data);
    if (data.stake_damage_timer > 0.0f && data.stake_damage > 0) {
        char damage[32];
        std::snprintf(damage, sizeof(damage), "%d", data.stake_damage);
        const float rise = (0.8f - data.stake_damage_timer) * 55.0f;
        ui.TextCentered(damage,
                        {data.stake_damage_screen.x - 90.0f,
                         data.stake_damage_screen.y - 120.0f - rise,
                         180.0f, 40.0f},
                        {255, 224, 120, 255}, 1960, 34.0f, true);
    }
    if (data.ui_design_mode) DrawDesignOverlay(ui, data);
}

void GameUI::DrawDialogue(UIRenderer& ui, const GameUIData& data) const {
    if (data.dialogue_timer <= 0.0f || data.dialogue_text.empty()) return;
    // Recovered UICanvas.ShowTextTalk presentation: a compact bottom speech
    // panel with the speaker name above the localized line.
    const float alpha = std::clamp(data.dialogue_timer, 0.0f, 1.0f);
    const engine::Color border{198, 161, 91, static_cast<std::uint8_t>(255 * alpha)};
    const engine::Color fill{22, 20, 26, static_cast<std::uint8_t>(232 * alpha)};
    const Rect panel{420, 850, 1080, 156};
    ui.Panel(panel, fill, border, 3.0f, 1950);
    if (!data.dialogue_speaker.empty()) {
        ui.TextFitted(data.dialogue_speaker,
                      {panel.x + 28, panel.y + 12, panel.w - 56, 32},
                      {226, 190, 112, static_cast<std::uint8_t>(255 * alpha)},
                      1951, engine::rendering::TextAlign::Left,
                      26.0f, 18.0f, true);
    }
    ui.TextFitted(data.dialogue_text,
                  {panel.x + 28, panel.y + 50, panel.w - 56, 92},
                  {245, 240, 226, static_cast<std::uint8_t>(255 * alpha)},
                  1951, engine::rendering::TextAlign::Left,
                  28.0f, 18.0f, false);
}

void GameUI::DrawMinimap(UIRenderer& ui, const GameUIData& data,
                         const engine::Rect& rect, bool expanded) const {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;
    const int32_t base = expanded ? 1984 : 1900;
    // The reference map_info uses an unsprite dark quad, not an atlas panel.
    ui.Rect(rect, {12, 15, 22, static_cast<unsigned char>(expanded ? 238 : 205)}, base);
    ui.Rect({rect.x, rect.y, rect.w, 2}, {70, 83, 103, 220}, base + 1);
    if (!expanded) {
        ui.TextFitted("MAP", {rect.x + 10, rect.y + 4, rect.w - 20, 22},
                      kMuted, base + 5, engine::rendering::TextAlign::Left,
                      24.0f, 14.0f, false);
    }
    if (data.map_rooms.empty()) {
        ui.TextFitted("NO MAP", {rect.x + 10, rect.y + rect.h * 0.5f - 14,
                                 rect.w - 20, 28},
                      kMuted, base + 5, engine::rendering::TextAlign::Center,
                      24.0f, 16.0f, false);
        return;
    }
    int min_x = data.map_rooms.front().x;
    int max_x = min_x;
    int min_y = data.map_rooms.front().y;
    int max_y = min_y;
    for (const MapRoomView& room : data.map_rooms) {
        min_x = std::min(min_x, room.x);
        max_x = std::max(max_x, room.x);
        min_y = std::min(min_y, room.y);
        max_y = std::max(max_y, room.y);
    }
    const int cols = std::max(1, max_x - min_x + 1);
    const int rows = std::max(1, max_y - min_y + 1);
    const float title_h = expanded ? 34.0f : 28.0f;
    const float inner_pad = expanded ? 46.0f : 16.0f;
    const float available_w = rect.w - inner_pad * 2.0f;
    const float available_h = rect.h - inner_pad * 2.0f - title_h;
    const float gap = expanded ? 26.0f : 10.0f;
    const float cell = std::clamp(std::min(
        (available_w - static_cast<float>(cols - 1) * gap) /
            static_cast<float>(cols),
        (available_h - static_cast<float>(rows - 1) * gap) /
            static_cast<float>(rows)), 16.0f, expanded ? 64.0f : 44.0f);
    const float grid_w = static_cast<float>(cols) * cell +
                         static_cast<float>(cols - 1) * gap;
    const float grid_h = static_cast<float>(rows) * cell +
                         static_cast<float>(rows - 1) * gap;
    const float origin_x = rect.x + (rect.w - grid_w) * 0.5f;
    const float origin_y = rect.y + title_h +
                           (rect.h - title_h - grid_h) * 0.5f;
    const auto center = [&](int x, int y) {
        return engine::Vec2{
            origin_x + static_cast<float>(x - min_x) * (cell + gap) +
                cell * 0.5f,
            origin_y + static_cast<float>(y - min_y) * (cell + gap) +
                cell * 0.5f};
    };
    // UIMap connectors are unsprite colour quads in the reference prefab.
    for (const MapLinkView& link : data.map_links) {
        const engine::Vec2 a = center(link.from_x, link.from_y);
        const engine::Vec2 b = center(link.to_x, link.to_y);
        const Color link_color{156, 156, 156, 220};
        if (std::abs(a.y - b.y) < 1.0f) {
            ui.Rect({std::min(a.x, b.x), a.y - 2.0f,
                     std::abs(a.x - b.x), 4.0f}, link_color, base + 1);
        } else if (std::abs(a.x - b.x) < 1.0f) {
            ui.Rect({a.x - 2.0f, std::min(a.y, b.y),
                     4.0f, std::abs(a.y - b.y)}, link_color, base + 1);
        } else {
            ui.Rect({std::min(a.x, b.x), a.y - 2.0f,
                     std::abs(a.x - b.x), 4.0f}, link_color, base + 1);
            ui.Rect({b.x - 2.0f, std::min(a.y, b.y),
                     4.0f, std::abs(a.y - b.y)}, link_color, base + 1);
        }
    }
    const auto has_asset = [&](const char* name) {
        return data.ui_assets != nullptr && data.ui_assets->Find(name) != nullptr;
    };
    for (const MapRoomView& room : data.map_rooms) {
        const engine::Vec2 c = center(room.x, room.y);
        const Rect cell_rect{c.x - cell * 0.5f, c.y - cell * 0.5f,
                             cell, cell};
        // Reference UIMap colors: white current, light gray visited, dark gray
        // unexplored. Cells and connectors are plain tinted quads.
        const Color fill = room.devroom ? Color{70, 70, 70, 220}
            : room.current ? Color{247, 242, 221, 255}
            : room.visited ? Color{180, 180, 180, 235}
                           : Color{70, 70, 70, 200};
        ui.Rect(cell_rect, fill, base + 3);
        if (room.current) {
            ui.Panel(cell_rect, {0, 0, 0, 0}, kFrameLight, 2.0f, base + 4);
        }
        std::string icon;
        if (room.devroom) {
            icon = "D";
        } else if (room.icon == "home") {
            icon = "ui_125";
        } else if (room.icon == "boss") {
            icon = "ui_14";
        } else if (room.icon == "special") {
            icon = "ui_114";
        } else if (room.icon == "chest") {
            icon = "ui_113";
        } else if (room.icon == "cage") {
            icon = "ui_193";
        }
        if (icon == "D") {
            ui.TextCentered("D", cell_rect, Color::White(), base + 6,
                            24.0f, true);
        } else if (!icon.empty() && has_asset(icon.c_str())) {
            const float icon_side = std::max(10.0f, cell * 0.62f);
            DrawAsset(ui, data, icon,
                      {c.x - icon_side * 0.5f, c.y - icon_side * 0.5f,
                       icon_side, icon_side},
                      Color::White(), base + 6);
        } else if (room.current && has_asset("ui_4")) {
            DrawAsset(ui, data, "ui_4",
                      {c.x - 8, c.y - 8, 16, 16}, Color::White(),
                      base + 6);
        }
    }
}

void GameUI::DrawDesignOverlay(UIRenderer& ui, const GameUIData& data) const {
    if (data.layout == nullptr) return;
    ui.Rect({0, 0, 1920, 1080}, {3, 5, 10, 80}, 1960);
    ui.TextFitted("UI DESIGN MODE  -  DRAG UI ELEMENTS",
                  {500, 18, 920, 40}, Color::White(), 1961,
                  engine::rendering::TextAlign::Center, 32.0f, 20.0f, true);
    for (const HudElement& element : data.layout->Elements()) {
        const bool selected = element.id == data.ui_design_selected;
        ui.Panel(element.rect,
                 {0, 0, 0, 0},
                 selected ? kFrameLight : Color{70, 170, 230, 190},
                 selected ? 3.0f : 1.0f, 1962);
        if (selected) {
            const float label_y = std::max(28.0f, element.rect.y - 24.0f);
            const Rect label_back{element.rect.x, label_y - 2,
                                  std::max(90.0f, element.rect.w), 24};
            ui.Rect(label_back, {8, 14, 24, 225}, 1963);
            ui.TextFitted(element.id,
                          {element.rect.x + 4, label_y,
                           std::max(80.0f, element.rect.w - 8), 20},
                          kFrameLight, 1964,
                          engine::rendering::TextAlign::Left,
                          18.0f, 14.0f, true);
        }
    }
    ui.TextFitted("ESC SAVE/EXIT   R RESET LAYOUT   S SAVE   F4 DEBUG",
                  {360, 58, 1200, 32}, kCream, 1964,
                  engine::rendering::TextAlign::Center, 26.0f, 16.0f, true);
    (void)data.design_mouse;
}

void GameUI::DrawPause(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 176}, 1980);
    const Rect panel{420, 258, 1080, 564};
    if (data.ui_assets != nullptr && data.ui_assets->Find("ui_2") != nullptr) {
        DrawAsset(ui, data, "ui_2", panel, Color{116, 126, 148, 255}, 1982,
                  true);
    } else {
        ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1982);
    }
    ui.Rect({panel.x + 12, panel.y + 12, panel.w - 24, panel.h - 24},
            {25, 30, 44, 238}, 1983);

    // PAUSE ribbon, recovered from the actual pause prefab.
    const Rect ribbon{580, 228, 760, 104};
    if (data.ui_assets != nullptr && data.ui_assets->Find("ui_13") != nullptr) {
        DrawAsset(ui, data, "ui_13", ribbon, Color::White(), 1986);
    } else {
        ui.Panel(ribbon, {160, 35, 40, 255}, {94, 22, 28, 255}, 4.0f, 1986);
    }
    ui.TextCentered("PAUSE", ribbon, Color::White(), 1988, 52.0f, true);

    // Character portrait framed with the recovered ui_22 frame.
    const Rect portrait{452, 360, 124, 124};
    if (data.ui_assets != nullptr && data.ui_assets->Find("ui_22") != nullptr) {
        DrawAsset(ui, data, "ui_22", portrait, Color::White(), 1989);
    } else {
        ui.Panel(portrait, {47, 67, 101, 255}, kFrameGold, 4.0f, 1989);
    }
    if (data.characters != nullptr && data.characters->Count() > 0) {
        const auto& character = data.characters->Active();
        if (character.body_sprite_index >= 0) {
            ui.SceneSprite(character.body_sprite_index,
                           {portrait.x + 8, portrait.y + 8,
                            portrait.w - 16, portrait.h - 16},
                           Color::White(), 1991);
        }
    }

    DrawPowerSlots(ui, data, Rect{600, 366, 0, 0}, 6, 0);
    for (int i = 0; i < 3; ++i) {
        const Rect slot{600.0f + static_cast<float>(i) * 48.0f, 432.0f,
                        43.0f, 43.0f};
        ui.Panel(slot, {28, 32, 42, 255}, {68, 74, 88, 255}, 2.0f, 1990);
        ui.TextCentered("-", slot, {189, 184, 174, 255}, 1991, 26.0f);
    }

    const Rect buttons[3] = {
        {470, 610, 300, 120},
        {810, 610, 300, 120},
        {1150, 610, 300, 120}
    };
    const char* icons[3] = {"ui_98", "ui_92", "ui_93"};
    for (int i = 0; i < 3; ++i) {
        const Rect& button = buttons[i];
        const bool selected = pause_index_ == i;
        if (data.ui_assets != nullptr &&
            data.ui_assets->Find("ui_38") != nullptr) {
            DrawAsset(ui, data, "ui_38", button, Color::White(), 2010, true);
            if (selected) {
                ui.Panel(button, {255, 230, 150, 0}, kFrameLight, 3.0f, 2011);
            }
        } else {
            ui.Panel(button, selected ? Color{156, 104, 45, 255}
                                      : Color{111, 81, 48, 255},
                     selected ? kFrameLight : kFrameGold, 3.0f, 2010);
        }
        if (data.ui_assets != nullptr &&
            data.ui_assets->Find(icons[i]) != nullptr) {
            DrawAsset(ui, data, icons[i],
                      {button.x + 16, button.y + (button.h - 34) * 0.5f,
                       34, 34}, Color::White(), 2012);
        }
        ui.TextCentered(kPauseItems[static_cast<std::size_t>(i)], button,
                        selected ? Color::White() : kCream, 2013, 28.0f, true);
    }
    ui.TextCentered("ESC  RESUME    ENTER  SELECT", {420, 760, 1080, 24},
                    kMuted, 1995, 18.0f);
}

void GameUI::DrawSettings(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 208}, 1980);
    const Rect panel{420, 178, 1080, 724};
    if (data.ui_assets != nullptr && data.ui_assets->Find("ui_2") != nullptr) {
        DrawAsset(ui, data, "ui_2", panel, Color{116, 126, 148, 255}, 1982,
                  true);
    } else {
        ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1982);
    }
    ui.Rect({panel.x + 12, panel.y + 12, panel.w - 24, panel.h - 24},
            {25, 30, 44, 245}, 1983);

    const Rect ribbon{panel.x + 230, panel.y - 36, panel.w - 460, 96};
    if (data.ui_assets != nullptr && data.ui_assets->Find("ui_13") != nullptr) {
        DrawAsset(ui, data, "ui_13", ribbon, Color::White(), 1986);
    } else {
        ui.Panel(ribbon, {160, 35, 40, 255}, {94, 22, 28, 255}, 4.0f, 1986);
    }
    ui.TextCentered("SETTINGS", ribbon, Color::White(), 1988, 44.0f, true);

    const char* values[6] = {
        data.auto_aim ? "AUTO AIM" : "MANUAL",
        data.fullscreen ? "ON" : "OFF",
        data.debug_overlay ? "ON" : "OFF",
        "OPEN",
        "CONFIRM",
        ""
    };
    constexpr float row_height = 66.0f;
    constexpr float row_gap = 12.0f;
    const float rows_total = 6.0f * row_height + 5.0f * row_gap;
    const float row_top = panel.y + (panel.h - rows_total) * 0.5f - 10.0f;
    for (int i = 0; i < 6; ++i) {
        const Rect row{panel.x + 72,
                       row_top + static_cast<float>(i) * (row_height + row_gap),
                       panel.w - 144, row_height};
        const bool selected = settings_index_ == i;
        const char* frame = selected ? "ui_btn3" : "ui_btn1";
        if (data.ui_assets != nullptr && data.ui_assets->Find(frame) != nullptr) {
            DrawAsset(ui, data, frame, row, Color::White(), 1990, true);
        } else {
            ui.Panel(row, selected ? Color{74, 65, 53, 255}
                                   : Color{37, 41, 55, 255},
                     selected ? kFrameLight : Color{68, 76, 93, 255},
                     2.0f, 1990);
        }
        // Disjoint label/value content rects guarantee that neither can spill
        // out of the frame or overlap the other. Both are vertically centered.
        const Rect label_rect{row.x + 26, row.y + 2, row.w * 0.54f,
                              row.h - 4};
        const Rect value_rect{row.x + row.w - 306, row.y + 2, 278,
                              row.h - 4};
        ui.TextFitted(kSettingsItems[static_cast<std::size_t>(i)],
                      label_rect, selected ? Color::White() : kCream, 1991,
                      engine::rendering::TextAlign::Left, 30.0f, 20.0f, true);
        if (i < 4 && values[i][0] != '\0') {
            ui.TextFitted(values[i], value_rect,
                          selected ? kGold : kMuted, 1992,
                          engine::rendering::TextAlign::Right, 30.0f,
                          20.0f, false);
        }
    }
    ui.TextFitted("ESC  BACK       ENTER  SELECT",
                  {panel.x + 40, panel.y + panel.h - 54, panel.w - 80, 28},
                  kMuted, 1995, engine::rendering::TextAlign::Center,
                  24.0f, 16.0f, false);
}

void GameUI::DrawMainMenu(UIRenderer& ui, const GameUIData& data) const {
    bool drew_background = false;
    if (data.ui_assets != nullptr) {
        if (data.ui_assets->Find("menu_backdrop") != nullptr) {
            DrawAsset(ui, data, "menu_backdrop", {0, 0, 1920, 1080},
                      Color::White(), 1980);
            drew_background = true;
        }
        if (data.ui_assets->Find("menu_swirl") != nullptr) {
            // Actual reference swirl. It is intentionally kept inside the
            // frame so no artwork is clipped at the left or right edge.
            DrawAsset(ui, data, "menu_swirl", {1000, 80, 800, 694},
                      Color::White(), 1982);
        }
    }
    if (!drew_background) {
        ui.Rect({0, 0, 1920, 1080}, {7, 8, 12, 255}, 1980);
        ui.Rect({720, 0, 1200, 1080}, {15, 34, 42, 255}, 1982);
        ui.Rect({760, 0, 1120, 1080}, {17, 45, 51, 255}, 1983);
        ui.Rect({800, 0, 1080, 1080}, {20, 55, 59, 255}, 1984);
    }

    // The backed-in silhouette, tunnel and green light in `menu_backdrop` are
    // the actual reference main-menu art. No runtime c00 sprite is used.
    ui.TextFitted("LIGHT", {64, 88, 700, 96}, kCream, 1991,
                  engine::rendering::TextAlign::Left, 96.0f, 72.0f, true);
    ui.TextFitted("KNIGHT", {64, 180, 700, 96}, kGold, 1991,
                  engine::rendering::TextAlign::Left, 96.0f, 72.0f, true);
    ui.TextFitted("NATIVE LINUX DEVROOM", {68, 300, 620, 28},
                  {133, 201, 204, 255}, 1993,
                  engine::rendering::TextAlign::Left, 28.0f, 18.0f, false);

    const Rect droom = MenuRect(data.layout, "menu_droom",
                                MainMenuDroomRect());
    const bool droom_selected = main_index_ == 4;
    ui.Panel(droom, {21, 57, 112, 255},
             droom_selected ? kFrameLight : Color{57, 117, 190, 255},
             droom_selected ? 3.0f : 2.0f, 1990);
    ui.TextCentered("D-ROOM", droom, Color::White(), 1991, 30.0f, true);

    // Four equal player-menu rows. Width/height follow the reference button
    // proportions (roughly 3.8:1); the stack itself is the requested native
    // vertical composition. Every label goes through TextCentered.
    static const char* kIds[4] = {"menu_new_game", "menu_continue",
                                  "menu_settings", "menu_quit"};
    for (int i = 0; i < 4; ++i) {
        const Rect button = MenuRect(data.layout, kIds[i],
                                     MainMenuButtonRect(i));
        DrawButton(ui, data, button, kMainItems[static_cast<std::size_t>(i)],
                   main_index_ == i, i != 1 || data.save_exists, 46.0f);
    }
    const Rect save_load = MenuRect(data.layout, "menu_save_load",
                                    MainMenuSaveLoadRect());
    DrawButton(ui, data, save_load, "S/L", main_index_ == 5, true, 34.0f);
    ui.TextFitted("LOCAL ONLY  //  NO SOCIAL FEATURES",
                  {760, 1004, 1060, 30}, {118, 145, 148, 255}, 1995,
                  engine::rendering::TextAlign::Right, 24.0f, 16.0f, false);
}

void GameUI::DrawSaveLoad(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 210}, 1980);
    const Rect panel{360, 180, 1200, 720};
    ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1982);
    ui.Panel({panel.x + 16, panel.y + 16, panel.w - 32, panel.h - 32},
             kPanelDark, {112, 92, 63, 255}, 3.0f, 1984);
    ui.TextCentered("SAVE / LOAD", {panel.x, panel.y + 22, panel.w, 56},
                    Color::White(), 1990, 36.0f, true);
    const Rect info{panel.x + 64, panel.y + 96, panel.w - 128, 210};
    ui.Panel(info, {16, 20, 31, 230}, {67, 77, 97, 255}, 2.0f, 1990);
    ui.TextFitted("SLOT 0", {info.x + 24, info.y + 16, 200, 28},
                  kGold, 1991, engine::rendering::TextAlign::Left,
                  26.0f, 16.0f, true);
    ui.TextFitted(data.save_slot_summary.empty()
                      ? std::string("NO NATIVE SAVE")
                      : data.save_slot_summary,
                  {info.x + 24, info.y + 56, info.w - 48, info.h - 72},
                  data.save_slot_summary.empty() ? kMuted : kCream, 1992,
                  engine::rendering::TextAlign::Left, 22.0f, 15.0f, false);
    static const char* kItems[4] = {"SAVE", "LOAD", "DELETE", "BACK"};
    for (int i = 0; i < 4; ++i) {
        const Rect button{panel.x + 64 + static_cast<float>(i) * 272.0f,
                          panel.y + 360, 248, 104};
        DrawButton(ui, data, button, kItems[i], save_load_index_ == i,
                   i == 0 || data.save_exists, 30.0f);
    }
    ui.TextCentered("UP/DOWN SELECT   ENTER CONFIRM   ESC BACK",
                    {panel.x, panel.y + 520, panel.w, 28}, kMuted, 1994,
                    18.0f);
}

void GameUI::DrawTrader(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 190}, 1980);
    const Rect panel{220, 150, 1480, 780};
    ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1982);
    ui.Panel({panel.x + 16, panel.y + 16, panel.w - 32, panel.h - 32},
             kPanelDark, {112, 92, 63, 255}, 3.0f, 1984);
    ui.TextCentered("ORB WEAPON TRADER", {panel.x, panel.y + 22, panel.w, 52},
                    Color::White(), 1990, 36.0f, true);
    ui.TextFitted("PRICES ARE RECOVERED RGWeapon ITEM VALUES",
                  {panel.x + 40, panel.y + 76, panel.w - 80, 26},
                  kMuted, 1991, engine::rendering::TextAlign::Center,
                  18.0f, 13.0f, false);
    const int count = std::min(3, static_cast<int>(data.trader_offers.size()));
    for (int i = 0; i < 3; ++i) {
        const Rect card{panel.x + 54 + static_cast<float>(i) * 466.0f,
                        panel.y + 130, 430, 470};
        const bool selected = i == data.trader_selected;
        ui.Panel(card, selected ? Color{58, 78, 111, 255}
                                : Color{31, 37, 51, 255},
                 selected ? kFrameLight : Color{67, 77, 97, 255},
                 selected ? 3.0f : 2.0f, 1990);
        if (i >= count) continue;
        const TraderOfferView& offer = data.trader_offers[static_cast<std::size_t>(i)];
        if (offer.sprite_index >= 0) {
            ui.SceneSprite(offer.sprite_index,
                           {card.x + 80, card.y + 30, 270, 160},
                           offer.sold ? Color{90, 90, 90, 255} : Color::White(),
                           1991);
        }
        ui.TextFitted(offer.display_name, {card.x + 18, card.y + 200,
                                           card.w - 36, 52},
                      offer.sold ? kMuted : Color::White(), 1992,
                      engine::rendering::TextAlign::Center, 28.0f, 18.0f, true);
        char stats[160];
        std::snprintf(stats, sizeof(stats), "DMG %d   ENERGY %d",
                      offer.damage, offer.energy_cost);
        ui.TextCentered(stats, {card.x + 18, card.y + 260, card.w - 36, 28},
                        kCream, 1992, 20.0f);
        if (data.ui_assets != nullptr &&
            data.ui_assets->Find("ui_52") != nullptr) {
            DrawAsset(ui, data, "ui_52",
                      {card.x + card.w * 0.5f - 108, card.y + 310, 34, 34},
                      Color::White(), 1993);
        }
        char price[32];
        std::snprintf(price, sizeof(price), "%d", offer.price);
        ui.TextFitted(offer.sold ? "SOLD" : price,
                      {card.x + card.w * 0.5f - 64, card.y + 306, 170, 36},
                      offer.sold ? kMuted : kGold, 1994,
                      engine::rendering::TextAlign::Left, 30.0f, 20.0f, true);
    }
    ui.TextCentered("LEFT/RIGHT SELECT   ENTER BUY   ESC CLOSE",
                    {panel.x, panel.y + 640, panel.w, 30}, kMuted, 1995,
                    20.0f);
}

void GameUI::DrawWorkshop(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 200}, 1980);
    const Rect panel{140, 90, 1640, 900};
    ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1982);
    ui.Panel({panel.x + 16, panel.y + 16, panel.w - 32, panel.h - 32},
             kPanelDark, {112, 92, 63, 255}, 3.0f, 1984);
    ui.TextCentered("WEAPON WORKSHOP", {panel.x, panel.y + 20, panel.w, 54},
                    Color::White(), 1990, 36.0f, true);
    ui.TextFitted("RECOVERED WEAPON MATERIAL RECIPES",
                  {panel.x + 40, panel.y + 76, panel.w - 80, 24},
                  kMuted, 1991, engine::rendering::TextAlign::Center,
                  18.0f, 13.0f, false);
    const Rect list{panel.x + 42, panel.y + 116, 760, 700};
    ui.Panel(list, {16, 20, 31, 230}, {67, 77, 97, 255}, 2.0f, 1990);
    const int count = static_cast<int>(data.workshop_recipes.size());
    const int selected = std::clamp(
        data.workshop_selected, 0,
        count > 0 ? count - 1 : 0);
    const int visible = 12;
    int first = std::max(0, selected - visible / 2);
    first = std::min(first, std::max(0, count - visible));
    for (int row = 0; row < visible && first + row < count; ++row) {
        const int index = first + row;
        const WorkshopRecipeView& recipe =
            data.workshop_recipes[static_cast<std::size_t>(index)];
        const Rect line{list.x + 12, list.y + 12 + static_cast<float>(row) * 56,
                        list.w - 24, 50};
        const bool is_selected = index == selected;
        if (is_selected) {
            ui.Panel(line, {58, 78, 111, 255}, kFrameLight, 2.0f, 1991);
        }
        ui.TextFitted(recipe.name, {line.x + 12, line.y + 8, line.w - 220, 34},
                      is_selected ? kGold : kCream, 1992,
                      engine::rendering::TextAlign::Left, 26.0f, 16.0f,
                      is_selected);
        ui.TextFitted(recipe.affordable ? "READY" : "MISSING",
                      {line.x + line.w - 200, line.y + 8, 188, 34},
                      recipe.affordable ? kGreen : kMuted, 1992,
                      engine::rendering::TextAlign::Right, 22.0f, 14.0f,
                      false);
    }
    const Rect detail{panel.x + 830, panel.y + 116, panel.w - 872, 700};
    ui.Panel(detail, {16, 20, 31, 230}, {67, 77, 97, 255}, 2.0f, 1990);
    if (count > 0) {
        const WorkshopRecipeView& recipe =
            data.workshop_recipes[static_cast<std::size_t>(selected)];
        if (recipe.sprite_index >= 0) {
            ui.SceneSprite(recipe.sprite_index,
                           {detail.x + detail.w * 0.5f - 120,
                            detail.y + 24, 240, 150}, Color::White(), 1991);
        }
        ui.TextFitted(recipe.name,
                      {detail.x + 24, detail.y + 184, detail.w - 48, 44},
                      kGold, 1992, engine::rendering::TextAlign::Center,
                      32.0f, 20.0f, true);
        char stats[128];
        std::snprintf(stats, sizeof(stats), "DMG %d   ENERGY %d",
                      recipe.damage, recipe.energy_cost);
        ui.TextCentered(stats,
                        {detail.x + 24, detail.y + 232, detail.w - 48, 30},
                        kCream, 1992, 22.0f);
        ui.TextFitted("MATERIALS",
                      {detail.x + 24, detail.y + 280, detail.w - 48, 26},
                      kMuted, 1993, engine::rendering::TextAlign::Left,
                      20.0f, 14.0f, false);
        for (std::size_t i = 0; i < recipe.materials.size(); ++i) {
            ui.TextFitted(recipe.materials[i],
                          {detail.x + 40,
                           detail.y + 314 + static_cast<float>(i) * 34,
                           detail.w - 80, 30},
                          kCream, 1993,
                          engine::rendering::TextAlign::Left, 24.0f, 16.0f,
                          false);
        }
    }
    if (!data.workshop_status.empty()) {
        ui.TextFitted(data.workshop_status,
                      {panel.x + 40, panel.y + panel.h - 104, panel.w - 80,
                       30},
                      kGold, 1994, engine::rendering::TextAlign::Center,
                      22.0f, 15.0f, false);
    }
    ui.TextCentered("UP/DOWN SELECT   ENTER CRAFT   D DISMANTLE ACTIVE WEAPON   ESC CLOSE",
                    {panel.x, panel.y + panel.h - 58, panel.w, 28}, kMuted,
                    1995, 19.0f);
}

void GameUI::DrawPetSelect(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 190}, 1980);
    const Rect panel{300, 150, 1320, 780};
    ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1982);
    ui.Panel({panel.x + 16, panel.y + 16, panel.w - 32, panel.h - 32},
             kPanelDark, {112, 92, 63, 255}, 3.0f, 1984);
    ui.TextCentered("PET MENU", {panel.x, panel.y + 24, panel.w, 52},
                    Color::White(), 1990, 36.0f, true);
    ui.TextFitted("RECOVERED RGPetController PREFABS",
                  {panel.x + 40, panel.y + 80, panel.w - 80, 26},
                  kMuted, 1991, engine::rendering::TextAlign::Center,
                  18.0f, 13.0f, false);
    const int count = static_cast<int>(data.pet_names.size());
    if (count == 0) {
        ui.TextCentered("NO RECOVERED PET DEFINITIONS",
                        {panel.x, panel.y + 320, panel.w, 40}, kMuted, 1992,
                        28.0f);
    }
    for (int i = 0; i < count; ++i) {
        const int col = i % 4;
        const int row = i / 4;
        const Rect cell{panel.x + 42 + static_cast<float>(col) * 309.0f,
                        panel.y + 126 + static_cast<float>(row) * 210.0f,
                        290, 190};
        const bool selected = i == data.selected_pet;
        ui.Panel(cell, selected ? Color{58, 78, 111, 255}
                                : Color{31, 37, 51, 255},
                 selected ? kFrameLight : Color{67, 77, 97, 255},
                 selected ? 3.0f : 2.0f, 1990);
        if (i < static_cast<int>(data.pet_sprites.size()) &&
            data.pet_sprites[static_cast<std::size_t>(i)] >= 0) {
            ui.SceneSprite(data.pet_sprites[static_cast<std::size_t>(i)],
                           {cell.x + 90, cell.y + 18, 110, 110},
                           Color::White(), 1991);
        }
        ui.TextFitted(data.pet_names[static_cast<std::size_t>(i)],
                      {cell.x + 12, cell.y + 132, cell.w - 24, 44},
                      selected ? kGold : kCream, 1992,
                      engine::rendering::TextAlign::Center, 28.0f, 16.0f,
                      true);
    }
    ui.TextCentered("ARROWS SELECT   ENTER CHOOSE   ESC BACK",
                    {panel.x, panel.y + panel.h - 60, panel.w, 30}, kMuted,
                    1995, 20.0f);
}

void GameUI::DrawCharacterSelect(UIRenderer& ui, const GameUIData& data) const {
    if (data.characters == nullptr || data.scene == nullptr) return;
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 200}, 1980);
    const Rect panel{250, 140, 1420, 800};
    ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1982);
    ui.Panel({panel.x + 16, panel.y + 16, panel.w - 32, panel.h - 32},
             kPanelDark, {112, 92, 63, 255}, 3.0f, 1984);
    ui.TextCentered("CHARACTER STATION", {panel.x, panel.y + 20, panel.w, 56},
                    Color::White(), 1990, 36.0f, true);
    const auto& characters = data.characters->Characters();
    const int count = static_cast<int>(characters.size());
    if (count == 0) return;
    const int selected = std::clamp(character_select_index_, 0, count - 1);
    const int columns = 5;
    const float cell_w = 244.0f;
    const float cell_h = 180.0f;
    for (int i = 0; i < count; ++i) {
        const int col = i % columns;
        const int row = i / columns;
        const Rect cell{panel.x + 50 + static_cast<float>(col) * cell_w,
                        panel.y + 106 + static_cast<float>(row) * cell_h,
                        cell_w - 18, cell_h - 14};
        const bool active = i == selected;
        ui.Panel(cell, active ? Color{58, 78, 111, 255}
                              : Color{31, 37, 51, 255},
                 active ? kFrameLight : Color{67, 77, 97, 255},
                 active ? 3.0f : 2.0f, 1990);
        if (characters[static_cast<std::size_t>(i)].body_sprite_index >= 0) {
            ui.SceneSprite(characters[static_cast<std::size_t>(i)].body_sprite_index,
                           {cell.x + 10, cell.y + 12, 96, 96},
                           Color::White(), 1991);
        }
        ui.Text(std::string(characters[static_cast<std::size_t>(i)].id),
                {cell.x + 112, cell.y + 16}, active ? kGold : kCream,
                1992, true, false, 20.0f);
        char stats[64];
        std::snprintf(stats, sizeof(stats), "HP %d  AR %d",
                      characters[static_cast<std::size_t>(i)].max_hp,
                      characters[static_cast<std::size_t>(i)].max_armor);
        ui.Text(stats, {cell.x + 112, cell.y + 46}, kMuted, 1992,
                false, false, 18.0f);
        std::snprintf(stats, sizeof(stats), "EN %d",
                      characters[static_cast<std::size_t>(i)].max_energy);
        ui.Text(stats, {cell.x + 112, cell.y + 82}, kMuted, 1992,
                false, false, 18.0f);
    }
    const auto& selected_character =
        characters[static_cast<std::size_t>(selected)];
    char detail[160];
    std::snprintf(detail, sizeof(detail),
                  "%s  HP %d  ARMOR %d  ENERGY %d  SPEED %.1f",
                  selected_character.id.c_str(), selected_character.max_hp,
                  selected_character.max_armor, selected_character.max_energy,
                  selected_character.speed);
    ui.TextCentered(detail, {panel.x, panel.y + 700, panel.w, 30},
                    kCream, 1993, 18.0f);
    ui.TextCentered("ARROWS SELECT   ENTER CONFIRM   ESC BACK",
                    {panel.x, panel.y + 742, panel.w, 24}, kMuted, 1994,
                    15.0f);
}

void GameUI::DrawWeaponSelect(UIRenderer& ui, const GameUIData& data) const {
    if (data.weapons == nullptr || data.weapons->Count() == 0) return;
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 200}, 1980);
    const Rect panel{400, 190, 1120, 700};
    ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1982);
    ui.Panel({panel.x + 16, panel.y + 16, panel.w - 32, panel.h - 32},
             kPanelDark, {112, 92, 63, 255}, 3.0f, 1984);
    ui.TextCentered("WEAPON STATION", {panel.x, panel.y + 24, panel.w, 54},
                    Color::White(), 1990, 36.0f, true);
    const int count = static_cast<int>(data.weapons->Count());
    const int selected = std::clamp(weapon_select_index_, 0, count - 1);
    constexpr int visible_rows = 5;
    const int first = std::clamp(selected - visible_rows / 2, 0,
                                 std::max(0, count - visible_rows));
    constexpr float row_h = 88.0f;
    constexpr float row_gap = 10.0f;
    for (int slot = 0; slot < visible_rows; ++slot) {
        const int i = first + slot;
        if (i >= count) break;
        const Rect row{panel.x + 70,
                       panel.y + 106 + static_cast<float>(slot) *
                                          (row_h + row_gap),
                       panel.w - 140, row_h};
        const bool active = i == selected;
        ui.Panel(row, active ? Color{82, 68, 48, 255}
                             : Color{35, 40, 53, 255},
                 active ? kFrameLight : Color{68, 76, 93, 255},
                 active ? 3.0f : 2.0f, 1990);
        const auto& weapon =
            data.weapons->Definitions()[static_cast<std::size_t>(i)];
        if (weapon.visual_sprite_index >= 0) {
            ui.SceneSprite(weapon.visual_sprite_index,
                           {row.x + 16, row.y + 12, 100, 60}, Color::White(),
                           1991);
        }
        char info[192];
        std::snprintf(info, sizeof(info),
                      "%s   DMG %d   COST %d   CYCLE %.3f",
                      weapon.display_name.empty() ? weapon.id.c_str()
                                                   : weapon.display_name.c_str(),
                      weapon.damage, weapon.EnergyCost(),
                      weapon.attack_cycle_seconds);
        ui.TextFitted(info, {row.x + 132, row.y + 12, row.w - 150, 30},
                      active ? Color::White() : kCream, 1992,
                      engine::rendering::TextAlign::Left, 22.0f, 16.0f, true);
        ui.TextFitted(std::string("PROJECTILE ") + weapon.projectile.id,
                      {row.x + 132, row.y + 46, row.w - 150, 24},
                      kMuted, 1992, engine::rendering::TextAlign::Left,
                      16.0f, 12.0f, false);
    }
    char page[64];
    std::snprintf(page, sizeof(page), "%d / %d", selected + 1, count);
    ui.TextFitted(page, {panel.x + panel.w - 220, panel.y + 26, 180, 30},
                  kGold, 1993, engine::rendering::TextAlign::Right, 24.0f,
                  16.0f, true);
    ui.TextCentered("UP/DOWN SELECT   ENTER EQUIP ACTIVE SLOT   ESC BACK",
                    {panel.x, panel.y + 630, panel.w, 24}, kMuted, 1994,
                    15.0f);
}

void GameUI::Draw(UIRenderer& ui, const GameUIData& data) const {
    if (screen_ == UIScreen::MainMenu) {
        DrawMainMenu(ui, data);
        return;
    }
    DrawHud(ui, data);
    if (screen_ == UIScreen::Pause) DrawPause(ui, data);
    else if (screen_ == UIScreen::Settings) DrawSettings(ui, data);
    else if (screen_ == UIScreen::CharacterSelect) DrawCharacterSelect(ui, data);
    else if (screen_ == UIScreen::WeaponSelect) DrawWeaponSelect(ui, data);
    else if (screen_ == UIScreen::SaveLoad) DrawSaveLoad(ui, data);
    else if (screen_ == UIScreen::Trader) DrawTrader(ui, data);
    else if (screen_ == UIScreen::PetSelect) DrawPetSelect(ui, data);
    else if (screen_ == UIScreen::Workshop) DrawWorkshop(ui, data);
}

} // namespace soulknight::game::ui
