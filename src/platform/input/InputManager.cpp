// Soul Knight Linux — Input Manager Implementation
#include "InputManager.hpp"
#include "Action.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <fstream>
#include <engine/core/Log.hpp>

namespace soulknight::platform {

InputManager::InputManager() {
    // Default keyboard mappings
    key_map_ = {
        {SDL_SCANCODE_W, Action::MoveUp},
        {SDL_SCANCODE_UP, Action::MoveUp},
        {SDL_SCANCODE_S, Action::MoveDown},
        {SDL_SCANCODE_DOWN, Action::MoveDown},
        {SDL_SCANCODE_A, Action::MoveLeft},
        {SDL_SCANCODE_LEFT, Action::MoveLeft},
        {SDL_SCANCODE_D, Action::MoveRight},
        {SDL_SCANCODE_RIGHT, Action::MoveRight},

        {SDL_SCANCODE_SPACE, Action::Attack},
        {SDL_SCANCODE_J, Action::Attack},
        // Native test aim vector, independent from WASD movement. The reference
        // uses a mobile right joystick; keypad is a keyboard test binding.
        {SDL_SCANCODE_KP_8, Action::AimUp},
        {SDL_SCANCODE_KP_2, Action::AimDown},
        {SDL_SCANCODE_KP_4, Action::AimLeft},
        {SDL_SCANCODE_KP_6, Action::AimRight},
        {SDL_SCANCODE_LSHIFT, Action::Skill},
        {SDL_SCANCODE_K, Action::Skill},
        {SDL_SCANCODE_LCTRL, Action::Dodge},
        {SDL_SCANCODE_L, Action::Dodge},
        {SDL_SCANCODE_E, Action::Interact},
        {SDL_SCANCODE_F, Action::Interact},
        {SDL_SCANCODE_Q, Action::WeaponSwitch},
        {SDL_SCANCODE_TAB, Action::WeaponSwitch},
        {SDL_SCANCODE_1, Action::WeaponSlot1},
        {SDL_SCANCODE_2, Action::WeaponSlot2},
        {SDL_SCANCODE_3, Action::WeaponSlot3},

        // One key maps to one action in this implementation. UI navigation is
        // not separately context-bound yet; menu screens must reuse the move
        // actions or a future context-aware mapping. Do not duplicate scancodes
        // here: unordered_map silently keeps only the first binding.
        {SDL_SCANCODE_RETURN, Action::Confirm},
        {SDL_SCANCODE_BACKSPACE, Action::Cancel},
        {SDL_SCANCODE_ESCAPE, Action::Pause},
        {SDL_SCANCODE_P, Action::Pause},
        {SDL_SCANCODE_I, Action::Inventory},
        {SDL_SCANCODE_M, Action::Map},

        {SDL_SCANCODE_F1, Action::DebugToggle},
        {SDL_SCANCODE_F2, Action::DebugNoclip},
        {SDL_SCANCODE_F3, Action::DebugKillAll},
        {SDL_SCANCODE_F4, Action::DebugSpawnEnemy},
    };

    // Default gamepad mappings
    gamepad_btn_map_ = {
        {SDL_GAMEPAD_BUTTON_DPAD_UP, Action::MenuUp},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, Action::MenuDown},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, Action::MenuLeft},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Action::MenuRight},
        {SDL_GAMEPAD_BUTTON_SOUTH, Action::Confirm},   // A
        {SDL_GAMEPAD_BUTTON_EAST, Action::Cancel},     // B
        {SDL_GAMEPAD_BUTTON_WEST, Action::Dodge},      // X
        {SDL_GAMEPAD_BUTTON_NORTH, Action::Skill},     // Y
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, Action::WeaponSlot1},  // LB
        {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, Action::WeaponSwitch}, // RB
        {SDL_GAMEPAD_BUTTON_START, Action::Pause},
        {SDL_GAMEPAD_BUTTON_BACK, Action::Inventory},
        {SDL_GAMEPAD_BUTTON_GUIDE, Action::Pause},
    };

    // Default gamepad axis mappings
    gamepad_axis_map_ = {
        {SDL_GAMEPAD_AXIS_LEFTX, {Action::MoveAxisX, Action::MoveAxisX}},
        {SDL_GAMEPAD_AXIS_LEFTY, {Action::MoveAxisY, Action::MoveAxisY}},
        {SDL_GAMEPAD_AXIS_RIGHTX, {Action::AimAxisX, Action::AimAxisX}},
        {SDL_GAMEPAD_AXIS_RIGHTY, {Action::AimAxisY, Action::AimAxisY}},
        {SDL_GAMEPAD_AXIS_LEFT_TRIGGER, {Action::Dodge, Action::Dodge}},
        {SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, {Action::Attack, Action::Attack}},
    };
}

InputManager::~InputManager() {
    Shutdown();
}

