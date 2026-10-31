// Soul Knight Linux — Player Controller
#pragma once

#include "PlayerInput.hpp"
#include "engine/core/Types.hpp"
#include <string>

namespace soulknight::game {

struct PlayerData {
    std::string character_id;
    std::string skin_id;
    int level = 1;
    int health = 100;
    int max_health = 100;
    int armor = 0;
    int energy = 100;
    int max_energy = 100;
    float move_speed = 5.0f;
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