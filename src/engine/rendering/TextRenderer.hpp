// Soul Knight Linux — native pixel text renderer
#pragma once

#include "BitmapFont.hpp"
#include "SpriteBatch.hpp"
#include "engine/assets/TextureData.hpp"
#include "engine/math/Vec2.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace soulknight::platform {
class FileSystem;
}

namespace soulknight::engine::rendering {

enum class TextAlign {
    Left,
    Center,
    Right
};

struct TextStyle {
    Color color = Color::White();
    bool shadow = false;
    Vec2 shadow_offset{2.0f, 2.0f};
    Color shadow_color{0, 0, 0, 200};
    bool outline = false;
};

// Draws through the regular SpriteBatch/SpriteRenderer path. If the locally
// generated recovered font is unavailable, a deterministic developer atlas is
// used so the executable still runs from a fresh checkout.
class TextRenderer {
public:
    static constexpr uint32_t kTextureId = BitmapFont::kTextureId;
    static constexpr int kAtlasColumns = 16;
    static constexpr int kAtlasRows = 6;
    static constexpr int kCellSize = 8;

    bool BuildAtlas() { return BuildFont(); }
    bool LoadPixelFont(const platform::FileSystem& filesystem,
                       const std::string& json_path, std::string* error);
    bool BuildFont();
    bool HasPixelFont() const { return font_.IsValid(); }
    const BitmapFont& Font() const { return font_; }

    const engine::assets::TextureData& Atlas() const {
        return font_.IsValid() ? font_.Atlas() : fallback_atlas_;
    }
    uint32_t TextureId() const { return kTextureId; }

    void DrawText(SpriteBatch& batch, std::string_view text,
                  const Vec2& top_left, float pixel_scale,
                  Color color = Color::White(), int32_t order = 100) const;
    void DrawText(SpriteBatch& batch, std::string_view text,
                  const Vec2& top_left, float pixel_scale,
                  const TextStyle& style, int32_t order = 100,
                  TextAlign align = TextAlign::Left,
                  float alignment_width = 0.0f) const;
    void DrawPanel(SpriteBatch& batch, const Vec2& top_left,
                   const Vec2& size, Color color, int32_t order = 90) const;
    void DrawPanelBorder(SpriteBatch& batch, const Vec2& top_left,
                         const Vec2& size, Color fill, Color border,
                         float border_width, int32_t order = 90) const;
    void DrawRect(SpriteBatch& batch, const Vec2& top_left,
                  const Vec2& size, Color color, int32_t order = 90) const;

    float TextWidth(std::string_view text, float pixel_scale) const;
    float LineHeight(float pixel_scale) const;
    float MeasureHeight(std::string_view text, float pixel_scale) const;

private:
    bool BuildFallbackAtlas();
    static const char* GlyphRows(char character);
    const Rect& WhiteUv() const;
    void DrawGlyphRun(SpriteBatch& batch, std::string_view text,
                      const Vec2& top_left, float pixel_scale,
                      const Color& color, int32_t order) const;

    BitmapFont font_;
    engine::assets::TextureData fallback_atlas_;
};

} // namespace soulknight::engine::rendering
