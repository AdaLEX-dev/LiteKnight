// Soul Knight Linux — RNG Tests
#include <doctest/doctest.h>
#include "engine/math/Random.hpp"

using namespace soulknight::engine;

TEST_CASE("PCG32 utility determinism" * doctest::test_suite("unit")) {
    PCG32 rng1(12345);
    PCG32 rng2(12345);

    CHECK(rng1.Next() == rng2.Next());
    CHECK(rng1.Next() == rng2.Next());

    PCG32 rng3(54321);
    CHECK(rng3.Next() != rng1.Next());
}

TEST_CASE("PCG32 utility wrapper" * doctest::test_suite("unit")) {
    Random rng(12345);

    SUBCASE("UInt32") {
        uint32_t v1 = rng.UInt32();
        uint32_t v2 = rng.UInt32();
        CHECK(v1 != v2);
    }

    SUBCASE("Range") {
        for (int i = 0; i < 100; ++i) {
            int v = rng.Range(5, 15);
            CHECK(v >= 5);
            CHECK(v <= 15);
        }
    }

    SUBCASE("Float") {
        for (int i = 0; i < 100; ++i) {
            float v = rng.Float();
            CHECK(v >= 0.0f);
            CHECK(v < 1.0f);
        }
    }

    SUBCASE("Bool probability") {
        int true_count = 0;
        for (int i = 0; i < 10000; ++i) {
            if (rng.Bool(0.3f)) true_count++;
        }
        CHECK(true_count > 2800);
        CHECK(true_count < 3200);
    }
}