bool InputManager::Initialize(SDL_Window* window) {
    (void)window; // Not needed for SDL3 input

    // Open first available gamepad
    int gamepad_count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepad_count);
    for (int i = 0; i < gamepad_count; ++i) {
        SDL_JoystickID id = gamepads[i];
        if (SDL_IsGamepad(id)) {
            gamepad_ = SDL_OpenGamepad(id);
            if (gamepad_) {
                gamepad_connected_ = true;
                LOG_INFO_INPUT("Gamepad connected: {}", SDL_GetGamepadName(gamepad_));
                break;
            }
        }
    }
    if (gamepads) {
        SDL_free(gamepads);
    }

    if (!gamepad_connected_) {
        LOG_INFO_INPUT("No gamepad connected");
    }

    // Clear snapshots
    current_ = InputSnapshot{};
    previous_ = InputSnapshot{};

    LOG_INFO_INPUT("Input manager initialized");
    return true;
}

void InputManager::Shutdown() {
    if (gamepad_) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
        gamepad_connected_ = false;
    }
}

void InputManager::BeginFrame() {
    // Swap snapshots; held state persists until an up event arrives.
    previous_ = current_;
    current_.pressed.reset();
    current_.released.reset();
    current_.mouse_wheel = 0.0f;
    current_.window_resized = false;
    current_.mouse_delta = {0, 0};
}

void InputManager::ProcessEvent(const SDL_Event& event) {
    HandleSDLEvent(event);
}

void InputManager::EndFrame() {
    UpdateAnalogState();
    UpdateDigitalState();
}

void InputManager::ProcessEvents() {
    BeginFrame();

    // Process synthetic events queued by the test/debug injection helpers
    // before real SDL events. This keeps injection deterministic and
    // independent of whether SDL has been initialized.
    for (const SDL_Event& injected : injected_events_) {
        HandleSDLEvent(injected);
    }
    injected_events_.clear();

    // Process SDL events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        HandleSDLEvent(event);
    }

    EndFrame();
}

void InputManager::UpdateDigitalState() {
    // Held buttons are already tracked in HandleSDLEvent
    // Nothing extra needed for keyboard/gamepad buttons

    // Mouse buttons
    for (int i = 0; i < 3; ++i) {
        if (current_.mouse_pressed[i] && !previous_.mouse_pressed[i]) {
            // Just pressed this frame
        } else if (!current_.mouse_pressed[i] && previous_.mouse_pressed[i]) {
            current_.mouse_released[i] = true;
        }
    }
}

void InputManager::UpdateAnalogState() {
    if (!gamepad_ || !gamepad_connected_) return;

    // Left stick
    f32 lx = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    f32 ly = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    current_.move_x = ProcessAxis(lx);
    current_.move_y = ProcessAxis(-ly); // Y is inverted in SDL

    // Right stick
    f32 rx = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
    f32 ry = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
    current_.aim_x = ProcessAxis(rx);
    current_.aim_y = ProcessAxis(-ry);

    // Triggers
    f32 lt = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    f32 rt = SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    current_.trigger_left = std::clamp(lt, 0.0f, 1.0f);
    current_.trigger_right = std::clamp(rt, 0.0f, 1.0f);
}

f32 InputManager::ProcessAxis(f32 raw) const {
    if (std::abs(raw) < analog_settings_.deadzone) return 0.0f;

    f32 sign = raw > 0 ? 1.0f : -1.0f;
    f32 abs_raw = std::abs(raw);
    f32 mapped = (abs_raw - analog_settings_.deadzone) / (analog_settings_.outer_deadzone - analog_settings_.deadzone);
    mapped = std::clamp(mapped, 0.0f, 1.0f);

    if (analog_settings_.invert_x) mapped = -mapped; // For specific axes

    return sign * mapped * analog_settings_.sensitivity;
}

