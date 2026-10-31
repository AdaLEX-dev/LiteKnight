// Soul Knight Linux — 1920x1080 logical pixel UI painter
#pragma once

#include "engine/assets/SceneAsset.hpp"
#include "engine/math/Color.hpp"
#include "engine/math/Rect.hpp"
#include "engine/math/Vec2.hpp"
#include "engine/rendering/TextRenderer.hpp"
#include "UIAssets.hpp"

#include <string>
#include <string_view>

namespace soulknight::engine {
struct Camera2D;
}
namespace soulknight::engine::rendering {
class SpriteBatch;
}

namespace soulknight::game::ui {

struct UIRenderContext {
    engine::Camera2D* camera = nullptr;
    engine::rendering::SpriteBatch* batch = nullptr;
    engine::rendering::TextRenderer* text = nullptr;
    const engine::assets::SceneAsset* scene = nullptr;
    int32_t order_base = 1000;
};

// Maps the immutable 1920x1080 logical space to world space through the active
// camera. All public coordinates are logical pixels with a top-left origin.
class UIRenderer {
public:
    void Begin(const UIRenderContext& context);
    bool Ready() const;

    engine::Vec2 ToWorld(const engine::Vec2& logical) const;
    float PixelScale() const { return world_per_pixel_; }
    float FontScale(float size_pixels) const;

    void Rect(const engine::Rect& rect, engine::Color color,
              int32_t order = 0) const;
    void Panel(const engine::Rect& rect, engine::Color fill,
               engine::Color border, float border_width = 2.0f,
               int32_t order = 0) const;
    void Text(std::string_view text, const engine::Vec2& top_left,
              engine::Color color, int32_t order = 0,
              bool shadow = false, bool outline = false,
              float size_pixels = 28.0f) const;
    void TextAligned(std::string_view text, const engine::Rect& rect,
                     engine::Color color, int32_t order = 0,
                     engine::rendering::TextAlign align =
                         engine::rendering::TextAlign::Left,
                     float size_pixels = 28.0f, bool shadow = false) const;
    void TextCentered(std::string_view text, const engine::Rect& rect,
                      engine::Color color, int32_t order = 0,
                      float size_pixels = 28.0f, bool shadow = false) const;
    float TextWidth(std::string_view text, float size_pixels = 28.0f) const;
    float LineHeight(float size_pixels = 28.0f) const;
    void Image(uint32_t texture_id, const engine::Rect& uv,
               const engine::Rect& rect, engine::Color color,
               int32_t order = 0) const;
    void NineSlice(const UIAssetSprite& sprite, const engine::Rect& rect,
                   engine::Color color, int32_t order = 0,
                   float border_pixels = 5.0f) const;
    void SceneSprite(int32_t sprite_index, const engine::Rect& rect,
                     engine::Color color, int32_t order = 0,
                     bool flip_x = false) const;

private:
    UIRenderContext context_{};
    float world_per_pixel_ = 0.0f;
};

} // namespace soulknight::game::ui
