// Soul Knight Linux — Render-world sprite batch implementation
#include "SpriteBatch.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::engine::rendering {

SpriteBatch::SpriteBatch() {
    draws_.reserve(kMaxSprites);
}

void SpriteBatch::Clear() {
    draws_.clear();
}

SpriteQuad BuildSpriteQuad(const SpriteDraw& draw) {
    SpriteQuad quad;

    const float width = draw.size.x;
    const float height = draw.size.y;
    // A flipped sprite is mirrored around its pivot, matching Unity's
    // SpriteRenderer.flipX/flipY and the recovered 180-degree Y rotation of the
    // player's `img` transform. Mirroring only the UVs would shift asymmetric
    // pivots (for example the recovered player pivot x=0.6) away from the
    // simulation position when facing changes.
    const float left = draw.flip_x
        ? (draw.pivot.x - 1.0f) * width
        : -draw.pivot.x * width;
    const float right = draw.flip_x
        ? draw.pivot.x * width
        : (1.0f - draw.pivot.x) * width;
    const float bottom = draw.flip_y
        ? (draw.pivot.y - 1.0f) * height
        : -draw.pivot.y * height;
    const float top = draw.flip_y
        ? draw.pivot.y * height
        : (1.0f - draw.pivot.y) * height;

    const Vec2 local[4] = {
        {left, bottom},
        {right, bottom},
        {right, top},
        {left, top}
    };

    const float c = std::cos(draw.rotation);
    const float s = std::sin(draw.rotation);
    for (std::size_t i = 0; i < 4; ++i) {
        const Vec2 rotated{
            local[i].x * c - local[i].y * s,
            local[i].x * s + local[i].y * c
        };
        quad.positions[i] = draw.position + rotated;
    }

    float u0 = draw.uv.x;
    float u1 = draw.uv.x + draw.uv.w;
    float v0 = draw.uv.y;
    float v1 = draw.uv.y + draw.uv.h;
    if (draw.flip_x) std::swap(u0, u1);
    if (draw.flip_y) std::swap(v0, v1);

    // Texture top is v0; world top is the third/fourth vertex.
    quad.uvs[0] = {u0, v1};
    quad.uvs[1] = {u1, v1};
    quad.uvs[2] = {u1, v0};
    quad.uvs[3] = {u0, v0};
    return quad;
}

bool SpriteBatch::DrawSprite(const Vec2& position, const Vec2& size, const Rect& uv,
                             const Color& color, SortLayer layer,
                             uint32_t texture_id, const Vec2& pivot,
                             float rotation, int32_t sorting_order,
                             bool flip_x, bool flip_y) {
    if (draws_.size() >= kMaxSprites) return false;
    SpriteDraw draw;
    draw.position = position;
    draw.size = size;
    draw.uv = uv;
    draw.color = color;
    draw.layer = layer;
    draw.texture_id = texture_id;
    draw.pivot = pivot;
    draw.rotation = rotation;
    draw.sorting_order = sorting_order;
    draw.flip_x = flip_x;
    draw.flip_y = flip_y;
    draws_.push_back(draw);
    return true;
}

bool SpriteBatch::DrawSprite(const SpriteDraw& draw) {
    if (draws_.size() >= kMaxSprites) return false;
    draws_.push_back(draw);
    return true;
}

void SpriteBatch::Sort() {
    std::stable_sort(draws_.begin(), draws_.end(),
                     [](const SpriteDraw& a, const SpriteDraw& b) {
                         if (a.layer != b.layer) {
                             return static_cast<uint8_t>(a.layer) <
                                    static_cast<uint8_t>(b.layer);
                         }
                         return a.sorting_order < b.sorting_order;
                     });
}

} // namespace soulknight::engine::rendering
