#include "WeaponPickup.hpp"

#include <limits>

namespace soulknight::game::weapons {

void WeaponPickupSystem::Clear() {
    pickups_.clear();
}

bool WeaponPickupSystem::Spawn(std::size_t weapon_index,
                               const engine::Vec2& position) {
    WeaponPickup pickup;
    pickup.weapon_index = weapon_index;
    pickup.position = position;
    pickups_.push_back(pickup);
    return true;
}

void WeaponPickupSystem::Update(float delta_seconds) {
    if (delta_seconds <= 0.0f) return;
    for (WeaponPickup& pickup : pickups_) {
        if (!pickup.active) continue;
        pickup.animation_time += delta_seconds;
    }
}

bool WeaponPickupSystem::TakeNearest(const engine::Vec2& player_position,
                                     float radius,
                                     std::size_t* weapon_index) {
    if (radius <= 0.0f) return false;
    std::size_t best = pickups_.size();
    float best_distance = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < pickups_.size(); ++i) {
        const WeaponPickup& pickup = pickups_[i];
        if (!pickup.active) continue;
        const float distance =
            (pickup.position - player_position).LengthSquared();
        if (distance <= radius * radius && distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    if (best == pickups_.size()) return false;
    pickups_[best].active = false;
    if (weapon_index != nullptr) {
        *weapon_index = pickups_[best].weapon_index;
    }
    return true;
}

std::size_t WeaponPickupSystem::ActiveCount() const {
    std::size_t count = 0;
    for (const WeaponPickup& pickup : pickups_) {
        if (pickup.active) ++count;
    }
    return count;
}

} // namespace soulknight::game::weapons
