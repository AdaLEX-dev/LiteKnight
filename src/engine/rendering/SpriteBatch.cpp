// Soul Knight Linux — Render-world sprite batch implementation
#include "SpriteBatch.hpp"

#include <algorithm>

namespace soulknight::engine::rendering {

SpriteBatch::SpriteBatch() {
    draws_.reserve(kMaxSprites);
}

void SpriteBatch::Clear() {
    draws_.clear();
}

bool SpriteBatch::DrawSprite(const Vec2& position, const Vec2& size, const Rect& uv,
                             const Color& color, SortLayer layer) {
    if (draws_.size() >= kMaxSprites) return false;
    draws_.push_back(SpriteDraw{position, size, uv, color, layer});
    return true;
}

void SpriteBatch::Sort() {
    std::stable_sort(draws_.begin(), draws_.end(),
                     [](const SpriteDraw& a, const SpriteDraw& b) {
                         return static_cast<uint8_t>(a.layer) < static_cast<uint8_t>(b.layer);
                     });
}

} // namespace soulknight::engine::rendering
