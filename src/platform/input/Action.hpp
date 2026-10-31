// Soul Knight Linux — Input Actions
#pragma once

#include <cstdint>

namespace soulknight::platform {

enum class Action : uint8_t {
    // Movement (digital)
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,

    // Analog movement (gamepad left stick)
    MoveAxisX,  // -1.0 to 1.0
    MoveAxisY,  // -1.0 to 1.0

    // Aiming (mouse / right stick)
    AimAxisX,   // -1.0 to 1.0
    AimAxisY,   // -1.0 to 1.0

    // Primary actions
    Attack,
    Skill,
    Dodge,
    Interact,
    WeaponSwitch,
    WeaponSlot1,
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

    // Debug
    DebugToggle,
    DebugNoclip,
    DebugKillAll,
    DebugSpawnEnemy,

    COUNT  // Must be last
};

constexpr uint8_t ACTION_COUNT = static_cast<uint8_t>(Action::COUNT);

} // namespace soulknight::platform