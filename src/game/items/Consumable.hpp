// Soul Knight Linux — recovered potion/consumable restoration
#pragma once

#include <algorithm>

namespace soulknight::game::items {

struct RestoreResult {
    int health_gained = 0;
    int energy_gained = 0;
};

inline RestoreResult ApplyRestore(int health, int energy, int& player_hp,
                                  int player_max_hp, int& player_energy,
                                  int player_max_energy) {
    RestoreResult result;
    const int hp_before = player_hp;
    const int energy_before = player_energy;
    player_hp = std::clamp(player_hp + std::max(0, health), 0,
                           std::max(0, player_max_hp));
    player_energy = std::clamp(player_energy + std::max(0, energy), 0,
                               std::max(0, player_max_energy));
    result.health_gained = player_hp - hp_before;
    result.energy_gained = player_energy - energy_before;
    return result;
}

} // namespace soulknight::game::items
