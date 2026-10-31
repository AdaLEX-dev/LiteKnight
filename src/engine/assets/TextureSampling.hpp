// Soul Knight Linux — texture sampling metadata
#pragma once

#include <cstdint>

namespace soulknight::engine::assets {

// Unity FilterMode values: Point=0, Bilinear=1, Trilinear=2.
enum class TextureFilter : uint8_t {
    Point = 0,
    Bilinear = 1,
    Trilinear = 2
};

// Unity TextureWrapMode values: Repeat=0, Clamp=1, Mirror=2.
enum class TextureWrap : uint8_t {
    Repeat = 0,
    Clamp = 1,
    Mirror = 2
};

} // namespace soulknight::engine::assets
