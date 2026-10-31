// Soul Knight Linux — local recovered UI atlas registry
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

namespace soulknight::game::ui {

struct UIAssetSprite {
    std::string name;
    std::string id;
    uint32_t texture_id = 0;
    engine::Rect uv{0.0f, 0.0f, 1.0f, 1.0f};
    engine::Vec2 source_pixels{0.0f, 0.0f};
    float pixels_per_unit = 0.0f;
};

struct UIAssetTexture {
    uint32_t id = 0;
    std::string name;
    std::string path;
    engine::assets::TextureData data;
};

// Loads only locally generated UI metadata/pixels. The manifest is metadata;
// PNGs are reference-derived and gitignored.
class UIAssets {
public:
    static constexpr uint32_t kFirstTextureId = 0x60000000U;

    bool Load(const platform::FileSystem& filesystem,
              const std::string& manifest_path, std::string* error);
    const UIAssetSprite* Find(const std::string& name) const;
    const std::vector<UIAssetTexture>& Textures() const { return textures_; }
    bool IsLoaded() const { return loaded_; }

private:
    std::vector<UIAssetTexture> textures_;
    std::unordered_map<std::string, UIAssetSprite> sprites_;
    bool loaded_ = false;
};

} // namespace soulknight::game::ui
