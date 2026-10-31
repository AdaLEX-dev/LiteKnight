// Soul Knight Linux — Player Input Implementation
#include "PlayerInput.hpp"
#include "engine/core/Log.hpp"

namespace soulknight::game {

void PlayerInput::UpdateFromSnapshot(const platform::InputSnapshot& snap) {
    // Movement
    move_intent = snap.GetMoveVector();
    if (move_intent.LengthSquared() > 1.0f) move_intent.Normalize();

    // Aiming
    Vec2 aim = snap.GetAimVector();
    if (aim.LengthSquared() > 0.01f) {
        aim_direction = aim.Normalized();
    }

    // Buttons
    attack_pressed = snap.IsPressed(platform::Action::Attack);
    attack_held = snap.IsHeld(platform::Action::Attack);
    skill_pressed = snap.IsPressed(platform::Action::Skill);
    dodge_pressed = snap.IsPressed(platform::Action::Dodge);
    interact_pressed = snap.IsPressed(platform::Action::Interact);
    weapon_switch_pressed = snap.IsPressed(platform::Action::WeaponSwitch);

    if (snap.IsPressed(platform::Action::WeaponSlot1)) weapon_slot = 0;
    else if (snap.IsPressed(platform::Action::WeaponSlot2)) weapon_slot = 1;
    else if (snap.IsPressed(platform::Action::WeaponSlot3)) weapon_slot = 2;
    else weapon_slot = -1;
}

} // namespace soulknight::game