// Soul Knight Linux — Render-world sprite batch
#pragma once

#include "engine/math/Color.hpp"
#include "engine/math/Rect.hpp"
#include "engine/math/Vec2.hpp"

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
    Vec2 position{};   // top-left in 1920x1080 logical pixels
    Vec2 size{};
    Rect uv{0.0f, 0.0f, 1.0f, 1.0f};
    Color color = Color::White();
    SortLayer layer = SortLayer::Character;
};

class SpriteBatch {
public:
    static constexpr std::size_t kMaxSprites = 4096;

    SpriteBatch();

    void Clear();
    bool DrawSprite(const Vec2& position, const Vec2& size, const Rect& uv,
                    const Color& color, SortLayer layer);
    void Sort();

    const std::vector<SpriteDraw>& GetDraws() const { return draws_; }
    bool IsEmpty() const { return draws_.empty(); }

private:
    std::vector<SpriteDraw> draws_;
};

} // namespace soulknight::engine::rendering
