// LiteKnight — locked-down reference texture / sprite / prefab gallery.
#include "engine/assets/PngDecoder.hpp"
#include "engine/assets/SceneAsset.hpp"
#include "engine/rendering/Camera2D.hpp"
#include "engine/rendering/SpriteBatch.hpp"
#include "engine/rendering/TextRenderer.hpp"
#include "engine/rendering/VulkanRenderer.hpp"
#include "platform/filesystem/FileSystem.hpp"
#include "platform/wayland/Window.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

namespace {

using Json = nlohmann::json;
using soulknight::engine::Camera2D;
using soulknight::engine::Vec2;
using soulknight::engine::assets::PngDecoder;
using soulknight::engine::assets::TextureData;
using soulknight::engine::rendering::SortLayer;
using soulknight::engine::rendering::SpriteBatch;
using soulknight::engine::rendering::SpriteDraw;
using soulknight::engine::rendering::TextRenderer;
using soulknight::engine::rendering::TextureUpload;
using soulknight::engine::rendering::VulkanRenderer;
using soulknight::platform::FileSystem;
using soulknight::platform::Window;
using soulknight::platform::WindowConfig;

namespace engine = soulknight::engine;

constexpr float kPixelsPerUnit = 100.0f;
constexpr float kHalfWidth = 9.6f;
constexpr float kHalfHeight = 5.4f;

std::filesystem::path ResolveAsset(const FileSystem& filesystem,
                                   const std::string& path) {
    const auto resolved = filesystem.ResolvePath(path);
    if (!resolved.empty()) return resolved;
    std::error_code error;
    const std::filesystem::path executable =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) return {};
    const std::filesystem::path fallback =
        executable.parent_path().parent_path() / "assets" / path;
    if (std::filesystem::exists(fallback)) return fallback;
    return {};
}

struct GalleryTexture {
    std::string key;
    std::string name;
    std::string source;
    std::string scene;
    std::string path;
    int width = 0;
    int height = 0;
    TextureData data;
    uint32_t texture_id = 0;
    bool valid = false;
    std::string error;
};

