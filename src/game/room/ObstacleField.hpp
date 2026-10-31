// Soul Knight Linux — recovered obstacle variants for the dev room
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/collision/AabbCollision.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace soulknight::game::room {

// The central r1_1 RGObjectSkin markers are replaced by obstacle_list
// prefabs. This class keeps the recovered marker grid and lets the dev room
// cycle the active recovered prefab while keeping rendered instances and
// collision boxes derived from the same variant.
class ObstacleField {
public:
    ObstacleField() = default;

    bool Configure(const engine::assets::SceneAsset& scene, std::string* error);
    bool Configured() const { return configured_; }

    bool Select(std::size_t index);
    void CycleNext();
    std::size_t ActiveIndex() const { return active_; }
    const engine::assets::ObstacleVariant& ActiveVariant() const;

    const std::vector<engine::assets::ObstacleMarker>& Markers() const {
        return markers_;
    }
    const std::vector<engine::assets::ObstacleVariant>& Variants() const {
        return variants_;
    }

    void AddColliders(engine::collision::AabbCollisionWorld& world) const;
    std::size_t ColliderCellCount() const;
    std::size_t RenderInstanceCount() const;

private:
    std::vector<engine::assets::ObstacleVariant> variants_;
    std::vector<engine::assets::ObstacleMarker> markers_;
    std::size_t active_ = 0;
    bool configured_ = false;
};

} // namespace soulknight::game::room
