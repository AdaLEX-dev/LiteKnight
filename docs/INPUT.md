# Soul Knight 2.6.8 — Input Design

> **Status:** design proposal only. The reference input semantics are **NOT**
> reconstructed. The SDL3 action mapping in `src/platform/input/` is a native
> scaffold and must not be cited as parity evidence. See `docs/VERTICAL_SLICE.md`.

**Target:** Keyboard, Mouse, Gamepad (SDL3), Wayland

---

## 1. Action Definitions

### Core Gameplay Actions
```cpp
// src/platform/input/Action.h
enum class Action : uint8_t {
    // Movement (digital + analog)
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,

    // Analog movement (gamepad left stick)
    MoveAxisX,  // -1.0..1.0
    MoveAxisY,  // -1.0..1.0

    // Aiming (mouse / right stick)
    AimAxisX,   // -1.0..1.0
    AimAxisY,   // -1.0..1.0

    // Primary actions
    Attack,         // Tap: shoot/melee, Hold: charge
    Skill,          // Character ability
    Dodge,          // Dash/roll
    Interact,       // Pickup, open chest, talk
    WeaponSwitch,   // Cycle weapons
    WeaponSlot1,    // Direct slot access
    WeaponSlot2,
    WeaponSlot3,

    // UI Navigation
    MenuUp,
    MenuDown,
    MenuLeft,
    MenuRight,
    Confirm,
    Cancel,
    Pause,
    Inventory,
    Map,

    // Debug (dev builds)
    DebugToggle,
    DebugNoclip,
    DebugKillAll,
    DebugSpawnEnemy,

    COUNT  // Must be last
};
```

---

## 2. Input Mapping (Configurable)

### Default Keyboard (config/input.toml)
```toml
[keyboard]
MoveUp = ["W", "Up"]
MoveDown = ["S", "Down"]
MoveLeft = ["A", "Left"]
MoveRight = ["D", "Right"]
Attack = ["Space", "J"]
Skill = ["LeftShift", "K"]
Dodge = ["LeftCtrl", "L"]
Interact = ["E", "F"]
WeaponSwitch = ["Q", "Tab"]
WeaponSlot1 = ["1"]
WeaponSlot2 = ["2"]
WeaponSlot3 = ["3"]
MenuUp = ["W", "Up"]
MenuDown = ["S", "Down"]
MenuLeft = ["A", "Left"]
MenuRight = ["D", "Right"]
Confirm = ["Enter", "Space"]
Cancel = ["Escape", "Backspace"]
Pause = ["Escape", "P"]
Inventory = ["Tab", "I"]
Map = ["M"]

[mouse]
# Mouse position → AimAxis
# Left click → Attack
# Right click → Skill
# Middle click → Dodge
# Scroll → WeaponSwitch
```

### Default Gamepad (SDL3 Gamepad API)
```toml
[gamepad]
# Left stick → MoveAxis (deadzone 0.2)
# Right stick → AimAxis (deadzone 0.2)
# DPad → Menu navigation
Attack = "A"           # South button
Skill = "B"            # East button
Dodge = "X"            # West button
Interact = "Y"         # North button
WeaponSwitch = "RightShoulder"  # RB
WeaponSlot1 = "LeftShoulder"    # LB
Pause = "Start"
Inventory = "Back"
Confirm = "A"
Cancel = "B"
MenuUp = "DPadUp"
MenuDown = "DPadDown"
MenuLeft = "DPadLeft"
MenuRight = "DPadRight"

[gamepad_triggers]
# Left trigger → Dodge (analog)
# Right trigger → Attack (analog, for charge weapons)
```

---

## 3. Input Snapshot (Per Frame)

```cpp
// src/platform/input/InputSnapshot.h
struct InputSnapshot {
    // Digital button state
    std::bitset<Action::COUNT> pressed;   // True only on frame pressed
    std::bitset<Action::COUNT> released;  // True only on frame released
    std::bitset<Action::COUNT> held;      // True while held

    // Analog axes (-1.0 to 1.0)
    float move_x = 0.0f;
    float move_y = 0.0f;
    float aim_x = 0.0f;
    float aim_y = 0.0f;
    float trigger_left = 0.0f;   // 0.0 to 1.0
    float trigger_right = 0.0f;  // 0.0 to 1.0

    // Mouse (if not using gamepad aim)
    Vec2 mouse_position = {0, 0};      // Screen coords
    Vec2 mouse_delta = {0, 0};         // Relative motion
    bool mouse_pressed[3] = {false};   // Left, Middle, Right
    bool mouse_released[3] = {false};
    float mouse_wheel = 0.0f;          // Scroll delta

    // Window
    bool window_focused = true;
    bool window_resized = false;
    Vec2 window_size = {1920, 1080};

    // Helpers
    bool IsPressed(Action a) const { return pressed[static_cast<uint8_t>(a)]; }
    bool IsReleased(Action a) const { return released[static_cast<uint8_t>(a)]; }
    bool IsHeld(Action a) const { return held[static_cast<uint8_t>(a)]; }
    Vec2 GetMoveVector() const { return {move_x, move_y}; }
    Vec2 GetAimVector() const { return {aim_x, aim_y}; }
};
```

