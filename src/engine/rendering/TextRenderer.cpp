#include "TextRenderer.hpp"

#include "ColorSpace.hpp"
#include "platform/filesystem/FileSystem.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace soulknight::engine::rendering {
namespace {

constexpr int kGlyphCount = 96;
constexpr int kWhiteGlyph = 95;

char UpperAscii(char character) {
    const unsigned char value = static_cast<unsigned char>(character);
    if (value >= 'a' && value <= 'z') {
        return static_cast<char>(value - ('a' - 'A'));
    }
    return static_cast<char>(character);
}

} // namespace

bool TextRenderer::LoadPixelFont(const platform::FileSystem& filesystem,
                                 const std::string& json_path,
                                 std::string* error) {
    return font_.Load(filesystem, json_path, error);
}

bool TextRenderer::BuildFont() {
    if (font_.IsValid()) return true;
    return BuildFallbackAtlas();
}

const char* TextRenderer::GlyphRows(char character) {
    switch (UpperAscii(character)) {
        case 'A': return "01110""10001""10001""11111""10001""10001""10001";
        case 'B': return "11110""10001""10001""11110""10001""10001""11110";
        case 'C': return "01110""10001""10000""10000""10000""10001""01110";
        case 'D': return "11110""10001""10001""10001""10001""10001""11110";
        case 'E': return "11111""10000""10000""11110""10000""10000""11111";
        case 'F': return "11111""10000""10000""11110""10000""10000""10000";
        case 'G': return "01110""10001""10000""10111""10001""10001""01110";
        case 'H': return "10001""10001""10001""11111""10001""10001""10001";
        case 'I': return "11111""00100""00100""00100""00100""00100""11111";
        case 'J': return "00111""00010""00010""00010""00010""10010""01100";
        case 'K': return "10001""10010""10100""11000""10100""10010""10001";
        case 'L': return "10000""10000""10000""10000""10000""10000""11111";
        case 'M': return "10001""11011""10101""10101""10001""10001""10001";
        case 'N': return "10001""11001""10101""10011""10001""10001""10001";
        case 'O': return "01110""10001""10001""10001""10001""10001""01110";
        case 'P': return "11110""10001""10001""11110""10000""10000""10000";
        case 'Q': return "01110""10001""10001""10001""10101""10010""01101";
        case 'R': return "11110""10001""10001""11110""10100""10010""10001";
        case 'S': return "01111""10000""10000""01110""00001""00001""11110";
        case 'T': return "11111""00100""00100""00100""00100""00100""00100";
        case 'U': return "10001""10001""10001""10001""10001""10001""01110";
        case 'V': return "10001""10001""10001""10001""10001""01010""00100";
        case 'W': return "10001""10001""10001""10101""10101""11011""10001";
        case 'X': return "10001""10001""01010""00100""01010""10001""10001";
        case 'Y': return "10001""10001""01010""00100""00100""00100""00100";
        case 'Z': return "11111""00001""00010""00100""01000""10000""11111";
        case '0': return "01110""10001""10011""10101""11001""10001""01110";
        case '1': return "00100""01100""00100""00100""00100""00100""01110";
        case '2': return "01110""10001""00001""00010""00100""01000""11111";
        case '3': return "11110""00001""00001""01110""00001""00001""11110";
        case '4': return "00010""00110""01010""10010""11111""00010""00010";
        case '5': return "11111""10000""10000""11110""00001""00001""11110";
        case '6': return "01110""10000""10000""11110""10001""10001""01110";
        case '7': return "11111""00001""00010""00100""01000""01000""01000";
        case '8': return "01110""10001""10001""01110""10001""10001""01110";
        case '9': return "01110""10001""10001""01111""00001""00001""01110";
        case ':': return "00000""00100""00100""00000""00100""00100""00000";
        case '-': return "00000""00000""00000""11111""00000""00000""00000";
        case '/': return "00001""00010""00010""00100""01000""01000""10000";
        case '.': return "00000""00000""00000""00000""00000""00100""00100";
        case '+': return "00000""00100""00100""11111""00100""00100""00000";
        case '%': return "11000""11001""00010""00100""01000""10011""00011";
        case '_': return "00000""00000""00000""00000""00000""00000""11111";
        case '[': return "01110""01000""01000""01000""01000""01000""01110";
        case ']': return "01110""00010""00010""00010""00010""00010""01110";
        case '(': return "00010""00100""01000""01000""01000""00100""00010";
        case ')': return "01000""00100""00010""00010""00010""00100""01000";
        case '?': return "01110""10001""00001""00010""00100""00000""00100";
        case '!': return "00100""00100""00100""00100""00100""00000""00100";
        case ',': return "00000""00000""00000""00000""00100""00100""01000";
        case '=': return "00000""00000""11111""00000""11111""00000""00000";
        case '#': return "01010""01010""11111""01010""11111""01010""01010";
        case '*': return "00000""10101""01110""11111""01110""10101""00000";
        case '<': return "00010""00100""01000""10000""01000""00100""00010";
        case '>': return "01000""00100""00010""00001""00010""00100""01000";
        case '|': return "00100""00100""00100""00100""00100""00100""00100";
        case ' ': return "00000""00000""00000""00000""00000""00000""00000";
        default: return "01110""10001""00001""00010""00100""00000""00100";
    }
}

