// Soul Knight Linux — recovered player energy state
#pragma once

#include <algorithm>

namespace soulknight::game::player {

// RoleAttributePlayer serializes max_energy = 180 for c00. Weapons consume the
// recovered per-attack energy value and a zero-energy shot is rejected.
struct EnergyState {
    int energy = 0;
    int max_energy = 0;
    // CONFIRMED from RoleAttributePlayer.EnergyReLoad disassembly: while below
    // max, a timer accumulates and adds exactly 1 energy every 2.0 seconds.
    static constexpr float kRegenerationInterval = 2.0f;
    float regeneration_timer = 0.0f;

    void Reset(int maximum, int initial = -1) {
        max_energy = std::max(0, maximum);
        energy = initial < 0 ? max_energy : std::clamp(initial, 0, max_energy);
        regeneration_timer = 0.0f;
    }

    void Update(float delta_seconds) {
        if (delta_seconds <= 0.0f || energy >= max_energy) return;
        regeneration_timer += delta_seconds;
        while (regeneration_timer >= kRegenerationInterval &&
               energy < max_energy) {
            regeneration_timer -= kRegenerationInterval;
            ++energy;
        }
        if (energy >= max_energy) regeneration_timer = 0.0f;
    }

    bool CanConsume(int amount) const {
        return amount <= 0 || energy >= amount;
    }

    bool Consume(int amount) {
        if (amount <= 0) return true;
        if (energy < amount) return false;
        energy -= amount;
        return true;
    }
};

} // namespace soulknight::game::player
