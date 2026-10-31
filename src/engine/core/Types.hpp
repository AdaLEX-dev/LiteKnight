// Soul Knight Linux — Core Types
#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>

namespace soulknight {

// Integer types
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

// Entity ID (32-bit: 24 bits index, 8 bits generation)
using Entity = u32;
constexpr Entity INVALID_ENTITY = std::numeric_limits<u32>::max();

inline u32 EntityIndex(Entity e) { return e & 0x00FFFFFF; }
inline u32 EntityGeneration(Entity e) { return (e >> 24) & 0xFF; }
inline Entity MakeEntity(u32 index, u32 generation) {
    return (generation << 24) | (index & 0x00FFFFFF);
}

// Component ID
using ComponentType = u16;
constexpr ComponentType INVALID_COMPONENT = std::numeric_limits<u16>::max();

// Math constants
constexpr f32 PI = 3.14159265358979323846f;
constexpr f32 TAU = 2.0f * PI;
constexpr f32 DEG2RAD = PI / 180.0f;
constexpr f64 RAD2DEG = 180.0f / PI;

// Utility
template<typename T>
constexpr T Clamp(T v, T lo, T hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

template<typename T>
constexpr T Lerp(T a, T b, f32 t) {
    return a + (b - a) * Clamp(t, 0.0f, 1.0f);
}

} // namespace soulknight