void InputManager::HandleSDLEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            auto it = key_map_.find(event.key.scancode);
            if (it != key_map_.end()) {
                Action a = it->second;
                bool down = event.type == SDL_EVENT_KEY_DOWN;
                if (down && !current_.held[static_cast<uint8_t>(a)]) {
                    current_.pressed[static_cast<uint8_t>(a)] = true;
                } else if (!down && current_.held[static_cast<uint8_t>(a)]) {
                    current_.released[static_cast<uint8_t>(a)] = true;
                }
                current_.held[static_cast<uint8_t>(a)] = down;
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            auto it = gamepad_btn_map_.find(static_cast<SDL_GamepadButton>(event.gbutton.button));
            if (it != gamepad_btn_map_.end()) {
                Action a = it->second;
                bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
                if (down && !current_.held[static_cast<uint8_t>(a)]) {
                    current_.pressed[static_cast<uint8_t>(a)] = true;
                } else if (!down && current_.held[static_cast<uint8_t>(a)]) {
                    current_.released[static_cast<uint8_t>(a)] = true;
                }
                current_.held[static_cast<uint8_t>(a)] = down;
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            const f32 raw = static_cast<f32>(event.gaxis.value) / 32767.0f;
            const f32 processed = ProcessAxis(raw);
            switch (event.gaxis.axis) {
                case SDL_GAMEPAD_AXIS_LEFTX:
                    current_.move_x = processed;
                    break;
                case SDL_GAMEPAD_AXIS_LEFTY:
                    current_.move_y = -processed;
                    break;
                case SDL_GAMEPAD_AXIS_RIGHTX:
                    current_.aim_x = processed;
                    break;
                case SDL_GAMEPAD_AXIS_RIGHTY:
                    current_.aim_y = -processed;
                    break;
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
                    current_.trigger_left = std::clamp(raw, 0.0f, 1.0f);
                    break;
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
                    current_.trigger_right = std::clamp(raw, 0.0f, 1.0f);
                    break;
                default:
                    break;
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            current_.mouse_position = {event.motion.x, event.motion.y};
            current_.mouse_delta = {event.motion.xrel, event.motion.yrel};
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int btn = event.button.button - 1; // SDL_BUTTON_LEFT=1 -> 0
            if (btn >= 0 && btn < 3) {
                bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                current_.mouse_pressed[btn] = down;
                current_.mouse_released[btn] = !down;
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            current_.mouse_wheel = event.wheel.y;
            break;
        }
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST: {
            current_.window_focused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
            break;
        }
        case SDL_EVENT_WINDOW_RESIZED: {
            current_.window_resized = true;
            current_.window_size = {float(event.window.data1), float(event.window.data2)};
            break;
        }
        case SDL_EVENT_GAMEPAD_ADDED: {
            if (!gamepad_connected_) {
                gamepad_ = SDL_OpenGamepad(event.gdevice.which);
                if (gamepad_) {
                    gamepad_connected_ = true;
                    LOG_INFO_INPUT("Gamepad connected: {}", SDL_GetGamepadName(gamepad_));
                }
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_REMOVED: {
            if (gamepad_ && event.gdevice.which == SDL_GetGamepadID(gamepad_)) {
                SDL_CloseGamepad(gamepad_);
                gamepad_ = nullptr;
                gamepad_connected_ = false;
                LOG_INFO_INPUT("Gamepad disconnected");
            }
            break;
        }
    }
}

void InputManager::BindKey(SDL_Scancode key, Action action) {
    key_map_[key] = action;
}

void InputManager::UnbindKey(SDL_Scancode key) {
    key_map_.erase(key);
}

void InputManager::BindGamepadButton(SDL_GamepadButton btn, Action action) {
    gamepad_btn_map_[btn] = action;
}

void InputManager::BindGamepadAxis(SDL_GamepadAxis axis, Action pos_action, Action neg_action) {
    gamepad_axis_map_[axis] = {pos_action, neg_action};
}

bool InputManager::LoadMappings(const std::string& path) {
    // UNIMPLEMENTED: no mapping-file format or parser exists yet.
    (void)path;
    return false;
}

bool InputManager::SaveMappings(const std::string& path) const {
    // UNIMPLEMENTED: no mapping-file format or serializer exists yet.
    (void)path;
    return false;
}

void InputManager::InjectKeyDown(SDL_Scancode key) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = key;
    event.key.down = true;
    event.key.repeat = false;
    injected_events_.push_back(event);
}

void InputManager::InjectKeyUp(SDL_Scancode key) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_UP;
    event.key.scancode = key;
    event.key.down = false;
    event.key.repeat = false;
    injected_events_.push_back(event);
}

void InputManager::InjectGamepadButtonDown(SDL_GamepadButton btn) {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    event.gbutton.button = btn;
    event.gbutton.down = true;
    injected_events_.push_back(event);
}

void InputManager::InjectGamepadButtonUp(SDL_GamepadButton btn) {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    event.gbutton.button = btn;
    event.gbutton.down = false;
    injected_events_.push_back(event);
}

void InputManager::InjectGamepadAxis(SDL_GamepadAxis axis, f32 value) {
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.axis = axis;
    event.gaxis.value = static_cast<Sint16>(std::clamp(value, -1.0f, 1.0f) * 32767.0f);
    injected_events_.push_back(event);
}

void InputManager::InjectMouseMotion(f32 x, f32 y, f32 xrel, f32 yrel) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = x;
    event.motion.y = y;
    event.motion.xrel = xrel;
    event.motion.yrel = yrel;
    injected_events_.push_back(event);
}

void InputManager::InjectMouseButtonDown(uint8_t button) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = static_cast<uint8_t>(button + 1);
    event.button.down = true;
    injected_events_.push_back(event);
}

void InputManager::InjectMouseButtonUp(uint8_t button) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button = static_cast<uint8_t>(button + 1);
    event.button.down = false;
    injected_events_.push_back(event);
}

void InputManager::InjectMouseWheel(f32 y) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.y = y;
    injected_events_.push_back(event);
}

} // namespace soulknight::platform