// Soul Knight Linux — world weapon pickup foundation
#pragma once

#include "engine/math/Vec2.hpp"

#include <cstddef>
#include <vector>

namespace soulknight::game::weapons {

struct WeaponPickup {
    std::size_t weapon_index = 0;
    engine::Vec2 position{0.0f, 0.0f};
    float animation_time = 0.0f;
    bool active = true;
};

// Foundation only: a world weapon pickup uses the recovered weapon prefab
// sprite and can be transferred into the active inventory slot. Production
// drop tables, merge rules, gold prices and pickup interaction UI are not
// reconstructed yet.
class WeaponPickupSystem {
public:
    void Clear();
    bool Spawn(std::size_t weapon_index, const engine::Vec2& position);
    void Update(float delta_seconds);

    // Deactivates and returns the nearest active pickup within radius.
    bool TakeNearest(const engine::Vec2& player_position, float radius,
                     std::size_t* weapon_index);

    const std::vector<WeaponPickup>& Pickups() const { return pickups_; }
    std::size_t ActiveCount() const;

private:
    std::vector<WeaponPickup> pickups_;
};

} // namespace soulknight::game::weapons
