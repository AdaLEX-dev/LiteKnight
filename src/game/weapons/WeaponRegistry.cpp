#include "WeaponRegistry.hpp"

namespace soulknight::game::weapons {

bool WeaponRegistry::Configure(const engine::assets::SceneAsset& scene) {
    definitions_ = scene.weapons;
    active_ = 0;
    return !definitions_.empty();
}

bool WeaponRegistry::Select(std::size_t index) {
    if (index >= definitions_.size()) return false;
    active_ = index;
    return true;
}

void WeaponRegistry::Cycle(int delta) {
    if (definitions_.empty()) return;
    const int count = static_cast<int>(definitions_.size());
    int next = (static_cast<int>(active_) + delta) % count;
    if (next < 0) next += count;
    active_ = static_cast<std::size_t>(next);
}

} // namespace soulknight::game::weapons
