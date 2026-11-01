// Soul Knight Linux — recovered obstacle variants for the dev room
#include "ObstacleField.hpp"

#include <utility>

namespace soulknight::game::room {

bool ObstacleField::Configure(const engine::assets::SceneAsset& scene,
                              std::string* error) {
    variants_.clear();
    markers_ = scene.obstacle_markers;
    active_ = 0;
    dev_override_ = false;
    configured_ = false;
    alive_.clear();
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
    alive_.assign(markers_.size(), true);
    configured_ = true;
    return true;
}

bool ObstacleField::Select(std::size_t index) {
    if (index >= variants_.size()) return false;
    active_ = index;
    dev_override_ = true;
    return true;
}

void ObstacleField::CycleNext() {
    if (variants_.empty()) return;
    active_ = (active_ + 1) % variants_.size();
    dev_override_ = true;
}

const engine::assets::ObstacleVariant& ObstacleField::ActiveVariant() const {
    return variants_[active_];
}

const engine::assets::ObstacleVariant& ObstacleField::VariantForMarker(
        const engine::assets::ObstacleMarker& marker) const {
    if (dev_override_) return ActiveVariant();
    if (marker.variant < 0 ||
        static_cast<std::size_t>(marker.variant) >= variants_.size()) {
        return ActiveVariant();
    }
    return variants_[static_cast<std::size_t>(marker.variant)];
}

bool ObstacleField::MarkerAlive(std::size_t index) const {
    return index < alive_.size() && alive_[index];
}

void ObstacleField::DestroyMarker(std::size_t index) {
    if (index < alive_.size()) alive_[index] = false;
}

void ObstacleField::AddColliders(
    engine::collision::AabbCollisionWorld& world) const {
    if (variants_.empty()) return;
    for (std::size_t i = 0; i < markers_.size(); ++i) {
        if (!MarkerAlive(i)) continue;
        const engine::assets::ObstacleVariant& variant =
            VariantForMarker(markers_[i]);
        if (!variant.has_collider || variant.collider_trigger) continue;
        world.AddSolid(engine::collision::Aabb{
            {markers_[i].position.x + variant.collider.min.x,
             markers_[i].position.y + variant.collider.min.y},
            {markers_[i].position.x + variant.collider.max.x,
             markers_[i].position.y + variant.collider.max.y}
        });
    }
}

std::size_t ObstacleField::ColliderCellCount() const {
    if (variants_.empty()) return 0;
    std::size_t count = 0;
    for (std::size_t i = 0; i < markers_.size(); ++i) {
        if (!MarkerAlive(i)) continue;
        const engine::assets::ObstacleVariant& variant =
            VariantForMarker(markers_[i]);
        if (variant.has_collider && !variant.collider_trigger) ++count;
    }
    return count;
}

std::size_t ObstacleField::RenderInstanceCount() const {
    if (variants_.empty()) return 0;
    std::size_t count = 0;
    for (std::size_t i = 0; i < markers_.size(); ++i) {
        if (!MarkerAlive(i)) continue;
        count += VariantForMarker(markers_[i]).visuals.size();
    }
    return count;
}

} // namespace soulknight::game::room
