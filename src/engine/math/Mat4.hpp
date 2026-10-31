// Soul Knight Linux — Math: Mat4
#pragma once

#include <cmath>
#include <array>
#include "Vec2.hpp"
#include "Vec3.hpp"

namespace soulknight::engine {

struct Mat4 {
    float m[16];

    constexpr Mat4() : m{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1} {}

    static constexpr Mat4 Identity() { return Mat4(); }

    static Mat4 Orthographic(float left, float right, float bottom, float top, float near, float far) {
        Mat4 m;
        float rl = right - left;
        float tb = top - bottom;
        float fn = far - near;

        m.m[0] = 2.0f / rl; m.m[4] = 0; m.m[8] = 0; m.m[12] = -(right + left) / rl;
        m.m[1] = 0; m.m[5] = 2.0f / tb; m.m[9] = 0; m.m[13] = -(top + bottom) / tb;
        m.m[2] = 0; m.m[6] = 0; m.m[10] = -2.0f / fn; m.m[14] = -(far + near) / fn;
        m.m[3] = 0; m.m[7] = 0; m.m[11] = 0; m.m[15] = 1;

        return m;
    }

    static Mat4 Translate(const Vec2& v) {
        Mat4 m = Identity();
        m.m[12] = v.x;
        m.m[13] = v.y;
        return m;
    }

    static Mat4 Translate(const Vec3& v) {
        Mat4 m = Identity();
        m.m[12] = v.x;
        m.m[13] = v.y;
        m.m[14] = v.z;
        return m;
    }

    static Mat4 RotateZ(float angle) {
        Mat4 m = Identity();
        float c = std::cos(angle);
        float s = std::sin(angle);
        m.m[0] = c; m.m[4] = -s;
        m.m[1] = s; m.m[5] = c;
        return m;
    }

    Mat4 operator*(const Mat4& other) const {
        Mat4 result;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                result.m[row * 4 + col] = 0;
                for (int k = 0; k < 4; ++k) {
                    result.m[row * 4 + col] += m[row * 4 + k] * other.m[k * 4 + col];
                }
            }
        }
        return result;
    }

    Vec2 TransformPoint(const Vec2& v) const {
        float x = m[0] * v.x + m[4] * v.y + m[12];
        float y = m[1] * v.x + m[5] * v.y + m[13];
        return {x, y};
    }

    Vec2 TransformVector(const Vec2& v) const {
        float x = m[0] * v.x + m[4] * v.y;
        float y = m[1] * v.x + m[5] * v.y;
        return {x, y};
    }
};

} // namespace soulknight::engine