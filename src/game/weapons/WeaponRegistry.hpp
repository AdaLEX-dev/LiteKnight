// Soul Knight Linux — recovered weapon definitions available to the devroom
#pragma once

#include "engine/assets/SceneAsset.hpp"

#include <cstddef>
#include <vector>

namespace soulknight::game::weapons {

// Registry of serialized/recovered weapon definitions. Selection changes the
// active definition; it does not mutate player position or collision state.
class WeaponRegistry {
public:
    bool Configure(const engine::assets::SceneAsset& scene);
    std::size_t Count() const { return definitions_.size(); }
    std::size_t ActiveIndex() const { return active_; }
    const engine::assets::WeaponDefinition& Active() const {
        return definitions_[active_];
    }
    const std::vector<engine::assets::WeaponDefinition>& Definitions() const {
        return definitions_;
    }
    bool Select(std::size_t index);
    void Cycle(int delta);

private:
    std::vector<engine::assets::WeaponDefinition> definitions_;
    std::size_t active_ = 0;
};

} // namespace soulknight::game::weapons
