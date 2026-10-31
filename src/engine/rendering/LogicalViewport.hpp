// Soul Knight Linux — 16:9 logical presentation contract
#pragma once

#include "engine/math/Vec2.hpp"

namespace soulknight::engine::rendering {

// Logical UI/world design space is always 1920x1080. The framebuffer and window
// may be any size; content is uniformly scaled and letterboxed/pillarboxed.
struct LogicalViewport {
    static constexpr int kLogicalWidth = 1920;
    static constexpr int kLogicalHeight = 1080;

    int framebuffer_width = kLogicalWidth;
    int framebuffer_height = kLogicalHeight;
    int window_width = kLogicalWidth;
    int window_height = kLogicalHeight;

    float framebuffer_scale = 1.0f;
    float window_scale = 1.0f;
    int framebuffer_x = 0;
    int framebuffer_y = 0;
    int framebuffer_viewport_width = kLogicalWidth;
    int framebuffer_viewport_height = kLogicalHeight;
    int window_x = 0;
    int window_y = 0;
    int window_viewport_width = kLogicalWidth;
    int window_viewport_height = kLogicalHeight;

    void Update(int fb_width, int fb_height, int win_width, int win_height);
    Vec2 WindowPointToLogical(const Vec2& point) const;
    Vec2 FramebufferPointToLogical(const Vec2& point) const;

    float Aspect() const {
        return static_cast<float>(kLogicalWidth) /
               static_cast<float>(kLogicalHeight);
    }
};

} // namespace soulknight::engine::rendering
