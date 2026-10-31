// Soul Knight Linux — Input Manager
#pragma once

#include "InputSnapshot.hpp"
#include <SDL3/SDL.h>
#include <unordered_map>
#include <string>
#include <vector>

namespace soulknight::platform {

struct AnalogSettings {
    f32 deadzone = 0.15f;
    f32 outer_deadzone = 0.95f;
    f32 sensitivity = 1.0f;
    bool invert_x = false;
    bool invert_y = false;
};

class InputManager {
public:
    InputManager();
    ~InputManager();

    bool Initialize(SDL_Window* window);
    void Shutdown();

    void ProcessEvents();

    const InputSnapshot& GetSnapshot() const { return current_; }

    // Remapping
    void BindKey(SDL_Scancode key, Action action);
    void UnbindKey(SDL_Scancode key);
    void BindGamepadButton(SDL_GamepadButton btn, Action action);
    void BindGamepadAxis(SDL_GamepadAxis axis, Action pos_action, Action neg_action);

    void SetAnalogSettings(const AnalogSettings& settings) { analog_settings_ = settings; }
    const AnalogSettings& GetAnalogSettings() const { return analog_settings_; }

    // Load/save mappings (TOML)
    bool LoadMappings(const std::string& path);
    bool SaveMappings(const std::string& path) const;

    // Debug
    void SetGamepadConnected(bool connected) { gamepad_connected_ = connected; }
    bool IsGamepadConnected() const { return gamepad_connected_; }

    // Testing support
    void InjectKeyDown(SDL_Scancode key);
    void InjectKeyUp(SDL_Scancode key);
    void InjectGamepadButtonDown(SDL_GamepadButton btn);
    void InjectGamepadButtonUp(SDL_GamepadButton btn);
    void InjectGamepadAxis(SDL_GamepadAxis axis, f32 value);
    void InjectMouseMotion(f32 x, f32 y, f32 xrel, f32 yrel);
    void InjectMouseButtonDown(uint8_t button);
    void InjectMouseButtonUp(uint8_t button);
    void InjectMouseWheel(f32 y);

private:
    InputSnapshot current_;
    InputSnapshot previous_;

    std::unordered_map<SDL_Scancode, Action> key_map_;
    std::unordered_map<SDL_GamepadButton, Action> gamepad_btn_map_;
    struct AxisMapping { Action pos, neg; };
    std::unordered_map<SDL_GamepadAxis, AxisMapping> gamepad_axis_map_;

    AnalogSettings analog_settings_;
    SDL_Gamepad* gamepad_ = nullptr;
    bool gamepad_connected_ = false;
    std::vector<SDL_Event> injected_events_;

    void UpdateDigitalState();
    void UpdateAnalogState();
    void HandleSDLEvent(const SDL_Event& event);
    f32 ProcessAxis(f32 raw) const;
};

} // namespace soulknight::platform