// Soul Knight Linux — render-world sorting tests
#include <doctest/doctest.h>

#include "engine/rendering/SpriteBatch.hpp"

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
