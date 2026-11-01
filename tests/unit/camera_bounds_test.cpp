// Soul Knight Linux — camera viewport/room boundary regression
#include <doctest/doctest.h>

#include "engine/rendering/Camera2D.hpp"

using soulknight::engine::Camera2D;
using soulknight::engine::Vec2;

TEST_CASE("Camera centres rooms smaller than the viewport" *
          doctest::test_suite("unit")) {
    Camera2D camera;
    camera.orthographic_size = 10.5f;
    camera.viewport_width = 1920.0f;
    camera.viewport_height = 1080.0f;
    camera.position = {100.0f, 100.0f};
    camera.ClampToRect({0.0f, 0.0f}, {15.0f, 15.0f});
    CHECK(camera.position.x == doctest::Approx(7.5f));
    CHECK(camera.position.y == doctest::Approx(7.5f));
}

TEST_CASE("Camera clamps to large hub and 21x21 room bounds" *
          doctest::test_suite("unit")) {
    Camera2D camera;
    camera.orthographic_size = 10.0f;
    camera.viewport_width = 1920.0f;
    camera.viewport_height = 1080.0f;
    camera.position = {-1000.0f, 1000.0f};
    camera.ClampToRect({-63.0f, -19.0f}, {-15.0f, 15.0f});
    CHECK(camera.position.x == doctest::Approx(-63.0f + camera.HalfWidth()));
    CHECK(camera.position.y == doctest::Approx(15.0f - camera.HalfHeight()));

    camera.position = {0.0f, 0.0f};
    camera.ClampToRect({0.0f, 0.0f}, {21.0f, 21.0f});
    CHECK(camera.position.x == doctest::Approx(10.5f));
    CHECK(camera.position.y == doctest::Approx(10.0f));
}
