// Soul Knight Linux — Math: Vec2
#pragma once

#include <cmath>
#include <engine/core/Types.hpp>

namespace soulknight::engine {

struct Vec2 {
    f32 x = 0.0f, y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(f32 x_, f32 y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
    constexpr Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
    constexpr Vec2 operator*(f32 scalar) const { return {x * scalar, y * scalar}; }
    constexpr Vec2 operator/(f32 scalar) const { return {x / scalar, y / scalar}; }

    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(f32 scalar) { x *= scalar; y *= scalar; return *this; }
    Vec2& operator/=(f32 scalar) { x /= scalar; y /= scalar; return *this; }

    constexpr bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
    constexpr bool operator!=(const Vec2& other) const { return !(*this == other); }

    constexpr f32 LengthSquared() const { return x*x + y*y; }
    f32 Length() const { return std::sqrt(LengthSquared()); }
    f32 Distance(const Vec2& other) const { return (*this - other).Length(); }

    Vec2 Normalized() const {
        f32 len = Length();
        return len > 0.0f ? Vec2{x/len, y/len} : Vec2{0, 0};
    }

    void Normalize() {
        f32 len = Length();
        if (len > 0.0f) { x /= len; y /= len; }
    }

    constexpr f32 Dot(const Vec2& other) const { return x*other.x + y*other.y; }
    constexpr f32 Cross(const Vec2& other) const { return x*other.y - y*other.x; }

    Vec2 Lerp(const Vec2& other, f32 t) const {
        return *this + (other - *this) * t;
    }

    static constexpr Vec2 Zero() { return {0, 0}; }
    static constexpr Vec2 One() { return {1, 1}; }
    static constexpr Vec2 Up() { return {0, -1}; }
    static constexpr Vec2 Down() { return {0, 1}; }
    static constexpr Vec2 Left() { return {-1, 0}; }
    static constexpr Vec2 Right() { return {1, 0}; }
};

} // namespace soulknight::engine