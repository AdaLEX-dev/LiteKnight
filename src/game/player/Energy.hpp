// Soul Knight Linux — recovered player energy state
#pragma once

#include <algorithm>

namespace soulknight::game::player {

// RoleAttributePlayer serializes max_energy = 180 for c00. Passive regeneration
// is not reconstructed for this slice; weapons consume energy per shot and a
// zero-energy shot is rejected until a recovered recovery source is added.
struct EnergyState {
    int energy = 0;
    int max_energy = 0;

    void Reset(int maximum, int initial = -1) {
        max_energy = std::max(0, maximum);
        energy = initial < 0 ? max_energy : std::clamp(initial, 0, max_energy);
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
