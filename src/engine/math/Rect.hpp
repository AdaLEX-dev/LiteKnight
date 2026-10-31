// Soul Knight Linux — Math: Rect
#pragma once

#include "Vec2.hpp"

namespace soulknight::engine {

struct Rect {
    f32 x = 0, y = 0, w = 0, h = 0;

    constexpr Rect() = default;
    constexpr Rect(f32 x_, f32 y_, f32 w_, f32 h_) : x(x_), y(y_), w(w_), h(h_) {}

    constexpr f32 Left() const { return x; }
    constexpr f32 Right() const { return x + w; }
    constexpr f32 Top() const { return y; }
    constexpr f32 Bottom() const { return y + h; }
    constexpr f32 CenterX() const { return x + w * 0.5f; }
    constexpr f32 CenterY() const { return y + h * 0.5f; }
    constexpr Vec2 Center() const { return {CenterX(), CenterY()}; }
    constexpr Vec2 Size() const { return {w, h}; }

    constexpr bool Contains(const Vec2& point) const {
        return point.x >= x && point.x < x + w && point.y >= y && point.y < y + h;
    }

    constexpr bool Intersects(const Rect& other) const {
        return x < other.x + other.w && x + w > other.x &&
               y < other.y + other.h && y + h > other.y;
    }

    constexpr Rect Intersection(const Rect& other) const {
        f32 left = std::max(x, other.x);
        f32 top = std::max(y, other.y);
        f32 right = std::min(x + w, other.x + other.w);
        f32 bottom = std::min(y + h, other.y + other.h);

        if (left >= right || top >= bottom) return Rect{0, 0, 0, 0};
        return Rect{left, top, right - left, bottom - top};
    }

    static Rect FromCenter(const Vec2& center, const Vec2& size) {
        return Rect{center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y};
    }
};

} // namespace soulknight::engine