bool TextRenderer::BuildFallbackAtlas() {
    fallback_atlas_.width = static_cast<uint32_t>(kAtlasColumns * kCellSize);
    fallback_atlas_.height = static_cast<uint32_t>(kAtlasRows * kCellSize);
    fallback_atlas_.rgba8.assign(fallback_atlas_.ExpectedByteSize(), 0);

    const auto put = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= static_cast<int>(fallback_atlas_.width) ||
            y >= static_cast<int>(fallback_atlas_.height)) {
            return;
        }
        const std::size_t offset =
            (static_cast<std::size_t>(y) * fallback_atlas_.width +
             static_cast<std::size_t>(x)) * 4U;
        fallback_atlas_.rgba8[offset + 0U] = 255U;
        fallback_atlas_.rgba8[offset + 1U] = 255U;
        fallback_atlas_.rgba8[offset + 2U] = 255U;
        fallback_atlas_.rgba8[offset + 3U] = 255U;
    };

    for (int index = 0; index < kGlyphCount; ++index) {
        const char character = static_cast<char>(32 + index);
        const char* rows = GlyphRows(character);
        const int cell_x = (index % kAtlasColumns) * kCellSize;
        const int cell_y = (index / kAtlasColumns) * kCellSize;
        for (int y = 0; y < 7; ++y) {
            for (int x = 0; x < 5; ++x) {
                if (rows[y * 5 + x] == '1') put(cell_x + x, cell_y + y);
            }
        }
    }
    const int white_x = (kWhiteGlyph % kAtlasColumns) * kCellSize;
    const int white_y = (kWhiteGlyph / kAtlasColumns) * kCellSize;
    for (int y = 0; y < kCellSize; ++y) {
        for (int x = 0; x < kCellSize; ++x) put(white_x + x, white_y + y);
    }
    return fallback_atlas_.IsValid();
}

const Rect& TextRenderer::WhiteUv() const {
    if (font_.IsValid()) return font_.WhiteUvRect();
    static const Rect kFallbackWhite{
        static_cast<float>((kWhiteGlyph % kAtlasColumns) * kCellSize) /
            static_cast<float>(kAtlasColumns * kCellSize),
        static_cast<float>((kWhiteGlyph / kAtlasColumns) * kCellSize) /
            static_cast<float>(kAtlasRows * kCellSize),
        static_cast<float>(kCellSize) /
            static_cast<float>(kAtlasColumns * kCellSize),
        static_cast<float>(kCellSize) /
            static_cast<float>(kAtlasRows * kCellSize)
    };
    return kFallbackWhite;
}

