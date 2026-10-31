// Soul Knight Linux — reference-compatible Unity 2018.4 RNG
//
// Reconstructed from libunity.so disassembly. See analysis/re/rgrandom.md.
// This is not PCG32. Do not use it for non-parity purposes without documenting
// the difference.

#pragma once

#include <cstdint>
#include "Vec2.hpp"
#include "Vec3.hpp"

namespace soulknight::engine {

struct UnityRandomState {
    uint32_t s0 = 0;
    uint32_t s1 = 0;
    uint32_t s2 = 0;
    uint32_t s3 = 0;

    bool operator==(const UnityRandomState& other) const {
        return s0 == other.s0 && s1 == other.s1 && s2 == other.s2 && s3 == other.s3;
    }
    bool operator!=(const UnityRandomState& other) const { return !(*this == other); }
};

// The engine-global UnityEngine.Random state. RGRandom streams borrow this as a
// scratch register; see RGRandomStream below.
class UnityRandom {
public:
    static UnityRandomState GetState();
    static void SetState(const UnityRandomState& state);

    // Unity 2018.4 seeding: four-word LCG expansion, no warm-up.
    static void InitState(int32_t seed);

    // Xorshift128 transition (shifts 11, 8, 19).
    static uint32_t Next();

    // 23-bit fraction in [0, 1).
    static float Value();

    // Exact reference branches, including min > max.
    static int32_t Range(int32_t min, int32_t max);
    static float Range(float min, float max);

    // (cos((1-u1)*2pi) * sqrt(1-u2), sin((1-u1)*2pi) * sqrt(1-u2))
    static Vec2 InsideUnitCircle();

private:
    static UnityRandomState state_;
};

// Per-object RGRandom wrapper. Local stream state is saved/restored around the
// engine-global RNG; sampling leaves the global state advanced (as the
// reference does).
class RGRandomStream {
public:
    using SeedProvider = int32_t (*)();

    RGRandomStream() = default;

    // The reference lazily seeds from RGGameInfo.Inst.sample_random_seed.
    // Native game code supplies the equivalent callback.
    void SetLazySeedProvider(SeedProvider provider) { lazy_seed_provider_ = provider; }

    void SetRandomSeed(int32_t seed);

    int32_t Range(int32_t min, int32_t max);
    float Range(float min, float max);
    Vec3 RandomInCircle(float radius);

    bool IsInitialized() const { return seed_state_ == 3; }
    int32_t GetSeed() const { return seed_; }
    UnityRandomState GetLocalState() const { return this_state_; }

private:
    void EnsureInitialized();

    int32_t seed_state_ = 0;  // RANDOM_STATE: 0 not ready, 3 GET_SEED
    int32_t seed_ = 0;
    UnityRandomState this_state_{};
    SeedProvider lazy_seed_provider_ = nullptr;
};

} // namespace soulknight::engine
