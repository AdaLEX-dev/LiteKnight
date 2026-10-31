// Soul Knight Linux — recovered pixel-font atlas
#pragma once

#include "engine/assets/TextureData.hpp"
#include "engine/math/Rect.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace soulknight::platform {
class FileSystem;
}

namespace soulknight::engine::rendering {

// Native loader for the locally generated atlas produced by
// tools/asset/build_ui_font.py from the recovered `pixel_bold` TTF.
// The atlas is reference-derived and therefore local-only.
class BitmapFont {
public:
    static constexpr uint32_t kTextureId = 0x50495846U; // "PIXF"

    struct Glyph {
        uint32_t codepoint = 0;
        Rect uv{0.0f, 0.0f, 0.0f, 0.0f};
        int width = 0;
        int height = 0;
        int bearing_x = 0;
        int bearing_y = 0;
        int advance = 0;
    };

    bool Load(const platform::FileSystem& filesystem,
              const std::string& json_path, std::string* error);
    bool IsValid() const { return valid_; }

    const engine::assets::TextureData& Atlas() const { return atlas_; }
    uint32_t TextureId() const { return kTextureId; }
    int Ascent() const { return ascent_; }
    int Descent() const { return descent_; }
    int LineHeight() const { return line_height_; }
    const Rect& WhiteUvRect() const { return white_uv_; }

    const Glyph* Find(uint32_t codepoint) const;
    float MeasureWidth(const std::string& text, float pixel_scale) const;
    std::size_t LineCount(const std::string& text) const;

private:
    std::unordered_map<uint32_t, Glyph> glyphs_;
    engine::assets::TextureData atlas_;
    Rect white_uv_{0.0f, 0.0f, 0.0f, 0.0f};
    int ascent_ = 0;
    int descent_ = 0;
    int line_height_ = 0;
    bool valid_ = false;
};

} // namespace soulknight::engine::rendering