void TextRenderer::DrawGlyphRun(SpriteBatch& batch, std::string_view text,
                                const Vec2& top_left, float pixel_scale,
                                const Color& color, int32_t order) const {
    float x = top_left.x;
    float y = top_left.y;
    const bool real_font = font_.IsValid();
    const float fallback_advance = 6.0f * pixel_scale;
    const float fallback_size = static_cast<float>(kCellSize) * pixel_scale;
    for (const char raw : text) {
        if (raw == '\n') {
            x = top_left.x;
            y -= LineHeight(pixel_scale);
            continue;
        }
        if (real_font) {
            const auto* glyph = font_.Find(static_cast<unsigned char>(raw));
            if (glyph == nullptr) continue;
            const float baseline = y -
                static_cast<float>(font_.Ascent()) * pixel_scale;
            const float glyph_x = x +
                static_cast<float>(glyph->bearing_x) * pixel_scale;
            const float glyph_y = baseline -
                static_cast<float>(glyph->bearing_y) * pixel_scale;
            if (glyph->width > 0 && glyph->height > 0) {
                SpriteDraw draw;
                draw.position = {glyph_x, glyph_y};
                draw.size = {
                    static_cast<float>(glyph->width) * pixel_scale,
                    static_cast<float>(glyph->height) * pixel_scale};
                draw.uv = glyph->uv;
                draw.color = LinearColor(color);
                draw.layer = SortLayer::UI;
                draw.sorting_order = order;
                draw.texture_id = kTextureId;
                draw.pivot = {0.0f, 1.0f};
                (void)batch.DrawSprite(draw);
            }
            x += static_cast<float>(glyph->advance) * pixel_scale;
        } else {
            char character = UpperAscii(raw);
            int index = static_cast<int>(static_cast<unsigned char>(character)) - 32;
            if (index < 0 || index >= kGlyphCount) index = '?' - 32;
            const int column = index % kAtlasColumns;
            const int row = index / kAtlasColumns;
            SpriteDraw draw;
            draw.position = {x, y};
            draw.size = {fallback_size, fallback_size};
            draw.uv = {
                static_cast<float>(column * kCellSize) /
                    static_cast<float>(fallback_atlas_.width),
                static_cast<float>(row * kCellSize) /
                    static_cast<float>(fallback_atlas_.height),
                static_cast<float>(kCellSize) /
                    static_cast<float>(fallback_atlas_.width),
                static_cast<float>(kCellSize) /
                    static_cast<float>(fallback_atlas_.height)
            };
            draw.color = LinearColor(color);
            draw.layer = SortLayer::UI;
            draw.sorting_order = order;
            draw.texture_id = kTextureId;
            draw.pivot = {0.0f, 1.0f};
            (void)batch.DrawSprite(draw);
            x += fallback_advance;
        }
    }
}

void TextRenderer::DrawText(SpriteBatch& batch, std::string_view text,
                            const Vec2& top_left, float pixel_scale,
                            Color color, int32_t order) const {
    if (pixel_scale <= 0.0f) return;
    if (!font_.IsValid() && !fallback_atlas_.IsValid()) return;
    DrawGlyphRun(batch, text, top_left, pixel_scale, color, order);
}

void TextRenderer::DrawText(SpriteBatch& batch, std::string_view text,
                            const Vec2& top_left, float pixel_scale,
                            const TextStyle& style, int32_t order,
                            TextAlign align, float alignment_width) const {
    if (pixel_scale <= 0.0f) return;
    if (!font_.IsValid() && !fallback_atlas_.IsValid()) return;
    const float width = TextWidth(text, pixel_scale);
    Vec2 start = top_left;
    if (align == TextAlign::Center) {
        start.x += (alignment_width - width) * 0.5f;
    } else if (align == TextAlign::Right) {
        start.x += alignment_width - width;
    }
    if (style.outline) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                DrawGlyphRun(batch, text,
                             {start.x + static_cast<float>(dx) * pixel_scale,
                              start.y + static_cast<float>(dy) * pixel_scale},
                             pixel_scale, style.shadow_color, order);
            }
        }
    } else if (style.shadow) {
        DrawGlyphRun(batch, text,
                     {start.x + style.shadow_offset.x * pixel_scale,
                      start.y - style.shadow_offset.y * pixel_scale},
                     pixel_scale, style.shadow_color, order);
    }
    DrawGlyphRun(batch, text, start, pixel_scale, style.color, order + 1);
}

