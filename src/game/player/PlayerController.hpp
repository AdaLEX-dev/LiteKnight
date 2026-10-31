// Soul Knight Linux — Player Controller
#pragma once

#include "PlayerInput.hpp"
#include "engine/core/Types.hpp"
#include <string>

namespace soulknight::game {

// UNIMPLEMENTED SCAFFOLDING: zero defaults are deliberate. Reference player
// stats and movement values have not been reconstructed; do not use these in
// gameplay or treat them as parity data.
struct PlayerData {
    std::string character_id;
    std::string skin_id;
    int level = 0;
    int health = 0;
    int max_health = 0;
    int armor = 0;
    int energy = 0;
    int max_energy = 0;
    float move_speed = 0.0f;
};

class PlayerController {
public:
    void Initialize(const PlayerData& data);
    void Update(float dt, const PlayerInput& input);
    void OnHit(float damage, const engine::Vec2& knockback);
    void OnDeath();

private:
    PlayerData data_;
    float dodge_cooldown_ = 0.0f;
};

} // namespace soulknight::game