---

## 4. Input Manager

```cpp
// src/platform/input/InputManager.cpp
class InputManager {
public:
    InputManager();
    ~InputManager();

    // Called once per frame from platform layer
    void ProcessEvents();

    // Get current frame snapshot
    const InputSnapshot& GetSnapshot() const { return current_; }

    // Remapping (runtime)
    void BindKey(SDL_Scancode key, Action action);
    void BindGamepadButton(SDL_GamepadButton btn, Action action);
    void BindGamepadAxis(SDL_GamepadAxis axis, Action action_pos, Action action_neg);

    // Load/save mappings
    void LoadMappings(const std::string& path);
    void SaveMappings(const std::string& path) const;

    // Debug
    void SetGamepadConnected(bool connected);
    bool IsGamepadConnected() const;

private:
    InputSnapshot current_;
    InputSnapshot previous_;

    // Mappings
    std::unordered_map<SDL_Scancode, Action> key_map_;
    std::unordered_map<SDL_GamepadButton, Action> gamepad_btn_map_;
    std::unordered_map<SDL_GamepadAxis, std::pair<Action, Action>> gamepad_axis_map_;

    SDL_Gamepad* gamepad_ = nullptr;

    void UpdateDigitalState();
    void UpdateAnalogState();
    void HandleSDLEvent(const SDL_Event& event);
};
```

---

## 5. SDL Event Processing

```cpp
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
            auto it = gamepad_btn_map_.find(event.gbutton.button);
            if (it != gamepad_btn_map_.end()) {
                Action a = it->second;
                bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
                // Same logic as keyboard
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            // Left stick: SDL_GAMEPAD_AXIS_LEFTX/LEFTY
            // Right stick: SDL_GAMEPAD_AXIS_RIGHTX/RIGHTY
            // Triggers: SDL_GAMEPAD_AXIS_LEFT_TRIGGER/RIGHT_TRIGGER
            float value = event.gaxis.value / 32767.0f;  // -1.0 to 1.0
            // Apply deadzone
            if (std::abs(value) < 0.15f) value = 0.0f;
            // Map to actions...
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            current_.mouse_position = {float(event.motion.x), float(event.motion.y)};
            current_.mouse_delta = {float(event.motion.xrel), float(event.motion.yrel)};
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int btn = event.button.button - 1;  // SDL_BUTTON_LEFT=1 → 0
            if (btn >= 0 && btn < 3) {
                bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                current_.mouse_pressed[btn] = down;
                current_.mouse_released[btn] = !down;
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            current_.mouse_wheel = float(event.wheel.y);
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
    }
}
```

---

## 6. Gameplay Input Consumption

### Player Input Component
```cpp
// src/game/player/PlayerInput.cpp
struct PlayerInput {
    Vec2 move_intent = {0, 0};      // Normalized -1..1
    Vec2 aim_direction = {1, 0};    // Normalized
    bool attack_pressed = false;
    bool attack_held = false;
    bool skill_pressed = false;
    bool dodge_pressed = false;
    bool interact_pressed = false;
    bool weapon_switch_pressed = false;
    int weapon_slot = -1;  // 0,1,2 or -1

    void UpdateFromSnapshot(const InputSnapshot& snap) {
        // Movement (keyboard WASD or gamepad left stick)
        move_intent = snap.GetMoveVector();
        if (move_intent.LengthSquared() > 1.0f) move_intent.Normalize();

        // Aiming (mouse or right stick)
        Vec2 aim = snap.GetAimVector();
        if (aim.LengthSquared() > 0.01f) {
            aim_direction = aim.Normalized();
        }

        // Buttons
        attack_pressed = snap.IsPressed(Action::Attack);
        attack_held = snap.IsHeld(Action::Attack);
        skill_pressed = snap.IsPressed(Action::Skill);
        dodge_pressed = snap.IsPressed(Action::Dodge);
        interact_pressed = snap.IsPressed(Action::Interact);
        weapon_switch_pressed = snap.IsPressed(Action::WeaponSwitch);

        // Direct weapon slots
        if (snap.IsPressed(Action::WeaponSlot1)) weapon_slot = 0;
        else if (snap.IsPressed(Action::WeaponSlot2)) weapon_slot = 1;
        else if (snap.IsPressed(Action::WeaponSlot3)) weapon_slot = 2;
        else weapon_slot = -1;
    }
};
```

