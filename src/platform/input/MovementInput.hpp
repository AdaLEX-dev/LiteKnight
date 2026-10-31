// Soul Knight Linux — keyboard movement intent
#pragma once

#include "InputSnapshot.hpp"
#include "engine/math/Vec2.hpp"

#include <algorithm>

namespace soulknight::platform {

// Keyboard-only movement abstraction. It deliberately does not read gamepad
// axes yet; the existing InputManager analog path can feed a future analog
// MovementInput implementation without changing the simulation contract.
struct MovementInput {
    engine::Vec2 direction{0.0f, 0.0f};
    float weight = 0.0f;

    bool IsActive() const { return weight > 0.0f; }
};

inline MovementInput ReadDigitalMovement(const InputSnapshot& snapshot) {
    MovementInput movement;
    float x = 0.0f;
    float y = 0.0f;
    if (snapshot.IsHeld(Action::MoveLeft)) x -= 1.0f;
    if (snapshot.IsHeld(Action::MoveRight)) x += 1.0f;
    if (snapshot.IsHeld(Action::MoveDown)) y -= 1.0f;
    if (snapshot.IsHeld(Action::MoveUp)) y += 1.0f;

    const engine::Vec2 raw{x, y};
    if (raw.LengthSquared() <= 0.0f) return movement;

    movement.direction = raw.Normalized();
    movement.weight = 1.0f;
    return movement;
}

} // namespace soulknight::platform
