// Soul Knight Linux — sprite, scene, camera and geometry tests
#include <doctest/doctest.h>

#include "engine/assets/SceneLoader.hpp"
#include "engine/assets/SpriteData.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/SpriteBatch.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <cmath>
#include <string>

using soulknight::engine::Camera2D;
using soulknight::engine::Color;
using soulknight::engine::Rect;
using soulknight::engine::Vec2;
using soulknight::engine::assets::SceneAsset;
using soulknight::engine::assets::SceneLoader;
using soulknight::engine::assets::SpriteData;
using soulknight::engine::rendering::BuildSpriteQuad;
using soulknight::engine::rendering::SortLayer;
using soulknight::engine::rendering::SpriteDraw;
using soulknight::engine::rendering::SpriteQuad;
using soulknight::platform::FileSystem;

TEST_CASE("SpriteData validates rect, UV, pivot, PPU and border" * doctest::test_suite("unit")) {
    SpriteData sprite;
    sprite.id = "test";
    sprite.texture_id = 3;
    sprite.texture_width = 256;
    sprite.texture_height = 128;
    sprite.rect = {64.0f, 16.0f, 32.0f, 24.0f};
    sprite.pivot = {0.25f, 0.75f};
    sprite.pixels_per_unit = 16.0f;
    sprite.border = {1.0f, 2.0f, 3.0f, 4.0f};

    CHECK(sprite.IsValid());
    CHECK(sprite.UvRect().x == doctest::Approx(0.25f));
    CHECK(sprite.UvRect().y == doctest::Approx(0.125f));
    CHECK(sprite.UvRect().w == doctest::Approx(0.125f));
    CHECK(sprite.UvRect().h == doctest::Approx(0.1875f));
    CHECK(sprite.WorldSize().x == doctest::Approx(2.0f));
    CHECK(sprite.WorldSize().y == doctest::Approx(1.5f));
    CHECK(sprite.border[0] == doctest::Approx(1.0f));
    CHECK(sprite.border[3] == doctest::Approx(4.0f));

    sprite.pivot = {1.5f, 0.5f};
    CHECK_FALSE(sprite.IsValid());
    sprite.pivot = {0.5f, 0.5f};
    sprite.rect = {240.0f, 0.0f, 32.0f, 24.0f};
    CHECK_FALSE(sprite.IsValid());
    sprite.rect = {0.0f, 100.0f, 32.0f, 40.0f};
    CHECK_FALSE(sprite.IsValid());
}

TEST_CASE("Sprite geometry handles pivot, crop, rotation, flips and determinism" * doctest::test_suite("unit")) {
    SpriteDraw draw;
    draw.position = {5.0f, 5.0f};
    draw.size = {2.0f, 2.0f};
    draw.uv = {0.5f, 0.1f, 0.25f, 0.2f};
    draw.pivot = {0.5f, 0.5f};
    draw.color = Color::White();
    draw.layer = SortLayer::Character;

    const SpriteQuad center = BuildSpriteQuad(draw);
    CHECK(center.positions[0].x == doctest::Approx(4.0f));
    CHECK(center.positions[0].y == doctest::Approx(4.0f));
    CHECK(center.positions[1].x == doctest::Approx(6.0f));
    CHECK(center.positions[2].y == doctest::Approx(6.0f));
    CHECK(center.uvs[0].x == doctest::Approx(0.5f));
    CHECK(center.uvs[0].y == doctest::Approx(0.3f));
    CHECK(center.uvs[2].x == doctest::Approx(0.75f));
    CHECK(center.uvs[2].y == doctest::Approx(0.1f));

    draw.pivot = {0.0f, 0.0f};
    const SpriteQuad corner = BuildSpriteQuad(draw);
    CHECK(corner.positions[0].x == doctest::Approx(5.0f));
    CHECK(corner.positions[0].y == doctest::Approx(5.0f));
    CHECK(corner.positions[2].x == doctest::Approx(7.0f));
    CHECK(corner.positions[2].y == doctest::Approx(7.0f));

    draw.pivot = {0.5f, 0.5f};
    draw.rotation = 3.14159265358979323846f * 0.5f;
    const SpriteQuad rotated = BuildSpriteQuad(draw);
    CHECK(rotated.positions[0].x == doctest::Approx(6.0f));
    CHECK(rotated.positions[0].y == doctest::Approx(4.0f));
    CHECK(rotated.positions[2].x == doctest::Approx(4.0f));
    CHECK(rotated.positions[2].y == doctest::Approx(6.0f));

    draw.rotation = 0.0f;
    draw.flip_x = true;
    draw.flip_y = true;
    const SpriteQuad flipped = BuildSpriteQuad(draw);
    CHECK(flipped.uvs[0].x == doctest::Approx(0.75f));
    CHECK(flipped.uvs[0].y == doctest::Approx(0.1f));
    CHECK(flipped.uvs[3].x == doctest::Approx(0.75f));
    CHECK(flipped.uvs[3].y == doctest::Approx(0.3f));

    const SpriteQuad repeated = BuildSpriteQuad(draw);
    CHECK(repeated.positions == flipped.positions);
    CHECK(repeated.uvs == flipped.uvs);
}

