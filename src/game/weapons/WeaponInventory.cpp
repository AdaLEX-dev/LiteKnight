#include "WeaponInventory.hpp"

#include <algorithm>

namespace soulknight::game::weapons {

void WeaponInventory::Configure(std::size_t available_count) {
    count_ = available_count;
    slots_ = {0, 0};
    if (count_ > 1) slots_[1] = 1;
    active_slot_ = 0;
}

bool WeaponInventory::EquipActive(std::size_t index) {
    if (count_ == 0 || index >= count_) return false;
    slots_[active_slot_] = index;
    return true;
}

bool WeaponInventory::SwitchSlot(std::size_t slot) {
    if (count_ == 0 || slot >= slots_.size()) return false;
    active_slot_ = slot;
    return true;
}

} // namespace soulknight::game::weapons
