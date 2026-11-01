// Soul Knight Linux — recovered obstacle variants for the dev room
#include "ObstacleField.hpp"

#include <utility>

namespace soulknight::game::room {

bool ObstacleField::Configure(const engine::assets::SceneAsset& scene,
                              std::string* error) {
    variants_.clear();
    markers_ = scene.obstacle_markers;
    active_ = 0;
    configured_ = false;
    if (error != nullptr) error->clear();

    if (scene.obstacle_variants.empty() && scene.obstacle_markers.empty()) {
        configured_ = true;
        return true;
    }
    if (scene.obstacle_variants.empty()) {
        if (error != nullptr) *error = "obstacle markers have no variants";
        return false;
    }
    if (scene.obstacle_markers.empty()) {
        if (error != nullptr) *error = "obstacle variants have no markers";
        return false;
    }

    // The dev obstacle switcher intentionally exposes only recovered prefabs
    // with a solid BoxCollider2D. Decorative obstacle_list entries without a
    // collider (sting01, speed_up, speed_down, signs01) remain in the scene
    // metadata but cannot satisfy the rendered-cell == collision-cell test.
    std::vector<int32_t> original_to_selectable(
        scene.obstacle_variants.size(), -1);
    for (std::size_t i = 0; i < scene.obstacle_variants.size(); ++i) {
        const engine::assets::ObstacleVariant& variant =
            scene.obstacle_variants[i];
        if (!variant.has_collider || variant.collider_trigger) continue;
        if (variant.visuals.empty()) {
            if (error != nullptr) {
                *error = "solid obstacle variant has no visuals: " + variant.name;
            }
            return false;
        }
        original_to_selectable[i] = static_cast<int32_t>(variants_.size());
        variants_.push_back(variant);
    }
    if (variants_.empty()) {
        if (error != nullptr) *error = "no solid obstacle variants are available";
        return false;
    }

    // Real room scenes also contain decorative obstacle_list entries (for
    // example r1_2/r1_3 skin_trap props) without a solid collider. They are
    // still rendered from the scene instances, but cannot participate in the
    // dev obstacle switcher's rendered-cell == collision-cell contract.
    std::vector<engine::assets::ObstacleMarker> solid_markers;
    solid_markers.reserve(markers_.size());
    for (engine::assets::ObstacleMarker& marker : markers_) {
        if (marker.variant < 0 ||
            static_cast<std::size_t>(marker.variant) >=
                original_to_selectable.size()) {
            if (error != nullptr) *error = "obstacle marker variant is invalid";
            return false;
        }
        const int32_t selectable =
            original_to_selectable[static_cast<std::size_t>(marker.variant)];
        if (selectable < 0) continue;
        marker.variant = selectable;
        solid_markers.push_back(marker);
    }
    markers_ = std::move(solid_markers);
    if (markers_.empty()) {
        // A room made only of decorative props is valid; it simply contributes
        // no solid obstacle cells.
        configured_ = true;
        return true;
    }
    active_ = static_cast<std::size_t>(markers_.front().variant);
    configured_ = true;
    return true;
}

bool ObstacleField::Select(std::size_t index) {
    if (index >= variants_.size()) return false;
    active_ = index;
    return true;
}

void ObstacleField::CycleNext() {
    if (variants_.empty()) return;
    active_ = (active_ + 1) % variants_.size();
}

const engine::assets::ObstacleVariant& ObstacleField::ActiveVariant() const {
    return variants_[active_];
}

void ObstacleField::AddColliders(
    engine::collision::AabbCollisionWorld& world) const {
    if (variants_.empty()) return;
    const engine::assets::ObstacleVariant& variant = ActiveVariant();
    if (!variant.has_collider || variant.collider_trigger) return;
    for (const engine::assets::ObstacleMarker& marker : markers_) {
        world.AddSolid(engine::collision::Aabb{
            {marker.position.x + variant.collider.min.x,
             marker.position.y + variant.collider.min.y},
            {marker.position.x + variant.collider.max.x,
             marker.position.y + variant.collider.max.y}
        });
    }
}

std::size_t ObstacleField::ColliderCellCount() const {
    if (variants_.empty()) return 0;
    const engine::assets::ObstacleVariant& variant = ActiveVariant();
    if (!variant.has_collider || variant.collider_trigger) return 0;
    return markers_.size();
}

std::size_t ObstacleField::RenderInstanceCount() const {
    if (variants_.empty()) return 0;
    return markers_.size() * ActiveVariant().visuals.size();
}

} // namespace soulknight::game::room
