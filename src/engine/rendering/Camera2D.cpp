// Soul Knight Linux — 2D orthographic camera implementation
#include "Camera2D.hpp"

namespace soulknight::engine {

Mat4 Camera2D::ViewProjection() const {
    const float half_w = HalfWidth();
    const float half_h = HalfHeight();
    // Vulkan NDC has Y down; invert the world Y axis so +Y is up on screen.
    const Mat4 projection = Mat4::Orthographic(
        -half_w, half_w, half_h, -half_h, -1.0f, 1.0f);
    const Mat4 view = Mat4::Translate({-position.x, -position.y, 0.0f});
    return projection * view;
}

Vec2 Camera2D::WorldToScreen(const Vec2& world) const {
    if (viewport_width <= 0.0f || viewport_height <= 0.0f) return {};
    const float half_w = HalfWidth();
    const float half_h = HalfHeight();
    const float delta_x = world.x - position.x;
    const float delta_y = world.y - position.y;
    return {
        viewport_width * 0.5f + (delta_x / half_w) * (viewport_width * 0.5f),
        viewport_height * 0.5f - (delta_y / half_h) * (viewport_height * 0.5f)
    };
}

Vec2 Camera2D::ScreenToWorld(const Vec2& screen) const {
    if (viewport_width <= 0.0f || viewport_height <= 0.0f) return {};
    const float half_w = HalfWidth();
    const float half_h = HalfHeight();
    const float ndc_x = (screen.x / viewport_width) * 2.0f - 1.0f;
    const float ndc_y = 1.0f - (screen.y / viewport_height) * 2.0f;
    return {position.x + ndc_x * half_w, position.y + ndc_y * half_h};
}

} // namespace soulknight::engine
