// Soul Knight Linux — deterministic 2D AABB collision implementation
#include "AabbCollision.hpp"

#include <algorithm>

namespace soulknight::engine::collision {

void AabbCollisionWorld::SetBounds(const Aabb& bounds) {
    bounds_ = bounds;
    has_bounds_ = bounds.IsValid();
}

void AabbCollisionWorld::ClearSolids() {
    solids_.clear();
}

void AabbCollisionWorld::AddSolid(const Aabb& solid) {
    if (solid.IsValid()) solids_.push_back(solid);
}

Aabb AabbCollisionWorld::MakeBox(const Vec2& center, const Vec2& half_extents) {
    return Aabb{
        {center.x - half_extents.x, center.y - half_extents.y},
        {center.x + half_extents.x, center.y + half_extents.y}
    };
}

CollisionResult AabbCollisionWorld::Move(const Vec2& position,
                                          const Vec2& half_extents,
                                          const Vec2& delta,
                                          Vec2& out_position) const {
    CollisionResult result;
    Vec2 center = position;

    // X axis: sweep against the old box so a large move cannot pass through a
    // thin wall. Normal player moves are ~0.2 units per 50 Hz tick, but the
    // explicit swept check also makes the collision contract testable.
    {
        const Aabb start_box = MakeBox(center, half_extents);
        Vec2 target = center;
        target.x += delta.x;
        const Aabb target_box = MakeBox(target, half_extents);

        if (has_bounds_) {
            if (target_box.min.x < bounds_.min.x) {
                target.x = bounds_.min.x + half_extents.x;
                result.blocked_x = true;
            } else if (target_box.max.x > bounds_.max.x) {
                target.x = bounds_.max.x - half_extents.x;
                result.blocked_x = true;
            }
        }

        for (const Aabb& solid : solids_) {
            const bool vertical_overlap = start_box.min.y < solid.max.y &&
                                          start_box.max.y > solid.min.y;
            if (!vertical_overlap) continue;
            if (delta.x > 0.0f && start_box.max.x <= solid.min.x &&
                target_box.max.x > solid.min.x) {
                target.x = std::min(target.x, solid.min.x - half_extents.x);
                result.blocked_x = true;
            } else if (delta.x < 0.0f && start_box.min.x >= solid.max.x &&
                       target_box.min.x < solid.max.x) {
                target.x = std::max(target.x, solid.max.x + half_extents.x);
                result.blocked_x = true;
            }
        }
        center.x = target.x;
    }

    // Y axis, using the already-resolved X position. This is what produces
    // wall-sliding instead of a full stop.
    {
        const Aabb start_box = MakeBox(center, half_extents);
        Vec2 target = center;
        target.y += delta.y;
        const Aabb target_box = MakeBox(target, half_extents);

        if (has_bounds_) {
            if (target_box.min.y < bounds_.min.y) {
                target.y = bounds_.min.y + half_extents.y;
                result.blocked_y = true;
            } else if (target_box.max.y > bounds_.max.y) {
                target.y = bounds_.max.y - half_extents.y;
                result.blocked_y = true;
            }
        }

        for (const Aabb& solid : solids_) {
            const bool horizontal_overlap = start_box.min.x < solid.max.x &&
                                            start_box.max.x > solid.min.x;
            if (!horizontal_overlap) continue;
            if (delta.y > 0.0f && start_box.max.y <= solid.min.y &&
                target_box.max.y > solid.min.y) {
                target.y = std::min(target.y, solid.min.y - half_extents.y);
                result.blocked_y = true;
            } else if (delta.y < 0.0f && start_box.min.y >= solid.max.y &&
                       target_box.min.y < solid.max.y) {
                target.y = std::max(target.y, solid.max.y + half_extents.y);
                result.blocked_y = true;
            }
        }
        center.y = target.y;
    }

    out_position = center;
    return result;
}

} // namespace soulknight::engine::collision
