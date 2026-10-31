// Soul Knight Linux — deterministic 2D AABB collision
#pragma once

#include "engine/math/Vec2.hpp"

#include <vector>

namespace soulknight::engine::collision {

struct Aabb {
    Vec2 min{0.0f, 0.0f};
    Vec2 max{0.0f, 0.0f};

    constexpr bool IsValid() const {
        return max.x > min.x && max.y > min.y;
    }

    constexpr bool Intersects(const Aabb& other) const {
        return min.x < other.max.x && max.x > other.min.x &&
               min.y < other.max.y && max.y > other.min.y;
    }
};

struct CollisionResult {
    bool blocked_x = false;
    bool blocked_y = false;
};

// Axis-separated AABB movement. This is deliberately small: the first native
// locomotion slice needs sliding, no tunneling at player speed, and deterministic
// results, not a general physics engine.
class AabbCollisionWorld {
public:
    void SetBounds(const Aabb& bounds);
    void ClearSolids();
    void AddSolid(const Aabb& solid);

    const Aabb& Bounds() const { return bounds_; }
    bool HasBounds() const { return has_bounds_; }
    const std::vector<Aabb>& Solids() const { return solids_; }

    // Moves the centre of the body by delta and resolves each axis separately.
    // The resulting centre is written to out_position.
    CollisionResult Move(const Vec2& position, const Vec2& half_extents,
                         const Vec2& delta, Vec2& out_position) const;

private:
    static Aabb MakeBox(const Vec2& center, const Vec2& half_extents);

    Aabb bounds_{};
    bool has_bounds_ = false;
    std::vector<Aabb> solids_;
};

} // namespace soulknight::engine::collision
