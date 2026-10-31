// Soul Knight Linux — sRGB/linear conversion for Vulkan vertex colors
#pragma once

#include "engine/math/Color.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace soulknight::engine::rendering {

inline uint8_t LinearChannel(uint8_t value) {
    const float srgb = static_cast<float>(value) / 255.0f;
    const float linear = srgb <= 0.04045f
        ? srgb / 12.92f
        : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
    return static_cast<uint8_t>(std::clamp(linear * 255.0f + 0.5f,
                                           0.0f, 255.0f));
}

// UI colors are authored as sRGB pixel values. Vulkan's sRGB swapchain expects
// linear vertex colors, so convert before submitting flat UI/panel/text quads.
inline Color LinearColor(const Color& color) {
    return Color{LinearChannel(color.r), LinearChannel(color.g),
                 LinearChannel(color.b), color.a};
}

} // namespace soulknight::engine::rendering
