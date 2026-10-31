// Soul Knight Linux — two-slot developer weapon inventory
#pragma once

#include <cstddef>
#include <array>

namespace soulknight::game::weapons {

class WeaponInventory {
public:
    void Configure(std::size_t available_count);
    bool EquipActive(std::size_t index);
    bool SwitchSlot(std::size_t slot);
    std::size_t ActiveSlot() const { return active_slot_; }
    std::size_t Slot(std::size_t slot) const { return slots_[slot]; }
    std::size_t ActiveIndex() const { return slots_[active_slot_]; }
    std::size_t Count() const { return count_; }

private:
    std::size_t count_ = 0;
    std::array<std::size_t, 2> slots_{0, 0};
    std::size_t active_slot_ = 0;
};

} // namespace soulknight::game::weapons
