// Soul Knight Linux — canonical CPU-side texture representation
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace soulknight::engine::assets {

// Canonical decoded texture: tightly packed 8-bit RGBA, top row first,
// byte order R,G,B,A. The vector owns the pixels; moves are cheap.
struct TextureData {
    static constexpr uint32_t kBytesPerPixel = 4;

    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba8;

    std::size_t ExpectedByteSize() const {
        constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
        const std::size_t w = static_cast<std::size_t>(width);
        const std::size_t h = static_cast<std::size_t>(height);
        if (w != 0 && h > max_size / w) return 0;
        const std::size_t pixels = w * h;
        if (pixels > max_size / kBytesPerPixel) return 0;
        return pixels * kBytesPerPixel;
    }

    bool IsValid() const {
        return width > 0 && height > 0 && !rgba8.empty() &&
               rgba8.size() == ExpectedByteSize();
    }

    void Clear() {
        width = 0;
        height = 0;
        rgba8.clear();
    }
};

} // namespace soulknight::engine::assets
