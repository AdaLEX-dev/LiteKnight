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
const std::array<const char*, 6> kSettingsItems{
    "AIM MODE", "FULLSCREEN", "DEBUG OVERLAY", "UI DESIGN MODE",
    "RESET SAVE", "BACK"};

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

void GameUI::OpenMap() {
    if (screen_ == UIScreen::MainMenu) return;
    screen_ = UIScreen::Map;
    action_ = UIAction::None;
}

void GameUI::CloseMap() {
    if (screen_ == UIScreen::Map) screen_ = UIScreen::Gameplay;
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
    } else if (screen_ == UIScreen::Map) {
        CloseMap();
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
        main_index_ = (main_index_ + delta + 5) % 5;
    } else if (screen_ == UIScreen::Settings) {
        settings_index_ = (settings_index_ + delta + 6) % 6;
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
        else action_ = UIAction::OpenDevroom;
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

bool GameUI::HandlePointerClick(const engine::Vec2& logical) {
    if (screen_ != UIScreen::MainMenu) return false;
    const Rect droom{54, 310, 196, 54};
    if (logical.x >= droom.x && logical.x <= droom.x + droom.w &&
        logical.y >= droom.y && logical.y <= droom.y + droom.h) {
        main_index_ = 4;
        Confirm();
        return true;
    }
    const Rect buttons[4] = {
        {120, 690, 620, 92}, {120, 800, 620, 92},
        {120, 910, 620, 92}, {120, 1020, 300, 48}};
    for (int i = 0; i < 4; ++i) {
        const Rect& r = buttons[i];
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
    const auto element = [&](const char* id) {
        static const HudLayout fallback;
        return data.layout != nullptr ? data.layout->RectFor(id)
                                      : fallback.RectFor(id);
    };

    // Status rows are authored in absolute logical 1920x1080 coordinates. The
    // viewport transform is applied by UIRenderer, so fullscreen/letterboxing
    // cannot move them relative to the game frame.
    const struct {
        const char* id;
        int icon;
        int value;
        int maximum;
        Color color;
    } rows[] = {
        {"hp", 0, data.health->hp, data.health->max_hp, kHealth},
        {"armor", 1, data.health->armor, data.health->max_armor, kArmor},
        {"energy", 2, data.energy->energy, data.energy->max_energy, kEnergy},
    };
    for (const auto& row : rows) {
        const Rect r = element(row.id);
        ui.Panel(r, {23, 21, 28, 220}, {102, 76, 46, 255}, 1.0f, 1900);
        DrawIcon(ui, data, row.icon, {r.x + 3, r.y + 4, 30, 30}, 1904);
        const Rect bar{r.x + 42, r.y + 3, r.w - 46, r.h - 6};
        DrawBar(ui, bar, row.maximum > 0
                             ? static_cast<float>(row.value) /
                                   static_cast<float>(row.maximum)
                             : 0.0f,
                row.color, 1904);
        char text[48];
        std::snprintf(text, sizeof(text), "%d/%d", row.value, row.maximum);
        ui.TextCentered(text, bar, Color::White(), 1910, 28.0f, true);
    }

    // Coin and pause stay together at the absolute upper-right edge.
    const Rect coin = element("coin");
    DrawAsset(ui, data, "ui_5", {coin.x, coin.y + 7, 30, 32},
              Color::White(), 1905);
    char coins[32];
    std::snprintf(coins, sizeof(coins), "%d", data.coins);
    ui.TextFitted(coins, {coin.x + 36, coin.y, coin.w - 36, coin.h},
                  kCream, 1906, engine::rendering::TextAlign::Left,
                  28.0f, 20.0f, true);

    const Rect pause = element("pause");
    ui.Panel(pause, {42, 29, 22, 235}, kFrameBrown, 2.0f, 1900);
    DrawAsset(ui, data, "ui_94",
              {pause.x + (pause.w - 30) * 0.5f, pause.y + 20, 30, 30},
              Color::White(), 1905);

    DrawMinimap(ui, data, element("minimap"), false);

    const Rect stage = element("stage");
    ui.TextFitted(std::string("ROOM ") + (data.scene != nullptr
                      ? data.scene->id : std::string("?")),
                  stage, kMuted, 1903,
                  engine::rendering::TextAlign::Left, 28.0f, 18.0f, true);

    // Character power, deliberately larger than a weapon slot.
    const Rect power = element("ability");
    ui.Panel(power, {32, 29, 40, 235},
             data.ability_state == 1 ? kFrameLight : kFrameBrown,
             2.0f, 1900);
    DrawAsset(ui, data, "ui_skill01",
              {power.x + 8, power.y + 8, power.w - 16, power.h - 16},
              data.ability_state == 0 ? Color{90, 90, 90, 255}
                                     : Color::White(), 1905);
    if (data.ability_cooldown > 0.0f) {
        const float fill = std::clamp(data.ability_cooldown, 0.0f, 1.0f);
        ui.Rect({power.x + 8, power.y + 8 + (power.h - 16) * (1.0f - fill),
                 power.w - 16, (power.h - 16) * fill},
                {5, 8, 16, 185}, 1906);
    }
    ui.TextCentered("Q", {power.x, power.y + power.h - 28,
                          power.w, 26}, kCream, 1907, 20.0f);
    // Recovered energy crystal + charge fill. The fill is driven by the real
    // ability state (ready/active/cooldown), not a decorative timer.
    const Rect charge{power.x + 10, power.y + power.h + 6,
                      power.w - 20, 14};
    DrawAsset(ui, data, "ui_67", {charge.x, charge.y - 2, 18, 18},
              Color::White(), 1906);
    ui.Rect({charge.x + 22, charge.y + 2, charge.w - 22, 10},
            {18, 20, 30, 230}, 1900);
    float charge_fraction = 0.0f;
    if (data.ability_state == 1 || data.ability_state == 2) {
        charge_fraction = 1.0f;
    } else if (data.ability_state == 3) {
        charge_fraction = 1.0f - std::clamp(data.ability_cooldown, 0.0f, 1.0f);
    }
    if (charge_fraction > 0.0f) {
        ui.Rect({charge.x + 23, charge.y + 3,
                 (charge.w - 24) * charge_fraction, 8},
                data.ability_state == 2 ? kGold : kEnergy, 1901);
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
            ui.Panel(slot_rect, {25, 29, 39, 232},
                     active ? kFrameLight : kFrameBrown,
                     active ? 3.0f : 2.0f, 1900);
            if (occupied) {
                const std::size_t index = data.inventory->Slot(slot);
                if (index < data.weapons->Count()) {
                    const auto& weapon = data.weapons->Definitions()[index];
                    if (weapon.visual_sprite_index >= 0) {
                        ui.SceneSprite(weapon.visual_sprite_index,
                                       {slot_rect.x + 4, slot_rect.y + 4,
                                        slot_rect.w - 8, slot_rect.h - 24},
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
    if (data.chest_nearby) {
        ui.Panel(prompt, {26, 25, 30, 246}, kFrameGold, 2.0f, 1920);
        ui.TextFitted("E  OPEN CHEST",
                      {prompt.x + 14, prompt.y + 8, prompt.w - 28, 34},
                      Color::White(), 1921,
                      engine::rendering::TextAlign::Center, 30.0f, 20.0f, true);
        ui.TextFitted(data.chest_label,
                      {prompt.x + 14, prompt.y + 50, prompt.w - 28, 30},
                      kGold, 1921, engine::rendering::TextAlign::Center,
                      28.0f, 18.0f, false);
    } else if (data.pickup_nearby) {
        ui.Panel(prompt, {26, 25, 30, 246}, kFrameGold, 2.0f, 1920);
        ui.TextFitted("E  PICK UP", {prompt.x + 14, prompt.y + 4,
                                     prompt.w - 28, 30},
                      Color::White(), 1921,
                      engine::rendering::TextAlign::Center, 28.0f, 20.0f, true);
        if (!data.pickup_name.empty()) {
            ui.TextFitted(data.pickup_name,
                          {prompt.x + 14, prompt.y + 34,
                           prompt.w - 28, 28},
                          kGold, 1921, engine::rendering::TextAlign::Center,
                          28.0f, 18.0f, true);
        }
        char stats[96];
        std::snprintf(stats, sizeof(stats), "DMG %d   ENERGY %d",
                      data.pickup_damage, data.pickup_energy_cost);
        ui.TextFitted(stats, {prompt.x + 14, prompt.y + 66,
                              prompt.w - 28, 30},
                      kMuted, 1921, engine::rendering::TextAlign::Center,
                      26.0f, 16.0f, false);
    } else if (data.interaction_nearby) {
        ui.Panel(prompt, {32, 27, 25, 242}, kFrameGold, 2.0f, 1920);
        ui.TextFitted("E  INTERACT", {prompt.x + 14, prompt.y + 8,
                                      prompt.w - 28, 32},
                      Color::White(), 1921,
                      engine::rendering::TextAlign::Center, 28.0f, 20.0f, true);
        ui.TextFitted(data.interaction_label,
                      {prompt.x + 14, prompt.y + 46,
                       prompt.w - 28, 30},
                      kGold, 1921, engine::rendering::TextAlign::Center,
                      28.0f, 18.0f, false);
    }

    if (data.ui_design_mode) DrawDesignOverlay(ui, data);
}

void GameUI::DrawMinimap(UIRenderer& ui, const GameUIData& data,
                         const engine::Rect& rect, bool expanded) const {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;
    const int32_t base = expanded ? 1984 : 1900;
    ui.Panel(rect, {18, 22, 32, 235}, {91, 114, 148, 255},
             expanded ? 3.0f : 2.0f, base);
    if (!expanded) {
        ui.TextFitted("MAP", {rect.x + 10, rect.y + 4, rect.w - 20, 24},
                      kMuted, base + 5, engine::rendering::TextAlign::Left,
                      28.0f, 16.0f, false);
    }
    if (data.map_rooms.empty()) {
        ui.TextFitted("NO MAP", {rect.x + 10, rect.y + rect.h * 0.5f - 14,
                                 rect.w - 20, 28},
                      kMuted, base + 5, engine::rendering::TextAlign::Center,
                      28.0f, 18.0f, false);
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
    const float title_h = expanded ? 0.0f : 28.0f;
    const float inner_pad = expanded ? 46.0f : 20.0f;
    const float available_w = rect.w - inner_pad * 2.0f;
    const float available_h = rect.h - inner_pad * 2.0f - title_h;
    const float cell = std::max(14.0f, std::min(
        (available_w - static_cast<float>(cols - 1) * 26.0f) /
            static_cast<float>(cols),
        (available_h - static_cast<float>(rows - 1) * 26.0f) /
            static_cast<float>(rows)));
    const float gap = expanded ? 26.0f : 12.0f;
    const float grid_w = static_cast<float>(cols) * cell +
                         static_cast<float>(cols - 1) * gap;
    const float grid_h = static_cast<float>(rows) * cell +
                         static_cast<float>(rows - 1) * gap;
    const float origin_x = rect.x + (rect.w - grid_w) * 0.5f;
    const float origin_y = rect.y + title_h +
                           (rect.h - title_h - grid_h) * 0.5f;
    const auto center = [&](const MapRoomView& room) {
        return engine::Vec2{
            origin_x + static_cast<float>(room.x - min_x) * (cell + gap) +
                cell * 0.5f,
            origin_y + static_cast<float>(room.y - min_y) * (cell + gap) +
                cell * 0.5f};
    };
    for (const MapLinkView& link : data.map_links) {
        const MapRoomView from{std::string{}, link.from_x, link.from_y};
        const MapRoomView to{std::string{}, link.to_x, link.to_y};
        const engine::Vec2 a = center(from);
        const engine::Vec2 b = center(to);
        if (std::abs(a.y - b.y) < 1.0f) {
            ui.Rect({std::min(a.x, b.x), a.y - 2.0f,
                     std::abs(a.x - b.x), 4.0f},
                    {67, 91, 126, 255}, base + 1);
        } else if (std::abs(a.x - b.x) < 1.0f) {
            ui.Rect({a.x - 2.0f, std::min(a.y, b.y),
                     4.0f, std::abs(a.y - b.y)},
                    {67, 91, 126, 255}, base + 1);
        } else {
            ui.Rect({std::min(a.x, b.x), a.y - 2.0f,
                     std::abs(a.x - b.x), 4.0f},
                    {67, 91, 126, 255}, base + 1);
            ui.Rect({b.x - 2.0f, std::min(a.y, b.y),
                     4.0f, std::abs(a.y - b.y)},
                    {67, 91, 126, 255}, base + 1);
        }
    }
    for (const MapRoomView& room : data.map_rooms) {
        const engine::Vec2 c = center(room);
        const Rect cell_rect{c.x - cell * 0.5f, c.y - cell * 0.5f,
                             cell, cell};
        const Color fill = room.current ? Color{82, 68, 48, 255}
            : room.visited ? Color{31, 51, 78, 255}
                           : Color{22, 27, 36, 240};
        const Color border = room.current ? kFrameLight
            : room.visited ? Color{91, 132, 180, 255}
                           : Color{47, 56, 70, 255};
        ui.Panel(cell_rect, fill, border, room.current ? 3.0f : 2.0f,
                 base + 4);
        std::string icon = room.devroom ? "D"
            : (room.id == "hero_room" ? "H" :
               (room.id.rfind("r1_", 0) == 0 ? room.id.substr(3, 1) : "."));
        ui.TextFitted(icon, {cell_rect.x, cell_rect.y + 2,
                             cell_rect.w, cell_rect.h - 4},
                      room.devroom ? Color::White() : kCream, base + 5,
                      engine::rendering::TextAlign::Center,
                      expanded ? 28.0f : 20.0f, 14.0f, false);
        if (expanded && !room.id.empty()) {
            ui.TextFitted(room.id,
                          {cell_rect.x + 4.0f,
                           cell_rect.y + cell_rect.h - 26.0f,
                           cell_rect.w - 8.0f, 22.0f},
                          kMuted, base + 6,
                          engine::rendering::TextAlign::Center,
                          18.0f, 11.0f, false);
        }
        if (room.current) {
            ui.Rect({cell_rect.x + cell_rect.w - 10.0f,
                     cell_rect.y + 6.0f, 5.0f, 5.0f},
                    Color::White(), base + 7);
        }
    }
}

void GameUI::DrawMap(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {4, 5, 9, 190}, 1978);
    const Rect panel = data.layout != nullptr
        ? data.layout->RectFor("map_panel")
        : Rect{520, 170, 880, 740};
    ui.Panel(panel, kPanelNavy, {76, 87, 112, 255}, 4.0f, 1980);
    ui.Panel({panel.x + 16, panel.y + 16, panel.w - 32, panel.h - 32},
             kPanelDark, {112, 92, 63, 255}, 3.0f, 1982);
    ui.TextFitted("MAP", {panel.x + 20, panel.y + 18, panel.w - 40, 44},
                  Color::White(), 1990,
                  engine::rendering::TextAlign::Center, 40.0f, 28.0f, true);
    const Rect map_rect{panel.x + 36, panel.y + 74, panel.w - 72, panel.h - 128};
    DrawMinimap(ui, data, map_rect, true);
    ui.TextFitted("M / ESC  CLOSE       VISITED ROOMS ARE BLUE",
                  {panel.x + 20, panel.y + panel.h - 44, panel.w - 40, 28},
                  kMuted, 1992, engine::rendering::TextAlign::Center,
                  24.0f, 16.0f, false);
}

void GameUI::DrawDesignOverlay(UIRenderer& ui, const GameUIData& data) const {
    if (data.layout == nullptr) return;
    ui.Rect({0, 0, 1920, 1080}, {3, 5, 10, 80}, 1960);
    ui.TextFitted("UI DESIGN MODE  -  DRAG HUD ELEMENTS",
                  {500, 18, 920, 40}, Color::White(), 1961,
                  engine::rendering::TextAlign::Center, 32.0f, 20.0f, true);
    for (const HudElement& element : data.layout->Elements()) {
        const bool selected = element.id == data.ui_design_selected;
        ui.Panel(element.rect,
                 {0, 0, 0, 0},
                 selected ? kFrameLight : Color{70, 170, 230, 190},
                 selected ? 3.0f : 1.0f, 1962);
        ui.TextFitted(element.id,
                      {element.rect.x + 2, element.rect.y + 2,
                       std::max(30.0f, element.rect.w - 4), 20},
                      selected ? kFrameLight : Color{120, 196, 235, 255},
                      1963, engine::rendering::TextAlign::Left,
                      16.0f, 12.0f, true);
    }
    ui.TextFitted("ESC SAVE/EXIT   R RESET LAYOUT   S SAVE   F4 DEBUG",
                  {360, 58, 1200, 32}, kCream, 1964,
                  engine::rendering::TextAlign::Center, 26.0f, 16.0f, true);
    (void)data.design_mouse;
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
    ui.TextCentered("ESC  RESUME    ENTER  SELECT", {420, 760, 1080, 24},
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
    const char* values[6] = {
        data.auto_aim ? "AUTO AIM" : "MANUAL",
        data.fullscreen ? "ON" : "OFF",
        data.debug_overlay ? "ON" : "OFF",
        "OPEN",
        "CONFIRM",
        ""
    };
    for (int i = 0; i < 6; ++i) {
        const Rect row{panel.x + 70,
                       panel.y + 110 + static_cast<float>(i) * 72.0f,
                       panel.w - 140, 60};
        const bool selected = settings_index_ == i;
        ui.Panel(row, selected ? Color{74, 65, 53, 255}
                              : Color{37, 41, 55, 255},
                 selected ? kFrameLight : Color{68, 76, 93, 255},
                 2.0f, 1990);
        ui.TextFitted(kSettingsItems[static_cast<std::size_t>(i)],
                      {row.x + 20, row.y, row.w - 220, row.h},
                      selected ? Color::White() : kCream, 1991,
                      engine::rendering::TextAlign::Left, 28.0f, 18.0f, true);
        if (i < 4) {
            ui.TextFitted(values[i], {row.x, row.y, row.w - 35, row.h},
                          selected ? kGold : kMuted, 1992,
                          engine::rendering::TextAlign::Right, 28.0f,
                          18.0f, false);
        }
    }
    ui.TextFitted("ESC  BACK       ENTER  SELECT",
                  {panel.x, panel.y + panel.h - 54, panel.w, 28},
                  kMuted, 1995, engine::rendering::TextAlign::Center,
                  24.0f, 16.0f, false);
}

void GameUI::DrawMainMenu(UIRenderer& ui, const GameUIData& data) const {
    ui.Rect({0, 0, 1920, 1080}, {7, 8, 12, 255}, 1980);
    // Native atmospheric backdrop. Actual recovered title/button art is used
    // where it exists; the wordmark below is deliberately the project title
    // "LIGHT KNIGHT", not a copied reference logo.
    ui.Rect({0, 0, 1920, 1080}, {11, 15, 23, 255}, 1981);
    ui.Rect({720, 0, 1200, 1080}, {15, 34, 42, 255}, 1982);
    ui.Rect({760, 0, 1120, 1080}, {17, 45, 51, 255}, 1983);
    ui.Rect({800, 0, 1080, 1080}, {20, 55, 59, 255}, 1984);
    ui.Rect({900, 230, 520, 16}, {142, 26, 30, 255}, 1986);
    ui.Rect({860, 246, 560, 12}, {185, 35, 32, 255}, 1987);
    ui.Rect({980, 200, 260, 14}, {103, 19, 24, 255}, 1986);
    ui.Rect({1100, 180, 180, 8}, {226, 59, 38, 255}, 1988);
    // A subtle vertical seam and corner pixels keep the composition cohesive
    // without clipping the left panel.
    ui.Rect({52, 74, 4, 560}, {140, 43, 39, 255}, 1989);
    ui.Rect({52, 74, 630, 3}, {220, 72, 55, 255}, 1989);

    if (data.characters != nullptr && data.characters->Count() > 0) {
        const auto& character = data.characters->Active();
        if (character.body_sprite_index >= 0) {
            ui.SceneSprite(character.body_sprite_index,
                           {1330, 360, 180, 180}, Color::White(), 1990);
        }
    }
    ui.TextFitted("LIGHT", {58, 92, 620, 86}, kCream, 1991,
                  engine::rendering::TextAlign::Left, 84.0f, 56.0f, true);
    ui.TextFitted("KNIGHT", {58, 174, 620, 86}, kGold, 1991,
                  engine::rendering::TextAlign::Left, 84.0f, 56.0f, true);
    ui.TextFitted("NATIVE LINUX DEVROOM", {62, 284, 620, 28},
                  {133, 201, 204, 255}, 1992,
                  engine::rendering::TextAlign::Left, 28.0f, 18.0f, false);
    ui.Rect({62, 318, 620, 4}, {190, 60, 44, 255}, 1992);

    // Developer utility: blue, opaque, deliberately quieter than the player
    // menu, and clickable with the same pointer transform as gameplay aiming.
    const Rect droom{62, 348, 196, 54};
    const bool droom_selected = main_index_ == 4;
    ui.Panel(droom, {21, 57, 112, 255},
             droom_selected ? kFrameLight : Color{57, 117, 190, 255},
             droom_selected ? 3.0f : 2.0f, 1990);
    ui.TextFitted("D-ROOM", {droom.x + 10, droom.y + 2,
                             droom.w - 20, droom.h - 4},
                  Color::White(), 1991,
                  engine::rendering::TextAlign::Center, 28.0f, 20.0f, true);

    const Rect buttons[4] = {
        {120, 690, 620, 92},
        {120, 800, 620, 92},
        {120, 910, 620, 92},
        {120, 1020, 300, 48}
    };
    for (int i = 0; i < 4; ++i) {
        DrawButton(ui, data, buttons[i], kMainItems[static_cast<std::size_t>(i)],
                   main_index_ == i, i != 1 || data.save_exists,
                   i == 3 ? 22.0f : 30.0f);
    }
    ui.TextFitted("LOCAL ONLY  //  NO SOCIAL FEATURES",
                  {820, 1000, 1000, 30}, {118, 145, 148, 255}, 1995,
                  engine::rendering::TextAlign::Left, 24.0f, 16.0f, false);
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
    else if (screen_ == UIScreen::Map) DrawMap(ui, data);
    else if (screen_ == UIScreen::CharacterSelect) DrawCharacterSelect(ui, data);
    else if (screen_ == UIScreen::WeaponSelect) DrawWeaponSelect(ui, data);
}

} // namespace soulknight::game::ui