void TextRenderer::DrawPanel(SpriteBatch& batch, const Vec2& top_left,
                             const Vec2& size, Color color,
                             int32_t order) const {
    DrawRect(batch, top_left, size, color, order);
}

void TextRenderer::DrawRect(SpriteBatch& batch, const Vec2& top_left,
                            const Vec2& size, Color color,
                            int32_t order) const {
    if (!font_.IsValid() && !fallback_atlas_.IsValid()) return;
    if (size.x <= 0.0f || size.y <= 0.0f) return;
    SpriteDraw draw;
    draw.position = top_left;
    draw.size = size;
    draw.uv = WhiteUv();
    draw.color = LinearColor(color);
    draw.layer = SortLayer::UI;
    draw.sorting_order = order;
    draw.texture_id = kTextureId;
    draw.pivot = {0.0f, 1.0f};
    (void)batch.DrawSprite(draw);
}

void TextRenderer::DrawLine(SpriteBatch& batch, const Vec2& start,
                            const Vec2& end, float width, Color color,
                            int32_t order) const {
    if (!font_.IsValid() && !fallback_atlas_.IsValid()) return;
    const Vec2 delta = end - start;
    const float length = delta.Length();
    if (length <= 0.0001f || width <= 0.0f) return;
    SpriteDraw draw;
    draw.position = start;
    draw.size = {length, width};
    draw.uv = WhiteUv();
    draw.color = LinearColor(color);
    draw.layer = SortLayer::UI;
    draw.sorting_order = order;
    draw.texture_id = kTextureId;
    draw.pivot = {0.0f, 0.5f};
    draw.rotation = std::atan2(delta.y, delta.x);
    (void)batch.DrawSprite(draw);
}

void TextRenderer::DrawPanelBorder(SpriteBatch& batch, const Vec2& top_left,
                                   const Vec2& size, Color fill, Color border,
                                   float border_width, int32_t order) const {
    if (size.x <= 0.0f || size.y <= 0.0f) return;
    DrawRect(batch, top_left, size, border, order);
    const float b = std::max(0.0f, border_width);
    DrawRect(batch, {top_left.x + b, top_left.y - b},
             {size.x - 2.0f * b, size.y - 2.0f * b}, fill, order + 1);
}

float TextRenderer::TextWidth(std::string_view text, float pixel_scale) const {
    if (pixel_scale <= 0.0f) return 0.0f;
    if (font_.IsValid()) {
        return font_.MeasureWidth(std::string(text), pixel_scale);
    }
    std::size_t longest = 0;
    std::size_t current = 0;
    for (const char c : text) {
        if (c == '\n') {
            longest = std::max(longest, current);
            current = 0;
        } else {
            ++current;
        }
    }
    longest = std::max(longest, current);
    return static_cast<float>(longest) * 6.0f * pixel_scale;
}

float TextRenderer::LineHeight(float pixel_scale) const {
    if (font_.IsValid()) {
        return static_cast<float>(font_.LineHeight()) * pixel_scale;
    }
    return 10.0f * pixel_scale;
}

float TextRenderer::MeasureHeight(std::string_view text, float pixel_scale) const {
    if (font_.IsValid()) {
        return static_cast<float>(font_.LineCount(std::string(text))) *
               LineHeight(pixel_scale);
    }
    std::size_t lines = 1;
    for (const char c : text) {
        if (c == '\n') ++lines;
    }
    return static_cast<float>(lines) * LineHeight(pixel_scale);
}

} // namespace soulknight::engine::rendering
