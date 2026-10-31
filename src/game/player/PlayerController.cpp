// Soul Knight Linux — Player Controller (Placeholder)
#include "PlayerController.hpp"
#include "PlayerInput.hpp"
#include "engine/core/Log.hpp"

namespace soulknight::game {

void PlayerController::Initialize(const PlayerData& data) {
    LOG_DEBUG_GAME("PlayerController initialized for character: {}", data.character_id);
}

void PlayerController::Update(float dt, const PlayerInput& input) {
    (void)dt;
    (void)input;
    // TODO: Implement player movement, attack, dodge, etc.
}

void PlayerController::OnHit(float damage, const engine::Vec2& knockback) {
    (void)damage;
    (void)knockback;
    // TODO: Implement damage response
}

void PlayerController::OnDeath() {
    LOG_INFO_GAME("Player died");
    // TODO: Implement death logic
}

} // namespace soulknight::game