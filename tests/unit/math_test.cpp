// Soul Knight Linux — Math Tests
#include <doctest/doctest.h>
#include "engine/math/Vec2.hpp"
#include "engine/math/Random.hpp"

using namespace soulknight::engine;

TEST_CASE("Vec2 basic operations" * doctest::test_suite("unit")) {
    Vec2 a{3.0f, 4.0f};
    Vec2 b{1.0f, 2.0f};

    SUBCASE("Addition") {
        CHECK((a + b) == Vec2{4.0f, 6.0f});
    }
    SUBCASE("Subtraction") {
        CHECK((a - b) == Vec2{2.0f, 2.0f});
    }
    SUBCASE("Scalar multiplication") {
        CHECK((a * 2.0f) == Vec2{6.0f, 8.0f});
    }
    SUBCASE("Scalar division") {
        CHECK((a / 2.0f) == Vec2{1.5f, 2.0f});
    }
    SUBCASE("Length") {
        CHECK(a.Length() == doctest::Approx(5.0f));
    }
    SUBCASE("Normalized") {
        Vec2 norm = a.Normalized();
        CHECK(norm.Length() == doctest::Approx(1.0f));
    }
    SUBCASE("Dot product") {
        CHECK(a.Dot(b) == 11.0f);
    }
    SUBCASE("Cross product") {
        CHECK(a.Cross(b) == 2.0f);
    }
    SUBCASE("Lerp") {
        CHECK(a.Lerp(b, 0.5f) == Vec2{2.0f, 3.0f});
    }
}

TEST_CASE("PCG32 utility range and bool" * doctest::test_suite("unit")) {
    Random rng(12345);

    SUBCASE("UInt32 generates values") {
        uint32_t v1 = rng.UInt32();
        uint32_t v2 = rng.UInt32();
        CHECK(v1 != v2);
    }

    SUBCASE("Range generates in bounds") {
        for (int i = 0; i < 100; ++i) {
            int v = rng.Range(0, 10);
            CHECK(v >= 0);
            CHECK(v <= 10);
        }
    }

    SUBCASE("Float in range") {
        for (int i = 0; i < 100; ++i) {
            float v = rng.Float(0.0f, 1.0f);
            CHECK(v >= 0.0f);
            CHECK(v < 1.0f);
        }
    }

    SUBCASE("Bool probability") {
        int true_count = 0;
        for (int i = 0; i < 10000; ++i) {
            if (rng.Bool(0.3f)) true_count++;
        }
        // Should be around 3000 (within 2%)
        CHECK(true_count > 2800);
        CHECK(true_count < 3200);
    }

    SUBCASE("Deterministic with same seed") {
        Random rng1(42);
        Random rng2(42);
        for (int i = 0; i < 100; ++i) {
            CHECK(rng1.UInt32() == rng2.UInt32());
        }
    }
}

TEST_CASE("PCG32 utility vector sampling" * doctest::test_suite("unit")) {
    Random rng(12345);

    SUBCASE("Unit vector has length 1") {
        for (int i = 0; i < 100; ++i) {
            Vec2 v = rng.Vec2Unit();
            CHECK(v.Length() == doctest::Approx(1.0f));
        }
    }

    SUBCASE("Vec2InCircle within radius") {
        for (int i = 0; i < 100; ++i) {
            Vec2 v = rng.Vec2InCircle(5.0f);
            CHECK(v.Length() <= 5.0f + 0.001f);
        }
    }
}