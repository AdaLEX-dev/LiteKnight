// Soul Knight Linux — Math: Color
#pragma once

#include <cstdint>
#include "engine/core/Types.hpp"

namespace soulknight::engine {

struct Color {
    uint8_t r = 255, g = 255, b = 255, a = 255;

    constexpr Color() = default;
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    constexpr Color(uint32_t rgba) {
        r = static_cast<uint8_t>((rgba >> 24) & 0xFFU);
        g = static_cast<uint8_t>((rgba >> 16) & 0xFFU);
        b = static_cast<uint8_t>((rgba >> 8) & 0xFFU);
        a = static_cast<uint8_t>(rgba & 0xFFU);
    }

    constexpr uint32_t ToUint32() const {
        return (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | uint32_t(a);
    }

    static constexpr Color White() { return {255, 255, 255, 255}; }
    static constexpr Color Black() { return {0, 0, 0, 255}; }
    static constexpr Color Red() { return {255, 0, 0, 255}; }
    static constexpr Color Green() { return {0, 255, 0, 255}; }
    static constexpr Color Blue() { return {0, 0, 255, 255}; }
    static constexpr Color Yellow() { return {255, 255, 0, 255}; }
    static constexpr Color Magenta() { return {255, 0, 255, 255}; }
    static constexpr Color Cyan() { return {0, 255, 255, 255}; }
    static constexpr Color Transparent() { return {0, 0, 0, 0}; }
};

using Color32 = Color;

} // namespace soulknight::engine