struct GallerySprite {
    std::string key;
    std::string name;
    std::string scene;
    std::string texture_key;
    std::string source_name;
    float rect[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float pivot[2] = {0.5f, 0.5f};
    float ppu = 16.0f;
    bool valid = false;
    std::string error;
};

struct GalleryPart {
    std::string sprite_key;
    Vec2 offset{0.0f, 0.0f};
    Vec2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
    int sorting_layer = 5;
    int sorting_order = 0;
    bool flip_x = false;
    bool flip_y = false;
};

struct GalleryPrefab {
    std::string key;
    std::string name;
    std::string type;
    std::string scene;
    std::string source;
    std::vector<GalleryPart> parts;
    bool valid = false;
    std::string error;
};

struct Entry {
    enum class Type { Texture, Sprite, Prefab };
    Type type = Type::Texture;
    std::size_t index = 0;
};

std::string Format(float value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.4g", static_cast<double>(value));
    return buffer;
}

class Gallery {
public:
    bool LoadManifest(FileSystem& filesystem, const std::string& path) {
        const auto text = filesystem.ReadTextFile(path);
        if (!text) {
            std::cerr << "gallery: manifest not found: " << path << "\n";
            return false;
        }
        Json document = Json::parse(*text, nullptr, false);
        if (document.is_discarded() || !document.is_object()) {
            std::cerr << "gallery: manifest is not valid JSON\n";
            return false;
        }
        for (const Json& item : document["textures"]) {
            GalleryTexture texture;
            texture.key = item.value("key", "");
            texture.name = item.value("name", texture.key);
            texture.source = item.value("source", "");
            texture.scene = item.value("scene", "");
            texture.path = item.value("path", "");
            texture.width = item.value("width", 0);
            texture.height = item.value("height", 0);
            const std::filesystem::path resolved =
                ResolveAsset(filesystem, texture.path);
            if (resolved.empty()) {
                texture.error = "PNG missing: " + texture.path;
            } else if (!PngDecoder::DecodeFile(resolved, texture.data,
                                               &texture.error)) {
                texture.error = "PNG decode failed: " + texture.error;
            } else {
                texture.width = static_cast<int>(texture.data.width);
                texture.height = static_cast<int>(texture.data.height);
                texture.valid = texture.data.IsValid();
            }
            texture.texture_id =
                0x100000u + static_cast<uint32_t>(textures_.size());
            texture_indices_[texture.key] = textures_.size();
            textures_.push_back(std::move(texture));
        }
        for (const Json& item : document["sprites"]) {
            GallerySprite sprite;
            sprite.key = item.value("key", "");
            sprite.name = item.value("name", sprite.key);
            sprite.scene = item.value("scene", "");
            sprite.texture_key = item.value("texture_key", "");
            sprite.source_name = item.value("source_name", "");
            sprite.ppu = item.value("ppu", 0.0f);
            if (item.contains("rect") && item["rect"].is_array() &&
                item["rect"].size() == 4) {
                for (int i = 0; i < 4; ++i) {
                    sprite.rect[i] = item["rect"][
                        static_cast<std::size_t>(i)].get<float>();
                }
            }
            if (item.contains("pivot") && item["pivot"].is_array() &&
                item["pivot"].size() == 2) {
                sprite.pivot[0] = item["pivot"][0].get<float>();
                sprite.pivot[1] = item["pivot"][1].get<float>();
            }
            const auto texture_it = texture_indices_.find(sprite.texture_key);
            if (texture_it == texture_indices_.end()) {
                sprite.error = "texture key missing: " + sprite.texture_key;
            } else {
                const GalleryTexture& texture = textures_[texture_it->second];
                if (!texture.valid) {
                    sprite.error = "texture is broken";
                } else if (sprite.rect[2] <= 0.0f || sprite.rect[3] <= 0.0f ||
                           sprite.rect[0] < 0.0f || sprite.rect[1] < 0.0f ||
                           sprite.rect[0] + sprite.rect[2] >
                               static_cast<float>(texture.width) + 0.01f ||
                           sprite.rect[1] + sprite.rect[3] >
                               static_cast<float>(texture.height) + 0.01f) {
                    sprite.error = "sprite rect outside texture";
                } else if (!std::isfinite(sprite.pivot[0]) ||
                           !std::isfinite(sprite.pivot[1]) ||
                           sprite.pivot[0] < 0.0f || sprite.pivot[0] > 1.0f ||
                           sprite.pivot[1] < 0.0f || sprite.pivot[1] > 1.0f) {
                    sprite.error = "sprite pivot invalid";
                } else if (!std::isfinite(sprite.ppu) || sprite.ppu <= 0.0f) {
                    sprite.error = "sprite PPU invalid";
                } else {
                    sprite.valid = true;
                }
            }
            sprite_indices_[sprite.key] = sprites_.size();
            sprites_.push_back(std::move(sprite));
        }
        for (const Json& item : document["prefabs"]) {
            GalleryPrefab prefab;
            prefab.key = item.value("key", "");
            prefab.name = item.value("name", prefab.key);
            prefab.type = item.value("type", "");
            prefab.scene = item.value("scene", "");
            prefab.source = item.value("source", "");
            prefab.valid = true;
            if (item.contains("parts") && item["parts"].is_array()) {
                for (const Json& part_item : item["parts"]) {
                    GalleryPart part;
                    part.sprite_key = part_item.value("sprite_key", "");
                    if (part_item.contains("offset") &&
                        part_item["offset"].is_array() &&
                        part_item["offset"].size() == 2) {
                        part.offset = {part_item["offset"][0].get<float>(),
                                       part_item["offset"][1].get<float>()};
                    }
                    if (part_item.contains("scale") &&
                        part_item["scale"].is_array() &&
                        part_item["scale"].size() == 2) {
                        part.scale = {part_item["scale"][0].get<float>(),
                                      part_item["scale"][1].get<float>()};
                    }
                    part.rotation = part_item.value("rotation", 0.0f);
                    part.sorting_layer = part_item.value("sorting_layer", 5);
                    part.sorting_order = part_item.value("sorting_order", 0);
                    part.flip_x = part_item.value("flip_x", false);
                    part.flip_y = part_item.value("flip_y", false);
                    const auto it = sprite_indices_.find(part.sprite_key);
                    if (it == sprite_indices_.end() ||
                        !sprites_[it->second].valid) {
                        prefab.valid = false;
                        prefab.error = "prefab sprite missing/broken: " +
                                       part.sprite_key;
                        break;
                    }
                    if (!std::isfinite(part.offset.x) ||
                        !std::isfinite(part.offset.y) ||
                        !std::isfinite(part.scale.x) ||
                        !std::isfinite(part.scale.y) ||
                        !std::isfinite(part.rotation)) {
                        prefab.valid = false;
                        prefab.error = "prefab transform is not finite";
                        break;
                    }
                    prefab.parts.push_back(std::move(part));
                }
            }
            if (prefab.parts.empty() && prefab.error.empty()) {
                prefab.valid = false;
                prefab.error = "prefab has no visual parts";
            }
            prefab_indices_[prefab.key] = prefabs_.size();
            prefabs_.push_back(std::move(prefab));
        }
        for (const Json& item : document["entries"]) {
            Entry entry;
            const std::string type = item.value("type", "");
            const std::string key = item.value("key", "");
            if (type == "texture") {
                const auto it = texture_indices_.find(key);
                if (it == texture_indices_.end()) continue;
                entry.type = Entry::Type::Texture;
                entry.index = it->second;
            } else if (type == "sprite") {
                const auto it = sprite_indices_.find(key);
                if (it == sprite_indices_.end()) continue;
                entry.type = Entry::Type::Sprite;
                entry.index = it->second;
            } else if (type == "prefab") {
                const auto it = prefab_indices_.find(key);
                if (it == prefab_indices_.end()) continue;
                entry.type = Entry::Type::Prefab;
                entry.index = it->second;
            } else {
                continue;
            }
            entries_.push_back(entry);
        }
        return !entries_.empty();
    }

