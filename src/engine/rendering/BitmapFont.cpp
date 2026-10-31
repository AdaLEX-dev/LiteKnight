#include "BitmapFont.hpp"

#include "engine/assets/PngDecoder.hpp"
#include "platform/filesystem/FileSystem.hpp"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION
#endif
#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>

namespace soulknight::engine::rendering {
namespace {

bool ReadInt(const nlohmann::json& object, const char* key, int* out) {
    if (!object.contains(key) || !object[key].is_number_integer()) return false;
    *out = object[key].get<int>();
    return true;
}

bool ReadRect(const nlohmann::json& object, const char* key, engine::Rect* out) {
    if (!object.contains(key) || !object[key].is_array() ||
        object[key].size() != 4) {
        return false;
    }
    for (std::size_t i = 0; i < 4; ++i) {
        if (!object[key][i].is_number()) return false;
    }
    out->x = object[key][0].get<float>();
    out->y = object[key][1].get<float>();
    out->w = object[key][2].get<float>();
    out->h = object[key][3].get<float>();
    return true;
}

} // namespace

bool BitmapFont::Load(const platform::FileSystem& filesystem,
                      const std::string& json_path, std::string* error) {
    if (error != nullptr) error->clear();
    valid_ = false;
    glyphs_.clear();
    const auto text = filesystem.ReadTextFile(json_path);
    if (!text) {
        if (error != nullptr) *error = "font metrics missing: " + json_path;
        return false;
    }
    nlohmann::json document = nlohmann::json::parse(*text, nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        if (error != nullptr) *error = "font metrics JSON is corrupt";
        return false;
    }
    if (!ReadInt(document, "ascent", &ascent_) ||
        !ReadInt(document, "descent", &descent_) ||
        !ReadInt(document, "line_height", &line_height_) ||
        !ReadRect(document, "white_rect", &white_uv_)) {
        if (error != nullptr) *error = "font metrics fields are invalid";
        return false;
    }
    if (line_height_ <= 0) {
        if (error != nullptr) *error = "font line height must be positive";
        return false;
    }
    if (!document.contains("texture") || !document["texture"].is_string() ||
        !document.contains("glyphs") || !document["glyphs"].is_array()) {
        if (error != nullptr) *error = "font atlas references are invalid";
        return false;
    }
    const std::string texture_name = document["texture"].get<std::string>();
    const std::filesystem::path json_dir =
        std::filesystem::path(json_path).parent_path();
    const std::string texture_path =
        (json_dir / texture_name).generic_string();
    const auto png = filesystem.ReadFile(texture_path);
    if (!png) {
        if (error != nullptr) *error = "font atlas missing: " + texture_path;
        return false;
    }
    if (!engine::assets::PngDecoder::Decode(png->data(), png->size(), atlas_,
                                            error)) {
        return false;
    }
    if (atlas_.width > 0 && atlas_.height > 0 && white_uv_.w > 1.0f) {
        white_uv_ = {
            white_uv_.x / static_cast<float>(atlas_.width),
            white_uv_.y / static_cast<float>(atlas_.height),
            white_uv_.w / static_cast<float>(atlas_.width),
            white_uv_.h / static_cast<float>(atlas_.height)
        };
    }
    for (const auto& item : document["glyphs"]) {
        if (!item.is_object()) continue;
        Glyph glyph;
        glyph.codepoint = item.value("codepoint", 0U);
        if (!ReadRect(item, "uv", &glyph.uv)) {
            // Derive the UV from the explicit atlas pixel rectangle.
            int x = 0;
            int y = 0;
            int w = 0;
            int h = 0;
            if (ReadInt(item, "x", &x) && ReadInt(item, "y", &y) &&
                ReadInt(item, "width", &w) && ReadInt(item, "height", &h) &&
                atlas_.width > 0 && atlas_.height > 0) {
                glyph.uv = {
                    static_cast<float>(x) / static_cast<float>(atlas_.width),
                    static_cast<float>(y) / static_cast<float>(atlas_.height),
                    static_cast<float>(w) / static_cast<float>(atlas_.width),
                    static_cast<float>(h) / static_cast<float>(atlas_.height)
                };
            }
        }
        if (!ReadInt(item, "width", &glyph.width) ||
            !ReadInt(item, "height", &glyph.height) ||
            !ReadInt(item, "bearing_x", &glyph.bearing_x) ||
            !ReadInt(item, "bearing_y", &glyph.bearing_y) ||
            !ReadInt(item, "advance", &glyph.advance)) {
            continue;
        }
        if (glyph.width < 0 || glyph.height < 0 || glyph.advance < 0) continue;
        glyphs_[glyph.codepoint] = glyph;
    }
    valid_ = !glyphs_.empty() && atlas_.IsValid();
    if (!valid_) {
        if (error != nullptr) *error = "font atlas decoded with no valid glyphs";
        return false;
    }
    return true;
}

const BitmapFont::Glyph* BitmapFont::Find(uint32_t codepoint) const {
    const auto it = glyphs_.find(codepoint);
    return it == glyphs_.end() ? nullptr : &it->second;
}

float BitmapFont::MeasureWidth(const std::string& text,
                               float pixel_scale) const {
    if (!valid_ || pixel_scale <= 0.0f) return 0.0f;
    float longest = 0.0f;
    float line = 0.0f;
    for (const char raw : text) {
        if (raw == '\n') {
            if (line > longest) longest = line;
            line = 0.0f;
            continue;
        }
        const auto* glyph = Find(static_cast<unsigned char>(raw));
        line += static_cast<float>(glyph != nullptr ? glyph->advance : 0) *
                pixel_scale;
    }
    return line > longest ? line : longest;
}

std::size_t BitmapFont::LineCount(const std::string& text) const {
    std::size_t lines = 1;
    for (const char c : text) {
        if (c == '\n') ++lines;
    }
    return lines;
}

} // namespace soulknight::engine::rendering
