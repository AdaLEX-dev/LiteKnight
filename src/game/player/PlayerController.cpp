// Soul Knight Linux — Player Controller
// UNIMPLEMENTED PLACEHOLDER: Update/OnHit/OnDeath are intentionally empty.
// No reference movement, combat, or death behavior has been reconstructed.
#include "PlayerController.hpp"
#include "PlayerInput.hpp"
#include "engine/core/Log.hpp"

namespace soulknight::game {

void PlayerController::Initialize(const PlayerData& data) {
    (void)data; // UNIMPLEMENTED: no reference-derived player data is applied yet.
    LOG_DEBUG_GAME("PlayerController initialized for character: {}", data.character_id);
}

void PlayerController::Update(float dt, const PlayerInput& input) {
    (void)dt;
    (void)input;
    // NOT IMPLEMENTED: reference movement, attack, and dodge behavior is not reconstructed.
}

void PlayerController::OnHit(float damage, const engine::Vec2& knockback) {
    (void)damage;
    (void)knockback;
    // NOT IMPLEMENTED: reference damage response is not reconstructed.
}

void PlayerController::OnDeath() {
    LOG_INFO_GAME("Player died");
    // NOT IMPLEMENTED: reference death logic is not reconstructed.
}

} // namespace soulknight::game