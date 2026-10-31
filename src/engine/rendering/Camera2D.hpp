// Soul Knight Linux — 2D orthographic camera
#pragma once

#include "engine/math/Mat4.hpp"
#include "engine/math/Vec2.hpp"

namespace soulknight::engine {

// World space is Y-up. The camera position is the center of the view.
// orthographic_size is the half-height in world units (Unity Camera convention).
struct Camera2D {
    Vec2 position{0.0f, 0.0f};
    float orthographic_size = 10.0f;
    float zoom = 1.0f;
    float viewport_width = 1920.0f;
    float viewport_height = 1080.0f;

    // NATIVE DESIGN: deterministic smoothing rate used by the locomotion slice.
    // The reference camera follow values have not been recovered.
    float follow_smoothing = 8.0f;

    float Aspect() const {
        return viewport_height > 0.0f ? viewport_width / viewport_height : 1.0f;
    }

    float HalfHeight() const {
        return orthographic_size / (zoom > 0.0f ? zoom : 1.0f);
    }

    float HalfWidth() const {
        return HalfHeight() * Aspect();
    }

    Mat4 ViewProjection() const;
    void Follow(const Vec2& target, float delta_seconds);
    Vec2 WorldToScreen(const Vec2& world) const;
    Vec2 ScreenToWorld(const Vec2& screen) const;
};

} // namespace soulknight::engine
