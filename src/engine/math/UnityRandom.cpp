// Soul Knight Linux — reference-compatible Unity 2018.4 RNG
// Evidence: analysis/re/rgrandom.md (libunity.so addresses 0x37AAA0..0x37BA70)

#include "UnityRandom.hpp"

#include <cmath>

namespace soulknight::engine {

namespace {
constexpr uint32_t kSeedMultiplier = 1812433253u; // 0x6C078965
constexpr uint32_t kFractionMask = 0x7FFFFFu;
constexpr float kFractionScale = 1.0f / 8388608.0f; // 2^-23
constexpr float kTwoPi = 6.2831854820251465f;
} // namespace

UnityRandomState UnityRandom::state_{};

UnityRandomState UnityRandom::GetState() { return state_; }

void UnityRandom::SetState(const UnityRandomState& state) { state_ = state; }

void UnityRandom::InitState(int32_t seed) {
    state_.s0 = static_cast<uint32_t>(seed);
    state_.s1 = state_.s0 * kSeedMultiplier + 1u;
    state_.s2 = state_.s1 * kSeedMultiplier + 1u;
    state_.s3 = state_.s2 * kSeedMultiplier + 1u;
}

uint32_t UnityRandom::Next() {
    uint32_t t = state_.s0 ^ (state_.s0 << 11);
    t ^= (t >> 8);
    const uint32_t r = t ^ state_.s3 ^ (state_.s3 >> 19);

    state_.s0 = state_.s1;
    state_.s1 = state_.s2;
    state_.s2 = state_.s3;
    state_.s3 = r;
    return r;
}

float UnityRandom::Value() {
    return static_cast<float>(Next() & kFractionMask) * kFractionScale;
}

int32_t UnityRandom::Range(int32_t min, int32_t max) {
    if (min < max) {
        const uint32_t range = static_cast<uint32_t>(max - min);
        const uint32_t r = Next() % range;
        return static_cast<int32_t>(static_cast<uint32_t>(min) + r);
    }
    if (max >= min) {
        return min;
    }
    // Reference branch for min > max: returns values in (max, min].
    const uint32_t range = static_cast<uint32_t>(min - max);
    const uint32_t r = Next() % range;
    return static_cast<int32_t>(static_cast<uint32_t>(min) - r);
}

float UnityRandom::Range(float min, float max) {
    const float f = Value();
    // Preserve the reference operation order: f*max + (1-f)*min.
    return f * max + (1.0f - f) * min;
}

Vec2 UnityRandom::InsideUnitCircle() {
    const float u1 = Value();
    const float angle = (1.0f - u1) * kTwoPi;
    const float u2 = Value();
    const float radius = std::sqrt(1.0f - u2);
    return Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
}

void RGRandomStream::SetRandomSeed(int32_t seed) {
    seed_ = seed;

    const UnityRandomState old_global = UnityRandom::GetState();
    UnityRandom::InitState(seed);
    this_state_ = UnityRandom::GetState();
    UnityRandom::SetState(old_global);

    seed_state_ = 3; // RANDOM_STATE.GET_SEED
}

void RGRandomStream::EnsureInitialized() {
    if (seed_state_ != 3) {
        SetRandomSeed(lazy_seed_provider_ ? lazy_seed_provider_() : 0);
    }
}

int32_t RGRandomStream::Range(int32_t min, int32_t max) {
    EnsureInitialized();
    UnityRandom::SetState(this_state_);
    const int32_t result = UnityRandom::Range(min, max);
    this_state_ = UnityRandom::GetState();
    return result;
}

float RGRandomStream::Range(float min, float max) {
    EnsureInitialized();
    UnityRandom::SetState(this_state_);
    const float result = UnityRandom::Range(min, max);
    this_state_ = UnityRandom::GetState();
    return result;
}

Vec3 RGRandomStream::RandomInCircle(float radius) {
    EnsureInitialized();
    UnityRandom::SetState(this_state_);
    const Vec2 p = UnityRandom::InsideUnitCircle();
    this_state_ = UnityRandom::GetState();
    return Vec3{p.x * radius, p.y * radius, 0.0f};
}

} // namespace soulknight::engine