    bool ValidateAll() const {
        std::size_t broken = 0;
        for (const GalleryTexture& texture : textures_) {
            if (!texture.valid) {
                ++broken;
                std::cout << "[FAIL] texture " << texture.key << " ("
                          << texture.path << "): " << texture.error << "\n";
            }
        }
        for (const GallerySprite& sprite : sprites_) {
            if (!sprite.valid) {
                ++broken;
                std::cout << "[FAIL] sprite " << sprite.key << " ("
                          << sprite.name << "): " << sprite.error << "\n";
            }
        }
        for (const GalleryPrefab& prefab : prefabs_) {
            if (!prefab.valid) {
                ++broken;
                std::cout << "[FAIL] prefab " << prefab.key << " ("
                          << prefab.name << "): " << prefab.error << "\n";
            }
        }
        std::cout << "\nASSET AUDIT\n"
                  << "Total entries: " << entries_.size() << "\n"
                  << "Textures: " << textures_.size() << "\n"
                  << "Sprites: " << sprites_.size() << "\n"
                  << "Prefabs: " << prefabs_.size() << "\n"
                  << "Broken: " << broken << "\n";
        return broken == 0;
    }

    const std::vector<GalleryTexture>& Textures() const { return textures_; }
    std::vector<TextureUpload> Uploads() const {
        std::vector<TextureUpload> uploads;
        uploads.reserve(textures_.size());
        for (const GalleryTexture& texture : textures_) {
            if (!texture.valid) continue;
            uploads.push_back(TextureUpload{
                texture.texture_id, &texture.data,
                soulknight::engine::assets::TextureFilter::Point,
                soulknight::engine::assets::TextureWrap::Clamp});
        }
        return uploads;
    }