TEST_CASE("Camera2D keeps world-to-screen and screen-to-world consistent" * doctest::test_suite("unit")) {
    Camera2D camera;
    camera.position = {20.0f, 20.0f};
    camera.orthographic_size = 10.0f;
    camera.viewport_width = 1920.0f;
    camera.viewport_height = 1080.0f;

    CHECK(camera.Aspect() == doctest::Approx(16.0f / 9.0f));
    CHECK(camera.HalfHeight() == doctest::Approx(10.0f));
    CHECK(camera.HalfWidth() == doctest::Approx(10.0f * 16.0f / 9.0f));

    const Vec2 center = camera.WorldToScreen({20.0f, 20.0f});
    CHECK(center.x == doctest::Approx(960.0f));
    CHECK(center.y == doctest::Approx(540.0f));
    const Vec2 ndc_center = camera.ViewProjection().TransformPoint({20.0f, 20.0f});
    CHECK(ndc_center.x == doctest::Approx(0.0f));
    CHECK(ndc_center.y == doctest::Approx(0.0f));

    const Vec2 world = {30.0f, 30.0f};
    const Vec2 screen = camera.WorldToScreen(world);
    CHECK(screen.x == doctest::Approx(1500.0f));
    CHECK(screen.y == doctest::Approx(0.0f));
    const Vec2 roundtrip = camera.ScreenToWorld(screen);
    CHECK(roundtrip.x == doctest::Approx(world.x));
    CHECK(roundtrip.y == doctest::Approx(world.y));

    camera.position = {21.0f, 19.0f};
    const Vec2 moved_center = camera.WorldToScreen(camera.position);
    CHECK(moved_center.x == doctest::Approx(960.0f));
    CHECK(moved_center.y == doctest::Approx(540.0f));

    camera.zoom = 2.0f;
    CHECK(camera.HalfHeight() == doctest::Approx(5.0f));
    CHECK(camera.HalfWidth() == doctest::Approx(5.0f * 16.0f / 9.0f));
}

TEST_CASE("SceneLoader reads native sprite and instance metadata" * doctest::test_suite("unit")) {
    FileSystem filesystem;
    CHECK(filesystem.Initialize());

    SceneAsset scene;
    std::string error;
    CHECK(SceneLoader::Load(filesystem, "test/scene_test.json", scene, &error));
    CHECK(error.empty());
    CHECK(scene.id == "synthetic_scene");
    CHECK(scene.textures.size() == 1);
    CHECK(scene.sprites.size() == 1);
    CHECK(scene.instances.size() == 1);
    CHECK(scene.textures[0].data.width == 4);
    CHECK(scene.textures[0].data.height == 4);
    CHECK(scene.textures[0].filter == soulknight::engine::assets::TextureFilter::Point);
    CHECK(scene.sprites[0].UvRect().w == doctest::Approx(1.0f));
    CHECK(scene.sprites[0].pixels_per_unit == doctest::Approx(4.0f));
    CHECK(scene.instances[0].sprite_index == 0);
    CHECK(scene.instances[0].position.x == doctest::Approx(1.0f));
    CHECK(scene.instances[0].position.y == doctest::Approx(2.0f));
    CHECK(scene.instances[0].color.g == 128);
    CHECK(scene.instances[0].sorting_layer == 5);
    CHECK(scene.instances[0].sorting_order == 2);
    CHECK(scene.instances[0].flip_x);

    SceneAsset missing;
    std::string missing_error;
    CHECK_FALSE(SceneLoader::Load(filesystem, "test/missing_scene.json", missing, &missing_error));
    CHECK_FALSE(missing_error.empty());
}
