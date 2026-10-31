// Soul Knight Linux — Camera
#pragma once

#include "engine/math/Vec2.hpp"
#include "engine/math/Mat4.hpp"

namespace soulknight::engine {

struct Camera {
    Vec2 position = {0, 0};
    float zoom = 1.0f;
    float rotation = 0.0f;
    float shake_intensity = 0.0f;
    float shake_timer = 0.0f;
    Vec2 target_position = {0, 0};
    float follow_speed = 10.0f;

    Mat4 ViewProjection() const {
        float half_w = 960.0f / zoom;
        float half_h = 540.0f / zoom;

        Mat4 proj = Mat4::Orthographic(-half_w, half_w, -half_h, half_h, -1000, 1000);
        Mat4 view = Mat4::Translate({-position.x, -position.y, 0}) * Mat4::RotateZ(rotation);

        if (shake_timer > 0) {
            // Simplified shake - in real impl use random
            view = Mat4::Translate({shake_intensity * 0.5f, shake_intensity * 0.5f, 0}) * view;
        }

        return proj * view;
    }

    void Follow(Vec2 target, float lerp_speed = 10.0f) {
        target_position = target;
        follow_speed = lerp_speed;
    }

    void Shake(float intensity, float duration) {
        shake_intensity = intensity;
        shake_timer = duration;
    }

    void Update(float dt) {
        // Smooth follow
        if (target_position != position) {
            position = position + (target_position - position) * (1.0f - std::exp(-follow_speed * dt));
        }

        if (shake_timer > 0) {
            shake_timer -= dt;
            if (shake_timer < 0) shake_timer = 0;
        }
    }
};

} // namespace soulknight::engine