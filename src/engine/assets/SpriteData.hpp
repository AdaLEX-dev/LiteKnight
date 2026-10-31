// Soul Knight Linux — native sprite metadata
#pragma once

#include "TextureSampling.hpp"
#include "engine/math/Rect.hpp"
#include "engine/math/Vec2.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace soulknight::engine::assets {

// Interprets a rectangular region of a TextureData. This is deliberately
// independent of Unity and of GPU handles.
//
// rect is a pixel rectangle in the texture with TOP-LEFT origin (native
// convention). pivot is normalized within rect, with (0,0) at its bottom-left.
// pixels_per_unit converts rect pixels to world units.
struct SpriteData {
    std::string id;
    uint32_t texture_id = 0;
    engine::Rect rect{0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t texture_width = 0;
    uint32_t texture_height = 0;
    engine::Vec2 pivot{0.5f, 0.5f};
    engine::Vec2 source_pivot{0.5f, 0.5f};
    engine::Vec2 texture_rect_offset{0.0f, 0.0f};
    float pixels_per_unit = 16.0f;
    std::array<float, 4> border{0.0f, 0.0f, 0.0f, 0.0f};
    TextureFilter filter = TextureFilter::Bilinear;
    TextureWrap wrap = TextureWrap::Clamp;

    bool IsValid() const {
        if (id.empty() || texture_width == 0 || texture_height == 0 ||
            pixels_per_unit <= 0.0f) {
            return false;
        }
        if (rect.w <= 0.0f || rect.h <= 0.0f) return false;
        if (rect.x < 0.0f || rect.y < 0.0f) return false;
        if (rect.x + rect.w > static_cast<float>(texture_width) ||
            rect.y + rect.h > static_cast<float>(texture_height)) {
            return false;
        }
        if (pivot.x < 0.0f || pivot.x > 1.0f ||
            pivot.y < 0.0f || pivot.y > 1.0f) {
            return false;
        }
        return true;
    }

    // Normalized top-left UV rectangle for the existing sprite shader.
    engine::Rect UvRect() const {
        if (texture_width == 0 || texture_height == 0) return {};
        return engine::Rect{
            rect.x / static_cast<float>(texture_width),
            rect.y / static_cast<float>(texture_height),
            rect.w / static_cast<float>(texture_width),
            rect.h / static_cast<float>(texture_height)
        };
    }

    engine::Vec2 WorldSize() const {
        if (pixels_per_unit <= 0.0f) return {};
        return {rect.w / pixels_per_unit, rect.h / pixels_per_unit};
    }
};

} // namespace soulknight::engine::assets
