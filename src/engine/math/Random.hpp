// Soul Knight Linux — PCG32 utility RNG (NOT the game's reference RNG)
//
// PCG32 is retained only as a general-purpose non-parity utility (for example
// editor/debug sampling). Gameplay must use the reference-compatible
// UnityRandom / RGRandomStream in UnityRandom.hpp. See analysis/re/rgrandom.md.
#pragma once

#include <cstdint>
#include <random>
#include <engine/core/Types.hpp>
#include <engine/math/Vec2.hpp>

namespace soulknight::engine {

// PCG32 — Permuted Congruential Generator
// Fast, statistically good, portable, deterministic
class PCG32 {
public:
    using state_type = uint64_t;
    using result_type = uint32_t;

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT32_MAX; }

    PCG32() = default;

    explicit PCG32(uint64_t seed) {
        state = 0;
        inc = (seed << 1) | 1;
        Next(); // Warm up
    }

    PCG32(uint64_t seed, uint64_t sequence) {
        state = 0;
        inc = (sequence << 1) | 1;
        Next();
        state += seed;
        Next();
    }

    // Seed with entropy
    void Seed(uint64_t seed) {
        state = 0;
        inc = (seed << 1) | 1;
        Next();
    }

    void Seed(uint64_t seed, uint64_t sequence) {
        state = 0;
        inc = (sequence << 1) | 1;
        Next();
        state += seed;
        Next();
    }

    // Generate next 32-bit value
    uint32_t Next() {
        uint64_t old_state = state;
        state = old_state * 6364136223846793005ULL + inc;
        uint32_t xorshifted = static_cast<uint32_t>(((old_state >> 18u) ^ old_state) >> 27u);
        uint32_t rot = static_cast<uint32_t>(old_state >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }

    // Standard RNG interface
    uint32_t operator()() { return Next(); }

    // Discard n values
    void Discard(uint64_t n) {
        for (uint64_t i = 0; i < n; ++i) Next();
    }

    // Serialize state
    void Serialize(uint64_t& out_state, uint64_t& out_inc) const {
        out_state = state;
        out_inc = inc;
    }

    void Deserialize(uint64_t in_state, uint64_t in_inc) {
        state = in_state;
        inc = in_inc;
    }

    // Comparison
    bool operator==(const PCG32& other) const { return state == other.state && inc == other.inc; }
    bool operator!=(const PCG32& other) const { return !(*this == other); }

private:
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc = 0xda3e39cb94b95bdbULL;
};

// Random wrapper with distributions
class Random {
public:
    Random() : rng_(std::random_device{}()) {}
    explicit Random(uint64_t seed) : rng_(seed) {}

    // Uniform distributions
    uint32_t UInt32() { return rng_.Next(); }
    uint64_t UInt64() { return (static_cast<uint64_t>(rng_.Next()) << 32) | rng_.Next(); }

    int32_t Int32() { return static_cast<int32_t>(rng_.Next()); }
    int64_t Int64() { return static_cast<int64_t>(UInt64()); }

    int32_t Range(int32_t min, int32_t max) {
        return min + static_cast<int32_t>(rng_.Next() % static_cast<uint32_t>(max - min + 1));
    }

    // Float distributions
    float Float() { return static_cast<float>(rng_.Next()) * (1.0f / 4294967296.0f); } // [0, 1)
    float Float(float min, float max) { return min + Float() * (max - min); }

    // Double distributions
    double Double() { return rng_.Next() * (1.0 / 4294967296.0); }
    double Double(double min, double max) { return min + Double() * (max - min); }

    // Boolean
    bool Bool() { return (rng_.Next() & 1) != 0; }
    bool Bool(float probability) { return Float() < probability; }

    // Vector
    Vec2 Vec2Unit() {
        float angle = Float() * 2.0f * PI;
        return {std::cos(angle), std::sin(angle)};
    }

    Vec2 Vec2InCircle(float radius = 1.0f) {
        float r = std::sqrt(Float()) * radius;
        float angle = Float() * 2.0f * PI;
        return {r * std::cos(angle), r * std::sin(angle)};
    }

    // Weighted choice
    template<typename T>
    const T& Choice(const std::vector<T>& items) {
        return items[Range(0, static_cast<int32_t>(items.size()) - 1)];
    }

    template<typename T, typename W>
    const T& WeightedChoice(const std::vector<T>& items, const std::vector<W>& weights) {
        float total = 0;
        for (auto w : weights) total += static_cast<float>(w);
        float r = Float() * total;
        float sum = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            sum += static_cast<float>(weights[i]);
            if (r <= sum) return items[i];
        }
        return items.back();
    }

    // Seed
    void Seed(uint64_t seed) { rng_.Seed(seed); }
    void Seed(uint64_t seed, uint64_t sequence) { rng_.Seed(seed, sequence); }

    // Serialize
    void Serialize(uint64_t& state, uint64_t& inc) const { rng_.Serialize(state, inc); }
    void Deserialize(uint64_t state, uint64_t inc) { rng_.Deserialize(state, inc); }

    // Access underlying RNG
    PCG32& GetRNG() { return rng_; }
    const PCG32& GetRNG() const { return rng_; }

private:
    PCG32 rng_;
};

// Global RNG (thread-local for safety)
inline thread_local Random g_rng;

inline Random& GetRNG() { return g_rng; }
inline void SetGlobalSeed(uint64_t seed) { g_rng.Seed(seed); }

} // namespace soulknight::engine