// Soul Knight Linux — reference-compatible Unity 2018.4 RNG tests
//
// Vectors are generated from the reconstructed algorithm in
// analysis/re/rgrandom.md. They are implementation/regression vectors, not yet
// runtime-verified against the reference. Keep them until a reference trace is
// available, then replace/augment with reference-observed vectors.

#include <doctest/doctest.h>
#include <array>
#include "engine/math/UnityRandom.hpp"

using namespace soulknight::engine;

namespace {
int32_t LazySeed12345() { return 12345; }
}

TEST_CASE("UnityRandom seed expansion and xorshift vectors" * doctest::test_suite("unit")) {
    UnityRandom::InitState(12345);
    const UnityRandomState state = UnityRandom::GetState();
    CHECK(state.s0 == 0x00003039u);
    CHECK(state.s1 == 0x7770877Eu);
    CHECK(state.s2 == 0x3459E2B7u);
    CHECK(state.s3 == 0x28CB6134u);

    const std::array<uint32_t, 8> expected = {
        0x294B1DECu, 0xDAF324CCu, 0x2144297Fu, 0x52F52EA2u,
        0x2320FD6Du, 0x60B66881u, 0x60B9BA39u, 0x9BC208B2u,
    };
    for (uint32_t value : expected) {
        CHECK(UnityRandom::Next() == value);
    }
}

TEST_CASE("UnityRandom float value uses 23-bit fraction" * doctest::test_suite("unit")) {
    UnityRandom::InitState(12345);
    CHECK(UnityRandom::Value() == doctest::Approx(0.586850643157959f).epsilon(1e-6));
    CHECK(UnityRandom::Value() == doctest::Approx(0.8995604515075684f).epsilon(1e-6));
    CHECK(UnityRandom::Value() == doctest::Approx(0.532516360282898f).epsilon(1e-6));
}

TEST_CASE("RGRandomStream int Range sequence" * doctest::test_suite("unit")) {
    RGRandomStream stream;
    stream.SetRandomSeed(12345);

    const std::array<int32_t, 12> expected = {6, 6, 9, 0, 9, 1, 9, 0, 0, 6, 6, 3};
    for (int32_t value : expected) {
        CHECK(stream.Range(0, 10) == value);
    }
}

TEST_CASE("RGRandomStream min > max branch" * doctest::test_suite("unit")) {
    RGRandomStream stream;
    stream.SetRandomSeed(12345);

    const std::array<int32_t, 12> expected = {4, 4, 1, 10, 1, 9, 1, 10, 10, 4, 4, 7};
    for (int32_t value : expected) {
        CHECK(stream.Range(10, 0) == value);
    }
}

TEST_CASE("RGRandomStream equal bounds does not advance" * doctest::test_suite("unit")) {
    RGRandomStream stream;
    stream.SetRandomSeed(12345);

    CHECK(stream.Range(7, 7) == 7);
    // The first draw after the equal-bound call must still be the first vector.
    CHECK(stream.Range(0, 10) == 6);
}

TEST_CASE("RGRandomStream float Range operation order" * doctest::test_suite("unit")) {
    RGRandomStream stream;
    stream.SetRandomSeed(12345);

    CHECK(stream.Range(0.0f, 1.0f) == doctest::Approx(0.586850643157959f).epsilon(1e-6));
    CHECK(stream.Range(0.0f, 1.0f) == doctest::Approx(0.8995604515075684f).epsilon(1e-6));
    CHECK(stream.Range(0.0f, 1.0f) == doctest::Approx(0.532516360282898f).epsilon(1e-6));
}

TEST_CASE("RGRandomStream SetRandomSeed preserves global state" * doctest::test_suite("unit")) {
    UnityRandom::InitState(999);
    const UnityRandomState before = UnityRandom::GetState();

    RGRandomStream stream;
    stream.SetRandomSeed(12345);

    CHECK(UnityRandom::GetState() == before);
    CHECK(stream.IsInitialized());
    CHECK(stream.GetSeed() == 12345);
}

TEST_CASE("RGRandomStream lazy seed provider" * doctest::test_suite("unit")) {
    RGRandomStream stream;
    stream.SetLazySeedProvider(&LazySeed12345);

    CHECK(!stream.IsInitialized());
    CHECK(stream.Range(0, 10) == 6);
    CHECK(stream.IsInitialized());
}

TEST_CASE("RGRandomStream streams are independent when interleaved" * doctest::test_suite("unit")) {
    RGRandomStream expected_stream;
    expected_stream.SetRandomSeed(12345);
    std::array<int32_t, 16> expected{};
    for (int32_t& value : expected) {
        value = expected_stream.Range(0, 100);
    }

    RGRandomStream a;
    RGRandomStream b;
    a.SetRandomSeed(12345);
    b.SetRandomSeed(54321);

    for (int32_t value : expected) {
        CHECK(a.Range(0, 100) == value);
        (void)b.Range(0, 100);
    }
}

TEST_CASE("RGRandomStream RandomInCircle stays inside radius" * doctest::test_suite("unit")) {
    RGRandomStream stream;
    stream.SetRandomSeed(12345);

    for (int i = 0; i < 32; ++i) {
        const Vec3 point = stream.RandomInCircle(5.0f);
        CHECK(point.z == 0.0f);
        CHECK(point.Length() <= 5.0f + 1e-4f);
    }
}
