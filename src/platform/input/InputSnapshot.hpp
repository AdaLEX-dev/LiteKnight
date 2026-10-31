// Soul Knight Linux — Input Snapshot
#pragma once

#include "Action.hpp"
#include <cmath>
#include <bitset>
#include <array>
#include <engine/core/Types.hpp>
#include <engine/math/Vec2.hpp>

namespace soulknight::platform {

using engine::Vec2;

struct InputSnapshot {
    // Digital button state
    std::bitset<ACTION_COUNT> pressed;   // True only on frame pressed
    std::bitset<ACTION_COUNT> released;  // True only on frame released
    std::bitset<ACTION_COUNT> held;      // True while held

    // Analog axes (-1.0 to 1.0)
    f32 move_x = 0.0f;
    f32 move_y = 0.0f;
    f32 aim_x = 0.0f;
    f32 aim_y = 0.0f;
    f32 trigger_left = 0.0f;   // 0.0 to 1.0
    f32 trigger_right = 0.0f;  // 0.0 to 1.0

    // Mouse
    Vec2 mouse_position = {0, 0};
    Vec2 mouse_delta = {0, 0};
    bool mouse_pressed[3] = {false};
    bool mouse_released[3] = {false};
    f32 mouse_wheel = 0.0f;

    // Window
    bool window_focused = true;
    bool window_resized = false;
    Vec2 window_size = {1920, 1080};

    // Helpers
    constexpr bool IsPressed(Action a) const { return pressed[static_cast<uint8_t>(a)]; }
    constexpr bool IsReleased(Action a) const { return released[static_cast<uint8_t>(a)]; }
    constexpr bool IsHeld(Action a) const { return held[static_cast<uint8_t>(a)]; }
    constexpr engine::Vec2 GetMoveVector() const { return engine::Vec2{move_x, move_y}; }
    constexpr engine::Vec2 GetAimVector() const { return engine::Vec2{aim_x, aim_y}; }
};

} // namespace soulknight::platform