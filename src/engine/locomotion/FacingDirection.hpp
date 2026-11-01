// Soul Knight Linux — recovered horizontal facing model
#pragma once

#include "engine/math/Vec2.hpp"

namespace soulknight::engine::locomotion {

// The recovered c00 character clips contain only forward-facing ide/run
// frames. RGController represents left/right facing by rotating the "img"
// transform 180 degrees around Y (equivalent to a horizontal mirror); there
// are no separate up/down/diagonal animation sets. This enum intentionally
// models only the evidence-backed horizontal facing.
enum class FacingDirection {
    Right,
    Left,
};

inline FacingDirection ResolveFacing(FacingDirection current,
                                     const Vec2& movement_direction) {
    if (movement_direction.x > 0.0f) return FacingDirection::Right;
    if (movement_direction.x < 0.0f) return FacingDirection::Left;
    return current;
}

// HYPOTHESIS: the exact reference body-flip threshold is not exposed in the
// recovered serialized fields. The native contract uses the vertical axis
// with a small hysteresis band so a pointer exactly above/below the player
// cannot make the body jitter. The body still has only left/right frames.
inline FacingDirection ResolveAimFacing(FacingDirection current, const Vec2& aim,
                                        float hysteresis = 0.08f) {
    if (aim.x > hysteresis) return FacingDirection::Right;
    if (aim.x < -hysteresis) return FacingDirection::Left;
    return current;
}

// CONFIRMED from the recovered c00 controller: char_1_skin_0_ide,
// char_1_skin_0_run, char_1_skin_0_dead and char_hit are the only clips. There
// are no separate up/down/diagonal clips. Body orientation therefore follows
// horizontal movement while moving, and falls back to aim only while idle.
inline FacingDirection ResolveBodyFacing(FacingDirection current,
                                         const Vec2& movement,
                                         const Vec2& aim, bool moving) {
    if (movement.x > 0.01f) return FacingDirection::Right;
    if (movement.x < -0.01f) return FacingDirection::Left;
    if (!moving) return ResolveAimFacing(current, aim);
    return current;
}

inline const char* FacingDirectionName(FacingDirection facing) {
    return facing == FacingDirection::Left ? "left" : "right";
}

} // namespace soulknight::engine::locomotion