    std::size_t Count() const { return entries_.size(); }
    const Entry& At(std::size_t index) const { return entries_[index]; }
    const GalleryTexture& TextureAt(std::size_t index) const {
        return textures_[index];
    }
    const GallerySprite& SpriteAt(std::size_t index) const {
        return sprites_[index];
    }
    const GalleryPrefab& PrefabAt(std::size_t index) const {
        return prefabs_[index];
    }
    const GalleryTexture& TextureForSprite(const GallerySprite& sprite) const {
        const auto it = texture_indices_.find(sprite.texture_key);
        const std::size_t index =
            it == texture_indices_.end() ? 0 : it->second;
        return textures_[index];
    }
    const GallerySprite& Sprite(const std::string& key) const {
        return sprites_[sprite_indices_.at(key)];
    }

private:
    std::vector<GalleryTexture> textures_;
    std::vector<GallerySprite> sprites_;
    std::vector<GalleryPrefab> prefabs_;
    std::vector<Entry> entries_;
    std::unordered_map<std::string, std::size_t> texture_indices_;
    std::unordered_map<std::string, std::size_t> sprite_indices_;
    std::unordered_map<std::string, std::size_t> prefab_indices_;
};

void DrawCheckerboard(SpriteBatch& batch, const TextRenderer& text) {
    const float cell = 0.4f;
    int row = 0;
    for (float y = -kHalfHeight; y < kHalfHeight; y += cell, ++row) {
        int column = 0;
        for (float x = -kHalfWidth; x < kHalfWidth; x += cell, ++column) {
            const uint8_t tone = ((row + column) % 2 == 0) ? 112 : 136;
            text.DrawRect(batch, {x, y}, {cell + 0.002f, cell + 0.002f},
                          {tone, tone, tone, 255}, 1);
        }
    }
}

void DrawSprite(SpriteBatch& batch, const Gallery& gallery,
                const GallerySprite& sprite, const Vec2& center,
                float display_scale, int order) {
    if (!sprite.valid) return;
    const GalleryTexture& texture = gallery.TextureForSprite(sprite);
    const engine::Rect uv{
        sprite.rect[0] / static_cast<float>(texture.width),
        sprite.rect[1] / static_cast<float>(texture.height),
        sprite.rect[2] / static_cast<float>(texture.width),
        sprite.rect[3] / static_cast<float>(texture.height)};
    SpriteDraw draw;
    draw.size = {sprite.rect[2] / kPixelsPerUnit * display_scale,
                 sprite.rect[3] / kPixelsPerUnit * display_scale};
    draw.position = center;
    draw.uv = uv;
    draw.texture_id = texture.texture_id;
    draw.pivot = {sprite.pivot[0], sprite.pivot[1]};
    draw.layer = SortLayer::UI;
    draw.sorting_order = order;
    batch.DrawSprite(draw);
}

void DrawEntry(SpriteBatch& batch, const Gallery& gallery, const Entry& entry) {
    if (entry.type == Entry::Type::Texture) {
        const auto& texture = gallery.TextureAt(entry.index);
        if (!texture.valid) return;
        const float max_w = 17.0f;
        const float max_h = 7.4f;
        float scale = 1.0f / kPixelsPerUnit;
        scale = std::min(max_w / static_cast<float>(texture.width),
                         max_h / static_cast<float>(texture.height));
        scale = std::min(scale, 1.0f / kPixelsPerUnit * 8.0f);
        if (scale < 1.0f / kPixelsPerUnit) scale = 1.0f / kPixelsPerUnit;
        SpriteDraw draw;
        draw.size = {static_cast<float>(texture.width) * scale,
                     static_cast<float>(texture.height) * scale};
        draw.position = {0.0f, 0.7f};
        draw.uv = {0.0f, 0.0f, 1.0f, 1.0f};
        draw.texture_id = texture.texture_id;
        draw.layer = SortLayer::UI;
        draw.sorting_order = 20;
        batch.DrawSprite(draw);
    } else if (entry.type == Entry::Type::Sprite) {
        const auto& sprite = gallery.SpriteAt(entry.index);
        if (!sprite.valid) return;
        const float max_pixels = std::max(sprite.rect[2], sprite.rect[3]);
        const float display_scale = max_pixels < 96.0f ? 4.0f : 1.0f;
        DrawSprite(batch, gallery, sprite, {0.0f, 0.7f}, display_scale, 20);
    } else {
        const auto& prefab = gallery.PrefabAt(entry.index);
        if (!prefab.valid) return;
        float min_x = 0.0f, max_x = 0.0f, min_y = 0.0f, max_y = 0.0f;
        bool first = true;
        for (const auto& part : prefab.parts) {
            const auto& sprite = gallery.Sprite(part.sprite_key);
            const float w = sprite.rect[2] / sprite.ppu * part.scale.x;
            const float h = sprite.rect[3] / sprite.ppu * part.scale.y;
            const float px = part.offset.x;
            const float py = part.offset.y;
            min_x = first ? px : std::min(min_x, px);
            max_x = first ? px + w : std::max(max_x, px + w);
            min_y = first ? py : std::min(min_y, py);
            max_y = first ? py + h : std::max(max_y, py + h);
            first = false;
        }
        const float span_x = std::max(0.05f, max_x - min_x);
        const float span_y = std::max(0.05f, max_y - min_y);
        const float fit = std::min(17.0f / span_x, 7.4f / span_y);
        const float scale = std::min(fit, 8.0f);
        const Vec2 origin{-0.5f * (min_x + max_x) * scale,
                          0.7f - 0.5f * (min_y + max_y) * scale};
        for (const auto& part : prefab.parts) {
            const auto& sprite = gallery.Sprite(part.sprite_key);
            const GalleryTexture& texture = gallery.TextureForSprite(sprite);
            SpriteDraw draw;
            draw.position = origin + Vec2{part.offset.x * scale,
                                          part.offset.y * scale};
            draw.size = {sprite.rect[2] / sprite.ppu * part.scale.x * scale,
                         sprite.rect[3] / sprite.ppu * part.scale.y * scale};
            draw.uv = {sprite.rect[0] / static_cast<float>(texture.width),
                       sprite.rect[1] / static_cast<float>(texture.height),
                       sprite.rect[2] / static_cast<float>(texture.width),
                       sprite.rect[3] / static_cast<float>(texture.height)};
            draw.texture_id = texture.texture_id;
            draw.pivot = {sprite.pivot[0], sprite.pivot[1]};
            draw.rotation = part.rotation;
            draw.flip_x = part.flip_x;
            draw.flip_y = part.flip_y;
            // Keep the prefab parts above the neutral viewport background.
            // Relative order within the prefab is preserved.
            draw.layer = SortLayer::UI;
            draw.sorting_order = 10 + part.sorting_order;
            batch.DrawSprite(draw);
        }
    }
}

std::string EntryTypeName(const Entry& entry) {
    if (entry.type == Entry::Type::Texture) return "Texture2D";
    if (entry.type == Entry::Type::Sprite) return "Sprite";
    return "Prefab";
}

struct Snapshot {
    std::size_t index = 0;
    std::string type;
    std::string name;
    std::string source;
    std::string texture;
    std::string size;
    std::string pivot;
    std::string ppu;
    std::string status;
};

Snapshot Describe(const Gallery& gallery, std::size_t index) {
    Snapshot snapshot;
    snapshot.index = index;
    const Entry& entry = gallery.At(index);
    snapshot.type = EntryTypeName(entry);
    if (entry.type == Entry::Type::Texture) {
        const auto& texture = gallery.TextureAt(entry.index);
        snapshot.name = texture.name + " [" + texture.key + "]";
        snapshot.source = texture.source + " / " + texture.scene + " / " +
                          texture.path;
        snapshot.texture = texture.key;
        snapshot.size = std::to_string(texture.width) + " x " +
                        std::to_string(texture.height) + " px";
        snapshot.pivot = "-";
        snapshot.ppu = "-";
        snapshot.status = texture.valid ? "included" : "BROKEN: " + texture.error;
    } else if (entry.type == Entry::Type::Sprite) {
        const auto& sprite = gallery.SpriteAt(entry.index);
        snapshot.name = sprite.name + " [" + sprite.key + "]";
        snapshot.source = sprite.scene + " / " + sprite.source_name;
        snapshot.texture = sprite.texture_key;
        snapshot.size = Format(sprite.rect[2]) + " x " +
                        Format(sprite.rect[3]) + " px @ " +
                        Format(sprite.rect[0]) + "," +
                        Format(sprite.rect[1]);
        snapshot.pivot = Format(sprite.pivot[0]) + ", " +
                         Format(sprite.pivot[1]);
        snapshot.ppu = Format(sprite.ppu);
        snapshot.status = sprite.valid ? "included" : "BROKEN: " + sprite.error;
    } else {
        const auto& prefab = gallery.PrefabAt(entry.index);
        snapshot.name = prefab.name + " [" + prefab.key + "]";
        snapshot.source = prefab.type + " / " + prefab.scene + " / " +
                          prefab.source;
        snapshot.texture = std::to_string(prefab.parts.size()) + " part(s)";
        snapshot.size = "-";
        snapshot.pivot = "-";
        snapshot.ppu = "-";
        snapshot.status = prefab.valid ? "included" : "BROKEN: " + prefab.error;
    }
    return snapshot;
}

void DrawTextLine(SpriteBatch& batch, const TextRenderer& text,
                  const std::string& line, float x, float y, float scale,
                  engine::Color color, int order) {
    text.DrawText(batch, line, {x, y}, scale, color, order);
}

std::string Truncate(std::string value, std::size_t maximum = 118) {
    if (value.size() <= maximum) return value;
    return value.substr(0, maximum - 3) + "...";
}

bool HasWaylandOnly(Window& window) {
    (void)window;
    const char* driver = SDL_GetCurrentVideoDriver();
    return driver != nullptr && std::string(driver) == "wayland";
}

} // namespace

