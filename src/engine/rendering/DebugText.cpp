// Soul Knight Linux — small native developer text renderer
#include "DebugText.hpp"

#include <algorithm>
#include <cctype>

namespace soulknight::engine::rendering {

namespace {

constexpr int kGlyphCount = 96; // ASCII 32..127
constexpr int kWhiteGlyph = 95; // ASCII 127, reserved as an 8x8 white block

char UpperAscii(char character) {
    const unsigned char value = static_cast<unsigned char>(character);
    if (value >= 'a' && value <= 'z') {
        return static_cast<char>(value - ('a' - 'A'));
    }
    return static_cast<char>(value);
}

} // namespace

const char* DebugTextRenderer::GlyphRows(char character) {
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

bool DebugTextRenderer::BuildAtlas() {
    atlas_.width = static_cast<uint32_t>(kAtlasColumns * kCellSize);
    atlas_.height = static_cast<uint32_t>(kAtlasRows * kCellSize);
    atlas_.rgba8.assign(atlas_.ExpectedByteSize(), 0);

    const auto put = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= static_cast<int>(atlas_.width) ||
            y >= static_cast<int>(atlas_.height)) {
            return;
        }
        const std::size_t offset =
            (static_cast<std::size_t>(y) * atlas_.width +
             static_cast<std::size_t>(x)) * 4U;
        atlas_.rgba8[offset + 0U] = 255U;
        atlas_.rgba8[offset + 1U] = 255U;
        atlas_.rgba8[offset + 2U] = 255U;
        atlas_.rgba8[offset + 3U] = 255U;
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

    // Reserve one cell as an opaque white rectangle for panels/separators.
    const int white_x = (kWhiteGlyph % kAtlasColumns) * kCellSize;
    const int white_y = (kWhiteGlyph / kAtlasColumns) * kCellSize;
    for (int y = 0; y < kCellSize; ++y) {
        for (int x = 0; x < kCellSize; ++x) put(white_x + x, white_y + y);
    }
    return atlas_.IsValid();
}

void DebugTextRenderer::DrawText(SpriteBatch& batch, std::string_view text,
                                 const Vec2& top_left, float pixel_scale,
                                 Color color, int32_t order) const {
    if (!atlas_.IsValid() || pixel_scale <= 0.0f) return;
    float x = top_left.x;
    float y = top_left.y;
    const float advance = 6.0f * pixel_scale;
    const float glyph_size = static_cast<float>(kCellSize) * pixel_scale;
    for (const char raw : text) {
        if (raw == '\n') {
            x = top_left.x;
            y -= LineHeight(pixel_scale);
            continue;
        }
        char character = UpperAscii(raw);
        int index = static_cast<int>(static_cast<unsigned char>(character)) - 32;
        if (index < 0 || index >= kGlyphCount) index = '?' - 32;
        const int column = index % kAtlasColumns;
        const int row = index / kAtlasColumns;
        SpriteDraw draw;
        draw.position = {x, y};
        draw.size = {glyph_size, glyph_size};
        draw.uv = {
            static_cast<float>(column * kCellSize) /
                static_cast<float>(atlas_.width),
            static_cast<float>(row * kCellSize) /
                static_cast<float>(atlas_.height),
            static_cast<float>(kCellSize) / static_cast<float>(atlas_.width),
            static_cast<float>(kCellSize) / static_cast<float>(atlas_.height)
        };
        draw.color = color;
        draw.layer = SortLayer::UI;
        draw.sorting_order = order;
        draw.texture_id = kTextureId;
        draw.pivot = {0.0f, 1.0f};
        (void)batch.DrawSprite(draw);
        x += advance;
    }
}

void DebugTextRenderer::DrawPanel(SpriteBatch& batch, const Vec2& top_left,
                                  const Vec2& size, Color color,
                                  int32_t order) const {
    if (!atlas_.IsValid()) return;
    const int column = kWhiteGlyph % kAtlasColumns;
    const int row = kWhiteGlyph / kAtlasColumns;
    SpriteDraw draw;
    draw.position = top_left;
    draw.size = size;
    draw.uv = {
        static_cast<float>(column * kCellSize) /
            static_cast<float>(atlas_.width),
        static_cast<float>(row * kCellSize) /
            static_cast<float>(atlas_.height),
        static_cast<float>(kCellSize) / static_cast<float>(atlas_.width),
        static_cast<float>(kCellSize) / static_cast<float>(atlas_.height)
    };
    draw.color = color;
    draw.layer = SortLayer::UI;
    draw.sorting_order = order;
    draw.texture_id = kTextureId;
    draw.pivot = {0.0f, 1.0f};
    (void)batch.DrawSprite(draw);
}

float DebugTextRenderer::TextWidth(std::string_view text, float pixel_scale) const {
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

} // namespace soulknight::engine::rendering
