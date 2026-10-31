// Soul Knight Linux — Render-world sprite batch
#pragma once

#include "engine/math/Color.hpp"
#include "engine/math/Rect.hpp"
#include "engine/math/Vec2.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace soulknight::engine::rendering {

// PROJECT DECISION: the reference display names are preserved, but these numeric
// values are a native ordering contract, not recovered Unity sorting-layer values.
// The renderer sorts stably so insertion order remains the tie-breaker.
enum class SortLayer : uint8_t {
    BackGround = 0,
    Floor,
    Shadow,
    Wall,
    Character,
    WallFont,
    Effect,
    UI,
    Count
};

struct SpriteDraw {
    Vec2 position{};       // world-space pivot position
    Vec2 size{};           // world-space size
    Rect uv{0.0f, 0.0f, 1.0f, 1.0f}; // normalized top-left UV rect
    Color color = Color::White();
    SortLayer layer = SortLayer::Character;
    int32_t sorting_order = 0;
    uint32_t texture_id = 0;
    Vec2 pivot{0.5f, 0.5f}; // normalized, bottom-left origin
    float rotation = 0.0f;  // radians, counter-clockwise in Y-up world
    bool flip_x = false;
    bool flip_y = false;
};

struct SpriteQuad {
    std::array<Vec2, 4> positions{}; // BL, BR, TR, TL in world space
    std::array<Vec2, 4> uvs{};       // matching normalized top-left UVs
};

SpriteQuad BuildSpriteQuad(const SpriteDraw& draw);

class SpriteBatch {
public:
    static constexpr std::size_t kMaxSprites = 16384;

    SpriteBatch();

    void Clear();
    bool DrawSprite(const Vec2& position, const Vec2& size, const Rect& uv,
                    const Color& color, SortLayer layer,
                    uint32_t texture_id = 0, const Vec2& pivot = {0.5f, 0.5f},
                    float rotation = 0.0f, int32_t sorting_order = 0,
                    bool flip_x = false, bool flip_y = false);
    bool DrawSprite(const SpriteDraw& draw);
    void Sort();

    const std::vector<SpriteDraw>& GetDraws() const { return draws_; }
    bool IsEmpty() const { return draws_.empty(); }

private:
    std::vector<SpriteDraw> draws_;
};

} // namespace soulknight::engine::rendering