int main(int argc, char** argv) {
    bool validate_all = false;
    std::size_t start_index = 0;
    std::string manifest_path = "reference_gallery_manifest.json";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--validate-all") {
            validate_all = true;
        } else if (arg == "--manifest" && i + 1 < argc) {
            manifest_path = argv[++i];
        } else if (arg == "--index" && i + 1 < argc) {
            start_index = static_cast<std::size_t>(
                std::strtoull(argv[++i], nullptr, 10));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "liteknight-texture-gallery [--validate-all] "
                         "[--manifest <asset-path>] [--index <n>]\n";
            return 0;
        }
    }

    FileSystem filesystem;
    if (!filesystem.Initialize()) {
        std::cerr << "gallery: filesystem initialization failed\n";
        return 1;
    }
    const std::filesystem::path resolved_manifest =
        ResolveAsset(filesystem, manifest_path);
    if (resolved_manifest.empty()) {
        std::cerr << "gallery: manifest not found: " << manifest_path << "\n";
        return 1;
    }
    Gallery gallery;
    if (!gallery.LoadManifest(filesystem, resolved_manifest.string())) return 1;
    std::cout << "Gallery asset enumeration: " << gallery.Count()
              << " entries\n";
    if (validate_all) {
        return gallery.ValidateAll() ? 0 : 1;
    }

    (void)SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    (void)SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cerr << "gallery: SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        std::cerr << "gallery: SDL_Vulkan_LoadLibrary failed: "
                  << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }
    WindowConfig window_config;
    window_config.title = "LiteKnight Reference Texture Gallery";
    window_config.width = 1920;
    window_config.height = 1080;
    window_config.resizable = true;
    Window window(window_config);
    if (!window.Initialize()) {
        std::cerr << "gallery: window initialization failed\n";
        SDL_Quit();
        return 1;
    }
    if (!HasWaylandOnly(window)) {
        std::cerr << "gallery: Wayland-only contract violated\n";
        window.Shutdown();
        SDL_Quit();
        return 1;
    }

    TextRenderer text;
    std::string font_error;
    const std::filesystem::path font_path =
        ResolveAsset(filesystem, "ui/font/pixel_bold.json");
    if (font_path.empty() ||
        !text.LoadPixelFont(filesystem, font_path.string(), &font_error)) {
        std::cerr << "gallery: pixel font unavailable (" << font_error
                  << "); using fallback atlas\n";
        if (!text.BuildFont()) {
            std::cerr << "gallery: fallback font build failed\n";
            return 1;
        }
    }
    std::vector<TextureUpload> uploads = gallery.Uploads();
    uploads.push_back(TextureUpload{TextRenderer::kTextureId, &text.Atlas(),
                                    soulknight::engine::assets::TextureFilter::Point,
                                    soulknight::engine::assets::TextureWrap::Clamp});
    const std::filesystem::path shader_dir = ResolveAsset(filesystem, "shaders");
    if (shader_dir.empty()) {
        std::cerr << "gallery: shader directory missing\n";
        return 1;
    }
    VulkanRenderer renderer;
    soulknight::engine::rendering::VulkanInitOptions options;
    options.vsync = true;
    if (!renderer.Initialize(window, options, uploads, shader_dir)) {
        std::cerr << "gallery: Vulkan initialization failed\n";
        return 1;
    }
    Camera2D camera;
    camera.position = {0.0f, 0.0f};
    camera.orthographic_size = kHalfHeight;
    camera.viewport_width = 1920.0f;
    camera.viewport_height = 1080.0f;

    std::size_t index = std::min(start_index, gallery.Count() - 1);
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            window.HandleEvent(event);
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type != SDL_EVENT_KEY_DOWN) continue;
            switch (event.key.key) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_LEFT:
                case SDLK_A:
                    if (index == 0) index = gallery.Count() - 1;
                    else --index;
                    break;
                case SDLK_RIGHT:
                case SDLK_D:
                case SDLK_SPACE:
                    index = (index + 1) % gallery.Count();
                    break;
                case SDLK_HOME: index = 0; break;
                case SDLK_END: index = gallery.Count() - 1; break;
                default: break;
            }
        }
        if (window.ShouldClose()) running = false;
        if (!running) break;
        if (window.IsMinimized()) {
            SDL_Delay(16);
            continue;
        }

        SpriteBatch batch;
        DrawCheckerboard(batch, text);
        // Asset viewport.
        text.DrawRect(batch, {-9.1f, -3.9f}, {18.2f, 8.9f},
                      {18, 22, 28, 230}, 5);
        text.DrawPanelBorder(batch, {-9.1f, -3.9f}, {18.2f, 8.9f},
                             {18, 22, 28, 230}, {92, 108, 130, 255}, 0.02f, 6);
        const Entry& entry = gallery.At(index);
        DrawEntry(batch, gallery, entry);
        if ((entry.type == Entry::Type::Texture &&
             !gallery.TextureAt(entry.index).valid) ||
            (entry.type == Entry::Type::Sprite &&
             !gallery.SpriteAt(entry.index).valid) ||
            (entry.type == Entry::Type::Prefab &&
             !gallery.PrefabAt(entry.index).valid)) {
            DrawTextLine(batch, text, "BROKEN ASSET - see metadata",
                         -4.5f, 0.8f, 0.022f, {255, 110, 110, 255}, 30);
        }

        const Snapshot snapshot = Describe(gallery, index);
        text.DrawRect(batch, {-9.4f, -5.35f}, {18.8f, 1.28f},
                      {8, 10, 14, 245}, 40);
        text.DrawPanelBorder(batch, {-9.4f, -5.35f}, {18.8f, 1.28f},
                             {8, 10, 14, 245}, {92, 108, 130, 255}, 0.015f, 41);
        DrawTextLine(batch, text,
                     "REFERENCE TEXTURE GALLERY   " +
                         std::to_string(index + 1) + " / " +
                         std::to_string(gallery.Count()),
                     -9.15f, 5.12f, 0.010f, {235, 225, 205, 255}, 50);
        DrawTextLine(batch, text,
                     Truncate(snapshot.name, 78) + "   |   " + snapshot.type,
                     -9.15f, 4.83f, 0.0080f, {240, 240, 240, 255}, 50);
        DrawTextLine(batch, text, "Source: " + Truncate(snapshot.source),
                     -9.15f, -4.12f, 0.0058f, {190, 200, 210, 255}, 50);
        DrawTextLine(batch, text,
                     "Texture: " + Truncate(snapshot.texture, 70) +
                         "   Size: " + snapshot.size +
                         "   Pivot: " + snapshot.pivot +
                         "   PPU: " + snapshot.ppu,
                     -9.15f, -4.32f, 0.0058f, {190, 200, 210, 255}, 50);
        DrawTextLine(batch, text, "Status: " + Truncate(snapshot.status, 90),
                     -9.15f, -4.52f, 0.0058f,
                     snapshot.status.rfind("BROKEN", 0) == 0
                         ? engine::Color{255, 120, 120, 255}
                         : engine::Color{150, 220, 160, 255},
                     50);
        DrawTextLine(batch, text,
                     "Left/A prev   Right/D or Space next   Home first   "
                     "End last   Esc quit",
                     -9.15f, -4.78f, 0.0058f, {170, 180, 195, 255}, 50);

        renderer.Render(batch, camera);
        window.ClearResizeRequest();
        window.SyncRuntimeState();
        if (window.ShouldClose()) running = false;
    }
    renderer.Shutdown();
    window.Shutdown();
    SDL_Vulkan_LoadLibrary(nullptr);
    SDL_Quit();
    return 0;
}