### Input Contexts
```cpp
enum class InputContext {
    Gameplay,
    Menu,
    Inventory,
    Dialog,
    Pause,
    Settings
};

class InputContextManager {
    InputContext current_ = InputContext::Gameplay;
    std::stack<InputContext> stack_;

    void Push(InputContext ctx) { stack_.push(current_); current_ = ctx; }
    void Pop() { if (!stack_.empty()) { current_ = stack_.top(); stack_.pop(); } }
    void Set(InputContext ctx) { current_ = ctx; while (!stack_.empty()) stack_.pop(); }

    // Consumes snapshot based on context
    PlayerInput GetPlayerInput(const InputSnapshot& snap) {
        PlayerInput input;
        if (current_ == InputContext::Gameplay) {
            input.UpdateFromSnapshot(snap);
        }
        // Menu/Inventory/etc handle their own input
        return input;
    }
};
```

---

## 7. Deadzone & Sensitivity

```cpp
struct AnalogSettings {
    float deadzone = 0.15f;        // Ignore small stick deflection
    float outer_deadzone = 0.95f;  // Clamp to 1.0 near edge
    float sensitivity = 1.0f;      // Multiply final value
    bool invert_x = false;
    bool invert_y = false;
};

float ProcessAxis(float raw, const AnalogSettings& settings) {
    if (std::abs(raw) < settings.deadzone) return 0.0f;

    // Map deadzone..outer_deadzone to 0..1
    float sign = raw > 0 ? 1.0f : -1.0f;
    float abs_raw = std::abs(raw);
    float mapped = (abs_raw - settings.deadzone) / (settings.outer_deadzone - settings.deadzone);
    mapped = std::clamp(mapped, 0.0f, 1.0f);

    return sign * mapped * settings.sensitivity;
}
```

---

## 8. Testing

```cpp
// tests/unit/input_test.cpp
TEST("Input snapshot captures press/release/hold") {
    InputManager mgr;
    mgr.BindKey(SDL_SCANCODE_W, Action::MoveUp);

    // Simulate press
    mgr.InjectKeyDown(SDL_SCANCODE_W);
    mgr.ProcessEvents();
    CHECK(mgr.GetSnapshot().IsPressed(Action::MoveUp));
    CHECK(mgr.GetSnapshot().IsHeld(Action::MoveUp));

    // Next frame - hold
    mgr.ProcessEvents();
    CHECK(!mgr.GetSnapshot().IsPressed(Action::MoveUp));
    CHECK(mgr.GetSnapshot().IsHeld(Action::MoveUp));

    // Release
    mgr.InjectKeyUp(SDL_SCANCODE_W);
    mgr.ProcessEvents();
    CHECK(mgr.GetSnapshot().IsReleased(Action::MoveUp));
    CHECK(!mgr.GetSnapshot().IsHeld(Action::MoveUp));
}

TEST("Analog deadzone applied") {
    InputManager mgr;
    mgr.InjectGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 0.1f);  // Below deadzone
    mgr.ProcessEvents();
    CHECK(mgr.GetSnapshot().move_x == 0.0f);

    mgr.InjectGamepadAxis(SDL_GAMEPAD_AXIS_LEFTX, 0.5f);
    mgr.ProcessEvents();
    CHECK(mgr.GetSnapshot().move_x > 0.0f);
}
```

---

## 9. Accessibility

```toml
# config/input.toml (user overrides)
[accessibility]
# Auto-attack (hold attack to repeat)
auto_attack = false
auto_attack_interval = 0.15  # seconds

# Simplified controls
one_button_mode = false      # Auto-aim, auto-dodge
hold_to_aim = true           # Hold aim button to show reticle

# Remap confirm/cancel for regions
confirm_button = "A"         # "A" or "B"
cancel_button = "B"

# Vibration
vibration_strength = 1.0     # 0.0 to 1.0
```

---

**Status:** DESIGN PROPOSAL ONLY — not reference evidence. Implementation is blocked by the evidence gates; this design is not a completion claim.