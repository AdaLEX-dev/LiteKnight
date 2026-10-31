// Soul Knight Linux — Player Input
// PROVISIONAL SCAFFOLDING: the mapping below is a platform abstraction, not a
// reconstruction of the reference input semantics. See docs/VERTICAL_SLICE.md.
#pragma once

#include "engine/math/Vec2.hpp"
#include "platform/input/InputSnapshot.hpp"

namespace soulknight::game {

using engine::Vec2;

struct PlayerInput {
    Vec2 move_intent = {0, 0};
    Vec2 aim_direction = {1, 0};
    bool attack_pressed = false;
    bool attack_held = false;
    bool skill_pressed = false;
    bool dodge_pressed = false;
    bool interact_pressed = false;
    bool weapon_switch_pressed = false;
    int weapon_slot = -1;

    void UpdateFromSnapshot(const platform::InputSnapshot& snap);
};

} // namespace soulknight::game