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

inline const char* FacingDirectionName(FacingDirection facing) {
    return facing == FacingDirection::Left ? "left" : "right";
}

} // namespace soulknight::engine::locomotion
