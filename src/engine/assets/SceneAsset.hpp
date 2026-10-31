// Soul Knight Linux — native scene data
#pragma once

#include "SpriteData.hpp"
#include "TextureData.hpp"
#include "engine/animation/AnimationClip.hpp"
#include "engine/math/Color.hpp"
#include "engine/math/Vec2.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace soulknight::engine::assets {

struct TextureAsset {
    uint32_t id = 0;
    std::string name;
    std::string png_path;
    TextureData data;
    TextureFilter filter = TextureFilter::Point;
    TextureWrap wrap = TextureWrap::Clamp;
};

struct SpriteInstance {
    uint32_t sprite_index = 0;
    Vec2 position{0.0f, 0.0f};
    float rotation = 0.0f; // radians, Y-up
    Vec2 scale{1.0f, 1.0f};
    Color color = Color::White();
    bool flip_x = false;
    bool flip_y = false;
    int32_t sorting_layer = 0;
    int32_t sorting_order = 0;
    std::string path; // source hierarchy path, evidence only
};

struct SceneCamera {
    Vec2 position{0.0f, 0.0f};
    float orthographic_size = 10.0f;
    float viewport_width = 1920.0f;
    float viewport_height = 1080.0f;
};

struct SceneAsset {
    std::string id;
    std::vector<TextureAsset> textures;
    std::vector<SpriteData> sprites;
    std::vector<SpriteInstance> instances;
    std::vector<engine::animation::AnimationClip> animations;
    SceneCamera camera;
    Vec2 spawn{0.0f, 0.0f};
    Rect collision_bounds{0.0f, 0.0f, 0.0f, 0.0f};

    int32_t FindSpriteIndex(const std::string& sprite_id) const {
        for (std::size_t i = 0; i < sprites.size(); ++i) {
            if (sprites[i].id == sprite_id) return static_cast<int32_t>(i);
        }
        return -1;
    }
};

} // namespace soulknight::engine::assets
