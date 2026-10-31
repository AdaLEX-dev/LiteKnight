#include "LogicalViewport.hpp"

#include <algorithm>

namespace soulknight::engine::rendering {
namespace {

struct Letterbox {
    float scale = 1.0f;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

Letterbox Fit(int width, int height) {
    Letterbox box;
    if (width <= 0 || height <= 0) return box;
    const float sx = static_cast<float>(width) /
                     static_cast<float>(LogicalViewport::kLogicalWidth);
    const float sy = static_cast<float>(height) /
                     static_cast<float>(LogicalViewport::kLogicalHeight);
    box.scale = std::min(sx, sy);
    // Prefer a crisp integer scale when the display can fit at least one full
    // logical screen. Otherwise fall back to aspect-preserving fractional fit.
    if (box.scale >= 1.0f) {
        box.scale = std::floor(box.scale);
    }
    box.width = static_cast<int>(static_cast<float>(LogicalViewport::kLogicalWidth) * box.scale);
    box.height = static_cast<int>(static_cast<float>(LogicalViewport::kLogicalHeight) * box.scale);
    box.x = (width - box.width) / 2;
    box.y = (height - box.height) / 2;
    return box;
}

Vec2 MapPoint(const Vec2& point, const Letterbox& box, int width, int height) {
    if (width <= 0 || height <= 0 || box.scale <= 0.0f) return {};
    const float x = (point.x - static_cast<float>(box.x)) / box.scale;
    const float y = (point.y - static_cast<float>(box.y)) / box.scale;
    return {
        std::clamp(x, 0.0f, static_cast<float>(LogicalViewport::kLogicalWidth)),
        std::clamp(y, 0.0f, static_cast<float>(LogicalViewport::kLogicalHeight))
    };
}

} // namespace

void LogicalViewport::Update(int fb_width, int fb_height, int win_width,
                             int win_height) {
    framebuffer_width = std::max(1, fb_width);
    framebuffer_height = std::max(1, fb_height);
    window_width = std::max(1, win_width);
    window_height = std::max(1, win_height);
    const Letterbox fb = Fit(framebuffer_width, framebuffer_height);
    framebuffer_scale = fb.scale;
    framebuffer_x = fb.x;
    framebuffer_y = fb.y;
    framebuffer_viewport_width = fb.width;
    framebuffer_viewport_height = fb.height;
    const Letterbox win = Fit(window_width, window_height);
    window_scale = win.scale;
    window_x = win.x;
    window_y = win.y;
    window_viewport_width = win.width;
    window_viewport_height = win.height;
}

Vec2 LogicalViewport::WindowPointToLogical(const Vec2& point) const {
    return MapPoint(point, Letterbox{window_scale, window_x, window_y,
                                     window_viewport_width,
                                     window_viewport_height},
                    window_width, window_height);
}

Vec2 LogicalViewport::FramebufferPointToLogical(const Vec2& point) const {
    return MapPoint(point, Letterbox{framebuffer_scale, framebuffer_x,
                                     framebuffer_y,
                                     framebuffer_viewport_width,
                                     framebuffer_viewport_height},
                    framebuffer_width, framebuffer_height);
}

} // namespace soulknight::engine::rendering
