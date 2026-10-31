#include "GameUI.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

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

const std::array<const char*, 3> kPauseItems{"HOME", "RESUME", "SETTINGS"};
const std::array<const char*, 4> kMainItems{"NEW GAME", "CONTINUE", "SETTINGS", "QUIT"};
const std::array<const char*, 4> kSettingsItems{
    "FULLSCREEN", "DEBUG OVERLAY", "RESET SAVE", "BACK"};

void DrawFrame(UIRenderer& ui, const Rect& rect, Color fill, int32_t order) {
    ui.Rect({rect.x + 3, rect.y + 3, rect.w - 6, rect.h - 6}, kFrameGold, order);
    ui.Rect({rect.x + 6, rect.y + 6, rect.w - 12, rect.h - 12}, kFrameLight,
            order + 1);
    ui.Rect({rect.x + 9, rect.y + 9, rect.w - 18, rect.h - 18}, fill,
            order + 2);
    ui.Rect({rect.x + 9, rect.y + 9, rect.w - 18, 2}, kFrameBrown, order + 3);
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

void GameUI::CloseSelection() {
    screen_ = UIScreen::Gameplay;
    action_ = UIAction::None;
}

void GameUI::Back() {
    if (screen_ == UIScreen::CharacterSelect ||
        screen_ == UIScreen::WeaponSelect) {
        CloseSelection();
        return;
    }
    if (screen_ == UIScreen::Settings) {
        screen_ = settings_parent_;
        if (screen_ == UIScreen::Pause) pause_index_ = 2;
    } else if (screen_ == UIScreen::Pause) {
        Resume();
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
        main_index_ = (main_index_ + delta + 4) % 4;
    } else if (screen_ == UIScreen::Settings) {
        settings_index_ = (settings_index_ + delta + 4) % 4;
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
        else action_ = UIAction::Quit;
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
    if (screen_ == UIScreen::Settings) {
        if (settings_index_ == 0) action_ = UIAction::ToggleFullscreen;
        else if (settings_index_ == 1) action_ = UIAction::ToggleDebugOverlay;
        else if (settings_index_ == 2) action_ = UIAction::ResetSave;
        else action_ = UIAction::Back;
    }
}

UIAction GameUI::TakeAction() {
    const UIAction result = action_;
    action_ = UIAction::None;
    return result;
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
        : "ui_btn1";
    const char* icon = label == "HOME" ? "ui_98"
        : label == "RESUME" ? "ui_92"
        : label == "SETTINGS" ? "ui_93"
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
    const Rect text_rect = icon != nullptr
        ? Rect{rect.x + 48, rect.y, rect.w - 48, rect.h} : rect;
    const bool light_frame = label == "NEW GAME" || label == "RESUME";
    const Color text_color = !enabled ? Color{142, 130, 112, 255}
        : light_frame ? Color{43, 29, 18, 255}
        : (selected ? Color::White() : kCream);
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
    // Top-left player panel. This mirrors the supplied reference layout:
    // heart/HP, shield/armor, crystal/energy in one warm framed plate.
    const Rect panel{18, 16, 352, 132};
    DrawFrame(ui, panel, {31, 26, 28, 255}, 1900);
    ui.Rect({28, 25, 332, 112}, {24, 21, 24, 255}, 1904);

    const float row_y[3] = {27.0f, 63.0f, 99.0f};
    const int values[3] = {data.health->hp, data.health->armor,
                           data.energy->energy};
    const int maxima[3] = {data.health->max_hp, data.health->max_armor,
                           data.energy->max_energy};
    const Color colors[3] = {kHealth, kArmor, kEnergy};
    for (int i = 0; i < 3; ++i) {
        DrawIcon(ui, data, i, {31, row_y[i] + 1, 30, 30}, 1906);
        const Rect bar{68, row_y[i] + 4, 282, 27};
        const float fraction = maxima[i] > 0
            ? static_cast<float>(values[i]) / static_cast<float>(maxima[i])
            : 0.0f;
        DrawBar(ui, bar, fraction, colors[i], 1906);
        char text[48];
        std::snprintf(text, sizeof(text), "%d/%d", values[i], maxima[i]);
        ui.TextCentered(text, bar, Color::White(), 1912, 20.0f, true);
    }

    // Desktop pause control and currency strip (native keyboard binding, real
    // recovered icons; no mobile buttons or joysticks).
    const Rect pause_rect{1790, 18, 86, 76};
    ui.Panel(pause_rect, {42, 29, 22, 235}, kFrameBrown, 2.0f, 1900);
    DrawAsset(ui, data, "ui_94", {1820, 34, 26, 26}, Color::White(), 1905);
    DrawAsset(ui, data, "ui_5", {1640, 32, 26, 30}, Color::White(), 1905);
    ui.Text("0", {1672, 28}, kCream, 1906, true, false, 28.0f);

    // Two-slot weapon strip. Weapon icons are recovered prefab sprites; empty
    // equipment explicitly renders an empty slot with no weapon.
    if (data.weapons != nullptr && data.inventory != nullptr) {
        for (std::size_t slot = 0; slot < 2; ++slot) {
            const Rect slot_rect{122.0f + static_cast<float>(slot) * 76.0f,
                                 158.0f, 70.0f, 76.0f};
            const bool active =
                data.inventory->ActiveSlot() == slot;
            const bool occupied = data.inventory->SlotOccupied(slot);
            ui.Panel(slot_rect, {25, 29, 39, 232},
                     active ? kFrameLight : kFrameBrown,
                     active ? 3.0f : 2.0f, 1900);
            if (occupied) {
                const std::size_t index = data.inventory->Slot(slot);
                if (index < data.weapons->Count()) {
                    const auto& weapon = data.weapons->Definitions()[index];
                    if (weapon.visual_sprite_index >= 0) {
                        ui.SceneSprite(weapon.visual_sprite_index,
                                       {slot_rect.x + 6, slot_rect.y + 6,
                                        58, 48},
                                       Color::White(), 1904);
                    }
                }
            } else {
                ui.TextCentered("-", {slot_rect.x, slot_rect.y + 18,
                                       slot_rect.w, 30},
                                {130, 128, 122, 255}, 1904, 24.0f);
            }
            ui.TextCentered(slot == 0 ? "1" : "2",
                            {slot_rect.x, slot_rect.y + 54, slot_rect.w, 18},
                            active ? kGold : kMuted, 1905, 14.0f);
        }
    }

    if (!data.pickup_nearby && data.interaction_nearby) {
        const float x = std::clamp(data.interaction_screen.x + 46.0f,
                                   16.0f, 1920.0f - 256.0f);
        const float y = std::clamp(data.interaction_screen.y - 164.0f,
                                   16.0f, 1080.0f - 120.0f);
        const Rect prompt{x, y, 240, 78};
        ui.Panel(prompt, {32, 27, 25, 242}, kFrameGold, 2.0f, 1920);
        ui.TextCentered("E  INTERACT", {prompt.x, prompt.y + 8,
                                        prompt.w, 30},
                        Color::White(), 1921, 22.0f, true);
        ui.TextCentered(data.interaction_label,
                        {prompt.x, prompt.y + 40, prompt.w, 26},
                        kGold, 1921, 16.0f);
    }
    if (data.pickup_nearby) {
        const float x = std::clamp(data.pickup_screen.x + 46.0f,
                                   16.0f, 1920.0f - 236.0f);
        const float y = std::clamp(data.pickup_screen.y - 164.0f,
                                   16.0f, 1080.0f - 120.0f);
        const Rect prompt{x, y, 220, 78};
        ui.Panel(prompt, {32, 27, 25, 242}, kFrameGold, 2.0f, 1920);
        ui.TextCentered("E  PICK UP", {prompt.x, prompt.y + 8,
                                       prompt.w, 30},
                        Color::White(), 1921, 22.0f, true);
        if (!data.pickup_name.empty()) {
            ui.TextCentered(data.pickup_name, {prompt.x, prompt.y + 40,
                                               prompt.w, 26},
                            kGold, 1921, 16.0f);
        }
    }
}

void GameUI::DrawPause(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 176}, 1980);
    const Rect panel{420, 258, 1080, 564};
    ui.Rect({panel.x + 8, panel.y + 8, panel.w, panel.h},
            {3, 4, 8, 180}, 1981);
    ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1982);
    ui.Rect({panel.x + 10, panel.y + 10, panel.w - 20, panel.h - 20},
            {40, 48, 69, 255}, 1983);
    ui.Panel({panel.x + 16, panel.y + 16, panel.w - 32, panel.h - 32},
             kPanelDark, {112, 92, 63, 255}, 3.0f, 1984);

    // PAUSE ribbon
    const Rect ribbon{650, 236, 620, 86};
    ui.Rect({ribbon.x + 8, ribbon.y + 6, ribbon.w - 16, ribbon.h},
            {65, 19, 24, 255}, 1986);
    ui.Panel(ribbon, {160, 35, 40, 255}, {94, 22, 28, 255}, 4.0f, 1987);
    ui.TextCentered("PAUSE", ribbon, Color::White(), 1988, 52.0f, true);

    // Character portrait
    const Rect portrait{452, 360, 116, 116};
    ui.Panel(portrait, {47, 67, 101, 255}, kFrameGold, 4.0f, 1990);
    if (data.characters != nullptr && data.characters->Count() > 0) {
        const auto& character = data.characters->Active();
        if (character.body_sprite_index >= 0) {
            ui.SceneSprite(character.body_sprite_index, {460, 366, 100, 100},
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
    for (int i = 0; i < 3; ++i) {
        DrawButton(ui, data, buttons[i], kPauseItems[static_cast<std::size_t>(i)],
                   pause_index_ == i, true, 26.0f);
    }
    ui.TextCentered("P / ESC  RESUME    ENTER  SELECT", {420, 760, 1080, 24},
                    kMuted, 1995, 16.0f);
}

void GameUI::DrawSettings(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 200}, 1980);
    const Rect panel{500, 230, 920, 620};
    ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1982);
    ui.Rect({panel.x + 10, panel.y + 10, panel.w - 20, panel.h - 20},
            {38, 46, 66, 255}, 1983);
    ui.Panel({panel.x + 16, panel.y + 16, panel.w - 32, panel.h - 32},
             kPanelDark, {112, 92, 63, 255}, 3.0f, 1984);
    ui.TextCentered("SETTINGS", {panel.x, panel.y + 24, panel.w, 56},
                    Color::White(), 1990, 40.0f, true);
    const char* values[4] = {
        data.fullscreen ? "ON" : "OFF",
        data.debug_overlay ? "ON" : "OFF",
        "CONFIRM",
        ""
    };
    for (int i = 0; i < 4; ++i) {
        const Rect row{panel.x + 70,
                       panel.y + 120 + static_cast<float>(i) * 92.0f,
                       panel.w - 140, 70};
        const bool selected = settings_index_ == i;
        ui.Panel(row, selected ? Color{74, 65, 53, 255}
                              : Color{37, 41, 55, 255},
                 selected ? kFrameLight : Color{68, 76, 93, 255},
                 2.0f, 1990);
        ui.TextAligned(kSettingsItems[static_cast<std::size_t>(i)],
                       {row.x + 20, row.y, row.w - 180, row.h},
                       selected ? Color::White() : kCream, 1991,
                       engine::rendering::TextAlign::Left, 22.0f, true);
        if (i < 2) {
            ui.TextAligned(values[i], {row.x, row.y, row.w - 25, row.h},
                           selected ? kGold : kMuted, 1992,
                           engine::rendering::TextAlign::Right, 22.0f);
        }
    }
    ui.TextCentered("ESC  BACK       ENTER  TOGGLE", {panel.x, 780, panel.w, 24},
                    kMuted, 1995, 16.0f);
}

