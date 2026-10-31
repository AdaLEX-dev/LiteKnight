// Soul Knight Linux — small native developer text renderer
#pragma once

#include "SpriteBatch.hpp"
#include "engine/assets/TextureData.hpp"
#include "engine/math/Vec2.hpp"

#include <cstdint>
#include <string_view>

namespace soulknight::engine::rendering {

// PLACEHOLDER UI ASSET: a compact 5x7 developer font generated at runtime.
// No recovered UI font atlas has been integrated yet. The glyph texture is a
// single nearest-filtered atlas; all text is drawn through the normal
// SpriteBatch/SpriteRenderer path.
class DebugTextRenderer {
public:
    static constexpr uint32_t kTextureId = 0x4C495445U; // "LITE"
    static constexpr int kAtlasColumns = 16;
    static constexpr int kAtlasRows = 6;
    static constexpr int kCellSize = 8;

    bool BuildAtlas();
    const engine::assets::TextureData& Atlas() const { return atlas_; }

    void DrawText(SpriteBatch& batch, std::string_view text,
                  const Vec2& top_left, float pixel_scale,
                  Color color = Color::White(), int32_t order = 100) const;
    void DrawPanel(SpriteBatch& batch, const Vec2& top_left,
                   const Vec2& size, Color color, int32_t order = 90) const;
    float TextWidth(std::string_view text, float pixel_scale) const;
    float LineHeight(float pixel_scale) const { return 10.0f * pixel_scale; }

private:
    static const char* GlyphRows(char character);
    engine::assets::TextureData atlas_;
};

} // namespace soulknight::engine::rendering
