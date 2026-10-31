// Soul Knight Linux — Math: Vec3
#pragma once

#include <cmath>
#include "engine/core/Types.hpp"

namespace soulknight::engine {

struct Vec3 {
    f32 x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    constexpr Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    constexpr Vec3 operator*(f32 scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    constexpr Vec3 operator/(f32 scalar) const { return {x / scalar, y / scalar, z / scalar}; }

    constexpr f32 LengthSquared() const { return x*x + y*y + z*z; }
    f32 Length() const { return std::sqrt(LengthSquared()); }

    void Normalize() {
        f32 len = Length();
        if (len > 0.0f) { x /= len; y /= len; z /= len; }
    }

    constexpr f32 Dot(const Vec3& other) const { return x*other.x + y*other.y + z*other.z; }
    Vec3 Cross(const Vec3& other) const {
        return {y*other.z - z*other.y, z*other.x - x*other.z, x*other.y - y*other.x};
    }
};

} // namespace soulknight::engine