void GameUI::DrawMainMenu(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {7, 8, 12, 255}, 1980);
    // Native atmospheric backdrop; reference layout is reproduced, not the
    // copyrighted background artwork.
    ui.Rect({0, 0, 1920, 1080}, {11, 15, 23, 255}, 1981);
    ui.Rect({720, 0, 1200, 1080}, {15, 34, 42, 255}, 1982);
    ui.Rect({760, 0, 1120, 1080}, {17, 45, 51, 255}, 1983);
    ui.Rect({800, 0, 1080, 1080}, {20, 55, 59, 255}, 1984);
    // Red energy sweep (native pixel bands, no gradient).
    ui.Rect({900, 230, 520, 16}, {142, 26, 30, 255}, 1986);
    ui.Rect({860, 246, 560, 12}, {185, 35, 32, 255}, 1987);
    ui.Rect({980, 200, 260, 14}, {103, 19, 24, 255}, 1986);
    ui.Rect({1100, 180, 180, 8}, {226, 59, 38, 255}, 1988);

    if (data.characters != nullptr && data.characters->Count() > 0) {
        const auto& character = data.characters->Active();
        if (character.body_sprite_index >= 0) {
            ui.SceneSprite(character.body_sprite_index, {1320, 360, 180, 180},
                           Color::White(), 1990);
        }
    }
    bool drew_logo = false;
    if (data.ui_assets != nullptr) {
        if (const auto* logo = data.ui_assets->Find("title_en0");
            logo != nullptr && logo->source_pixels.x > 0.0f &&
            logo->source_pixels.y > 0.0f) {
            const float width = 620.0f;
            const float height = width * logo->source_pixels.y /
                                 logo->source_pixels.x;
            ui.Image(logo->texture_id, logo->uv,
                     {52, 76, width, height}, Color::White(), 1991);
            drew_logo = true;
        }
    }
    if (!drew_logo) {
        ui.Text("LITEKNIGHT", {48, 84}, Color::White(), 1991, true, true, 72.0f);
    }
    ui.Text("NATIVE LINUX DEVROOM", {54, 232}, {133, 201, 204, 255}, 1992,
            false, false, 20.0f);
    ui.Rect({54, 258, 700, 4}, {190, 60, 44, 255}, 1992);

    const Rect buttons[3] = {
        {120, 690, 620, 92},
        {120, 800, 620, 92},
        {120, 910, 620, 92}
    };
    for (int i = 0; i < 3; ++i) {
        DrawButton(ui, data, buttons[i], kMainItems[static_cast<std::size_t>(i)],
                   main_index_ == i, i != 1 || data.save_exists, 30.0f);
    }
    DrawButton(ui, data, {120, 1020, 300, 48}, kMainItems[3], main_index_ == 3,
               true, 18.0f);
    ui.Text("LOCAL ONLY  //  NO SOCIAL FEATURES", {820, 1010},
            {118, 145, 148, 255}, 1995, false, false, 16.0f);
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
    for (int i = 0; i < count; ++i) {
        const Rect row{panel.x + 70, panel.y + 110 + static_cast<float>(i) * 108,
                       panel.w - 140, 92};
        const bool active = i == selected;
        ui.Panel(row, active ? Color{82, 68, 48, 255}
                             : Color{35, 40, 53, 255},
                 active ? kFrameLight : Color{68, 76, 93, 255},
                 active ? 3.0f : 2.0f, 1990);
        const auto& weapon = data.weapons->Definitions()[static_cast<std::size_t>(i)];
        if (weapon.visual_sprite_index >= 0) {
            ui.SceneSprite(weapon.visual_sprite_index,
                           {row.x + 16, row.y + 12, 104, 64}, Color::White(),
                           1991);
        }
        char info[160];
        std::snprintf(info, sizeof(info),
                      "%s   DMG %d   COST %d   CYCLE %.3f",
                      weapon.id.c_str(), weapon.damage, weapon.consume,
                      weapon.attack_cycle_seconds);
        ui.Text(info, {row.x + 142, row.y + 22},
                active ? Color::White() : kCream, 1992, true, false, 20.0f);
        ui.Text(std::string("PROJECTILE ") + weapon.projectile.id,
                {row.x + 142, row.y + 50}, kMuted, 1992, false, false, 15.0f);
    }
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
}

} // namespace soulknight::game::ui
