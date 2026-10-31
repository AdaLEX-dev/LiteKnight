// Soul Knight Linux — Y-depth sorting regression
#include <doctest/doctest.h>

#include "engine/rendering/WorldDepth.hpp"

using soulknight::engine::rendering::IsCharacterSortedUnityLayer;
using soulknight::engine::rendering::WorldDepthOrder;

TEST_CASE("World depth orders south content in front of north content" *
          doctest::test_suite("unit")) {
    CHECK(WorldDepthOrder(10.0f) > WorldDepthOrder(12.0f));
    CHECK(WorldDepthOrder(10.0f) < WorldDepthOrder(8.0f));
    CHECK(WorldDepthOrder(10.0f, 2) > WorldDepthOrder(10.0f, 1));
    CHECK(IsCharacterSortedUnityLayer(5));
    CHECK(IsCharacterSortedUnityLayer(6));
    CHECK_FALSE(IsCharacterSortedUnityLayer(3));
    CHECK_FALSE(IsCharacterSortedUnityLayer(7));
}
