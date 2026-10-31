#include "UIRenderer.hpp"

#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/ColorSpace.hpp"
#include "engine/rendering/SpriteBatch.hpp"

#include <algorithm>
#include <cmath>

namespace soulknight::game::ui {
namespace {

constexpr float kLogicalWidth = 1920.0f;
constexpr float kLogicalHeight = 1080.0f;

} // namespace

void UIRenderer::Begin(const UIRenderContext& context) {
    context_ = context;
    world_per_pixel_ = 0.0f;
    if (context_.camera == nullptr) return;
    world_per_pixel_ =
        (2.0f * context_.camera->HalfHeight()) / kLogicalHeight;
}

bool UIRenderer::Ready() const {
    return context_.camera != nullptr && context_.batch != nullptr &&
           context_.text != nullptr && world_per_pixel_ > 0.0f;
}

engine::Vec2 UIRenderer::ToWorld(const engine::Vec2& logical) const {
    if (context_.camera == nullptr) return {};
    const engine::Vec2 center = context_.camera->position;
    return {
        center.x + (logical.x - kLogicalWidth * 0.5f) * world_per_pixel_,
        center.y - (logical.y - kLogicalHeight * 0.5f) * world_per_pixel_
    };
}

float UIRenderer::FontScale(float size_pixels) const {
    // The recovered pixel_bold atlas is rasterized at 28 px for desktop
    // readability. Never render UI text smaller than that native size; large
    // titles scale up cleanly.
    return (std::max(28.0f, size_pixels) / 28.0f) * world_per_pixel_;
}

void UIRenderer::Rect(const engine::Rect& rect, engine::Color color,
                      int32_t order) const {
    if (!Ready()) return;
    context_.text->DrawRect(*context_.batch, ToWorld({rect.x, rect.y}),
                            {rect.w * world_per_pixel_,
                             rect.h * world_per_pixel_},
                            color, context_.order_base + order);
}

void UIRenderer::Panel(const engine::Rect& rect, engine::Color fill,
                       engine::Color border, float border_width,
                       int32_t order) const {
    if (!Ready()) return;
    context_.text->DrawPanelBorder(
        *context_.batch, ToWorld({rect.x, rect.y}),
        {rect.w * world_per_pixel_, rect.h * world_per_pixel_}, fill, border,
        border_width * world_per_pixel_, context_.order_base + order);
}

void UIRenderer::Text(std::string_view text, const engine::Vec2& top_left,
                      engine::Color color, int32_t order, bool shadow,
                      bool outline, float size_pixels) const {
    if (!Ready()) return;
    engine::rendering::TextStyle style;
    style.color = color;
    style.shadow = shadow;
    style.shadow_offset = {2.0f, 2.0f};
    style.shadow_color = engine::Color{0, 0, 0, 210};
    style.outline = outline;
    context_.text->DrawText(*context_.batch, text,
                            ToWorld({std::round(top_left.x),
                                     std::round(top_left.y)}),
                            FontScale(size_pixels), style,
                            context_.order_base + order);
}

void UIRenderer::TextAligned(std::string_view text, const engine::Rect& rect,
                             engine::Color color, int32_t order,
                             engine::rendering::TextAlign align,
                             float size_pixels, bool shadow) const {
    if (!Ready()) return;
    const float width = TextWidth(text, size_pixels);
    engine::Vec2 position{rect.x, rect.y + (rect.h - LineHeight(size_pixels)) * 0.5f};
    if (align == engine::rendering::TextAlign::Center) {
        position.x += (rect.w - width) * 0.5f;
    } else if (align == engine::rendering::TextAlign::Right) {
        position.x += rect.w - width;
    }
    Text(text, position, color, order, shadow, false, size_pixels);
}

void UIRenderer::TextCentered(std::string_view text, const engine::Rect& rect,
                              engine::Color color, int32_t order,
                              float size_pixels, bool shadow) const {
    TextAligned(text, rect, color, order,
                engine::rendering::TextAlign::Center, size_pixels, shadow);
}


namespace {
std::vector<std::string> WrapWords(std::string_view text, float max_width,
                                   const engine::rendering::TextRenderer& renderer,
                                   float scale) {
    std::vector<std::string> lines;
    std::string current;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find(' ', start);
        const std::string word(text.substr(start, end == std::string_view::npos
                                                    ? std::string_view::npos
                                                    : end - start));
        if (word.empty()) {
            if (end == std::string_view::npos) break;
            start = end + 1;
            continue;
        }
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (!current.empty() &&
            renderer.TextWidth(candidate, scale) > max_width) {
            lines.push_back(current);
            current = word;
        } else {
            current = candidate;
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    if (!current.empty()) lines.push_back(current);
    if (lines.empty()) lines.push_back(std::string(text));
    return lines;
}
} // namespace

float UIRenderer::TextWidthFitted(std::string_view text, const engine::Rect& rect,
                                  float preferred_size, float min_size) const {
    if (context_.text == nullptr) return 0.0f;
    const float world = world_per_pixel_;
    float size = std::max(28.0f, preferred_size);
    while (size >= min_size) {
        const float scale = (size / 28.0f) * world;
        if (context_.text->TextWidth(text, scale) <= rect.w &&
            context_.text->LineHeight(scale) <= rect.h) {
            return size;
        }
        size -= 1.0f;
    }
    return min_size;
}

float UIRenderer::TextFitted(std::string_view text, const engine::Rect& rect,
                             engine::Color color, int32_t order,
                             engine::rendering::TextAlign align,
                             float preferred_size, float min_size,
                             bool shadow) const {
    if (!Ready() || rect.w <= 0.0f || rect.h <= 0.0f) return 0.0f;
    const float world = world_per_pixel_;
    float size = std::max(28.0f, preferred_size);
    float scale = (size / 28.0f) * world;
    const float width = context_.text->TextWidth(text, scale);
    if (width <= rect.w && context_.text->LineHeight(scale) <= rect.h) {
        TextAligned(text, rect, color, order, align, size, shadow);
        return size;
    }
    // Keep the recovered primary font size and wrap at word boundaries.
    const std::vector<std::string> lines =
        WrapWords(text, rect.w, *context_.text, scale);
    const float line_height = context_.text->LineHeight(scale);
    if (static_cast<float>(lines.size()) * line_height <= rect.h &&
        lines.size() > 1) {
        const float total = static_cast<float>(lines.size()) * line_height;
        float y = rect.y + (rect.h - total) * 0.5f;
        for (const std::string& line : lines) {
            const float line_width = context_.text->TextWidth(line, scale);
            float x = rect.x;
            if (align == engine::rendering::TextAlign::Center) {
                x += (rect.w - line_width) * 0.5f;
            } else if (align == engine::rendering::TextAlign::Right) {
                x += rect.w - line_width;
            }
            Text(line, {std::round(x), std::round(y)}, color, order, shadow,
                 false, size);
            y += line_height;
        }
        return size;
    }
    // Last resort: shrink inside the content box rather than overflow it.
    while (size > min_size) {
        size -= 1.0f;
        scale = (size / 28.0f) * world;
        if (context_.text->TextWidth(text, scale) <= rect.w &&
            context_.text->LineHeight(scale) <= rect.h) {
            break;
        }
    }
    const float raw_width = context_.text->TextWidth(text, scale);
    engine::Vec2 position{
        rect.x, rect.y + (rect.h - context_.text->LineHeight(scale)) * 0.5f};
    if (align == engine::rendering::TextAlign::Center) {
        position.x += (rect.w - raw_width) * 0.5f;
    } else if (align == engine::rendering::TextAlign::Right) {
        position.x += rect.w - raw_width;
    }
    engine::rendering::TextStyle style;
    style.color = color;
    style.shadow = shadow;
    style.shadow_offset = {2.0f, 2.0f};
    style.shadow_color = engine::Color{0, 0, 0, 210};
    style.outline = false;
    context_.text->DrawText(*context_.batch, text,
                            ToWorld({std::round(position.x),
                                     std::round(position.y)}),
                            scale, style, context_.order_base + order);
    return size;
}
float UIRenderer::TextWidth(std::string_view text, float size_pixels) const {
    if (context_.text == nullptr) return 0.0f;
    return context_.text->TextWidth(text, FontScale(size_pixels));
}

float UIRenderer::LineHeight(float size_pixels) const {
    if (context_.text == nullptr) return 0.0f;
    return context_.text->LineHeight(FontScale(size_pixels));
}

void UIRenderer::Image(uint32_t texture_id, const engine::Rect& uv,
                       const engine::Rect& rect, engine::Color color,
                       int32_t order) const {
    if (!Ready() || texture_id == 0) return;
    engine::rendering::SpriteDraw draw;
    draw.position = ToWorld({rect.x, rect.y});
    draw.size = {rect.w * world_per_pixel_, rect.h * world_per_pixel_};
    draw.uv = uv;
    draw.color = engine::rendering::LinearColor(color);
    draw.layer = engine::rendering::SortLayer::UI;
    draw.sorting_order = context_.order_base + order;
    draw.texture_id = texture_id;
    draw.pivot = {0.0f, 1.0f};
    (void)context_.batch->DrawSprite(draw);
}

void UIRenderer::NineSlice(const UIAssetSprite& sprite,
                           const engine::Rect& rect, engine::Color color,
                           int32_t order, float border_pixels) const {
    if (!Ready() || sprite.texture_id == 0 || sprite.source_pixels.x <= 0.0f ||
        sprite.source_pixels.y <= 0.0f) {
        return;
    }
    const float sw = sprite.source_pixels.x;
    const float sh = sprite.source_pixels.y;
    const float sb = std::clamp(border_pixels, 0.0f,
                                std::min(sw, sh) * 0.45f);
    const float db = std::min({sb * (rect.w / sw), sb * (rect.h / sh),
                               rect.w * 0.33f, rect.h * 0.33f});
    const float u0 = sprite.uv.x;
    const float v0 = sprite.uv.y;
    const float uw = sprite.uv.w;
    const float uh = sprite.uv.h;
    const float su = sb / sw;
    const float sv = sb / sh;
    const float xs[4] = {rect.x, rect.x + db, rect.x + rect.w - db,
                         rect.x + rect.w};
    const float ys[4] = {rect.y, rect.y + db, rect.y + rect.h - db,
                         rect.y + rect.h};
    const float us[4] = {u0, u0 + uw * su, u0 + uw * (1.0f - su), u0 + uw};
    const float vs[4] = {v0, v0 + uh * sv, v0 + uh * (1.0f - sv), v0 + uh};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            // Avoid degenerate quads while still drawing the four corners and
            // stretched edge/center bands.
            const engine::Rect dst{xs[col], ys[row], xs[col + 1] - xs[col],
                                   ys[row + 1] - ys[row]};
            const engine::Rect uv{us[col], vs[row],
                                  us[col + 1] - us[col],
                                  vs[row + 1] - vs[row]};
            if (dst.w > 0.0f && dst.h > 0.0f) {
                Image(sprite.texture_id, uv, dst, color, order);
            }
        }
    }
}

void UIRenderer::SceneSprite(int32_t sprite_index, const engine::Rect& rect,
                             engine::Color color, int32_t order,
                             bool flip_x) const {
    if (!Ready() || context_.scene == nullptr || sprite_index < 0 ||
        static_cast<std::size_t>(sprite_index) >= context_.scene->sprites.size()) {
        return;
    }
    const auto& sprite =
        context_.scene->sprites[static_cast<std::size_t>(sprite_index)];
    engine::rendering::SpriteDraw draw;
    draw.position = ToWorld({rect.x, rect.y});
    draw.size = {rect.w * world_per_pixel_, rect.h * world_per_pixel_};
    draw.uv = sprite.UvRect();
    draw.color = engine::rendering::LinearColor(color);
    draw.layer = engine::rendering::SortLayer::UI;
    draw.sorting_order = context_.order_base + order;
    draw.texture_id = sprite.texture_id;
    draw.pivot = {0.0f, 1.0f};
    draw.flip_x = flip_x;
    (void)context_.batch->DrawSprite(draw);
}

} // namespace soulknight::game::ui
