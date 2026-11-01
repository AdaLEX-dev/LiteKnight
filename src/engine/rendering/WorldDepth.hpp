// Soul Knight Linux — deterministic world-depth ordering
#pragma once

#include <cmath>
#include <cstdint>

namespace soulknight::engine::rendering {

// The reference scene places wall fronts and characters on distinct static
// sorting layers (WallFont=6 above Character=5). That static order hides a
// character whenever its tall body overlaps a wall/furniture front. The native
// renderer keeps the recovered layer semantics for backdrop/projectile/UI but
// resolves character-adjacent foreground sprites on a Y depth band:
//   lower world Y (south) draws later, in front.
// This is a rendering rule, not a player transform offset. It is documented as
// HYPOTHESIS because the exact reference runtime ordering script is not present.
constexpr int32_t kWorldDepthBase = 100000;
constexpr float kWorldDepthScale = 100.0f;

inline int32_t WorldDepthOrder(float base_y, int32_t tie_break = 0) {
    if (!std::isfinite(base_y)) return kWorldDepthBase + tie_break;
    return kWorldDepthBase -
           static_cast<int32_t>(std::lround(base_y * kWorldDepthScale)) +
           tie_break;
}

// Ground props and characters use the sprite's physical foot (bottom) rather
// than the serialized transform Y, because centre-pivoted furniture would
// otherwise sort half a sprite too high. Tilemaps, lights and UI overlays keep
// their authored sorting.
inline float SpriteFootY(float position_y, float world_height, float pivot_y) {
    if (!std::isfinite(position_y) || !std::isfinite(world_height) ||
        !std::isfinite(pivot_y)) {
        return position_y;
    }
    return position_y - world_height * pivot_y;
}

inline bool IsCharacterSortedUnityLayer(int32_t unity_layer) {
    // Character (5) and WallFont (6): bodies, NPCs, doors, wall fronts and
    // obstacle tops. Recover layer values themselves are unchanged in scene JSON.
    return unity_layer == 5 || unity_layer == 6;
}

} // namespace soulknight::engine::rendering
