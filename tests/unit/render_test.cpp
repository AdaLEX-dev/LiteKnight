// Soul Knight Linux — render-world sorting tests
#include <doctest/doctest.h>

#include "engine/rendering/SpriteBatch.hpp"

#include <cmath>

using soulknight::engine::Color;
using soulknight::engine::Rect;
using soulknight::engine::Vec2;
using soulknight::engine::rendering::SortLayer;
using soulknight::engine::rendering::SpriteBatch;

TEST_CASE("Sprite batch sorts by layer and preserves insertion order" * doctest::test_suite("unit")) {
    SpriteBatch batch;
    CHECK(batch.IsEmpty());

    CHECK(batch.DrawSprite({0, 0}, {10, 10}, Rect{0, 0, 1, 1}, Color::White(), SortLayer::UI));
    CHECK(batch.DrawSprite({1, 1}, {10, 10}, Rect{0, 0, 1, 1}, Color::Red(), SortLayer::Floor));
    CHECK(batch.DrawSprite({2, 2}, {10, 10}, Rect{0, 0, 1, 1}, Color::Green(), SortLayer::Character));
    CHECK(batch.DrawSprite({3, 3}, {10, 10}, Rect{0, 0, 1, 1}, Color::Blue(), SortLayer::Character));

    batch.Sort();
    const auto& draws = batch.GetDraws();
    CHECK(draws.size() == 4);
    if (draws.size() != 4) return;
    CHECK(draws[0].layer == SortLayer::Floor);
    CHECK(draws[0].color.ToUint32() == Color::Red().ToUint32());
    CHECK(draws[1].layer == SortLayer::Character);
    CHECK(draws[1].color.ToUint32() == Color::Green().ToUint32());
    CHECK(draws[2].layer == SortLayer::Character);
    CHECK(draws[2].color.ToUint32() == Color::Blue().ToUint32());
    CHECK(draws[3].layer == SortLayer::UI);

    batch.Clear();
    CHECK(batch.IsEmpty());
}

TEST_CASE("Sprite batch enforces its documented quad budget" * doctest::test_suite("unit")) {
    SpriteBatch batch;
    for (std::size_t i = 0; i < SpriteBatch::kMaxSprites; ++i) {
        (void)batch.DrawSprite({0, 0}, {1, 1}, Rect{0, 0, 1, 1},
                               Color::White(), SortLayer::BackGround);
    }
    CHECK_FALSE(batch.DrawSprite({0, 0}, {1, 1}, Rect{0, 0, 1, 1},
                                 Color::White(), SortLayer::BackGround));
    CHECK(batch.GetDraws().size() == SpriteBatch::kMaxSprites);
}

TEST_CASE("Sprite flips mirror geometry around the pivot" *
          doctest::test_suite("unit")) {
    soulknight::engine::rendering::SpriteDraw draw;
    draw.position = {10.0f, 10.0f};
    draw.size = {2.0f, 2.0f};
    draw.pivot = {0.6f, 0.4f};
    draw.uv = {0.0f, 0.0f, 1.0f, 1.0f};

    const auto plain = soulknight::engine::rendering::BuildSpriteQuad(draw);
    draw.flip_x = true;
    const auto mirrored = soulknight::engine::rendering::BuildSpriteQuad(draw);

    // Asymmetric pivots must stay anchored when facing changes: the mirrored
    // quad is the negated local geometry of the unflipped quad.
    CHECK(mirrored.positions[0].x == doctest::Approx(
        draw.position.x - (plain.positions[1].x - draw.position.x)));
    CHECK(mirrored.positions[1].x == doctest::Approx(
        draw.position.x + (draw.position.x - plain.positions[0].x)));
    // UVs still carry the texture mirror.
    CHECK(mirrored.uvs[0].x == doctest::Approx(1.0f));
    CHECK(mirrored.uvs[1].x == doctest::Approx(0.0f));

    draw.flip_x = false;
    draw.flip_y = true;
    const auto vertical = soulknight::engine::rendering::BuildSpriteQuad(draw);
    CHECK(vertical.positions[0].y == doctest::Approx(
        draw.position.y - (plain.positions[2].y - draw.position.y)));
    CHECK(vertical.positions[2].y == doctest::Approx(
        draw.position.y + (draw.position.y - plain.positions[0].y)));
}
