// Soul Knight Linux — deterministic room entry/combat/clear state machine
#pragma once

#include "engine/math/Vec2.hpp"

namespace soulknight::game::room {

// UNENTERED is represented by Inactive when a room is not the active scene.
// The physical room transition puts the player just inside the entrance, so
// the live sequence is Entered -> Combat -> Cleared. Enemies remain dormant in
// Entered, which prevents tracking/attacking through the closing room boundary.
enum class RoomActivationState {
    Inactive,
    Entered,
    Combat,
    Cleared
};

inline const char* RoomActivationName(RoomActivationState state) {
    switch (state) {
        case RoomActivationState::Inactive: return "UNENTERED";
        case RoomActivationState::Entered: return "ENTERED";
        case RoomActivationState::Combat: return "COMBAT";
        case RoomActivationState::Cleared: return "CLEARED";
    }
    return "UNENTERED";
}

// `entry_grace` is the time the player can approach the room center before the
// fight activates. Crossing 1.25 world units inward activates immediately.
inline RoomActivationState AdvanceRoomActivation(
        RoomActivationState state, float delta_seconds,
        const engine::Vec2& entry_position,
        const engine::Vec2& player_position,
        bool has_enemies, bool all_dead, float entry_grace = 0.45f) {
    switch (state) {
        case RoomActivationState::Inactive:
            if (!has_enemies || all_dead) return RoomActivationState::Cleared;
            return RoomActivationState::Entered;
        case RoomActivationState::Entered: {
            if (!has_enemies || all_dead) return RoomActivationState::Cleared;
            const bool inward =
                (player_position - entry_position).LengthSquared() >
                1.25f * 1.25f;
            if (inward || delta_seconds >= entry_grace) {
                return RoomActivationState::Combat;
            }
            return RoomActivationState::Entered;
        }
        case RoomActivationState::Combat:
            return all_dead ? RoomActivationState::Cleared
                            : RoomActivationState::Combat;
        case RoomActivationState::Cleared:
            return RoomActivationState::Cleared;
    }
    return state;
}

} // namespace soulknight::game::room
