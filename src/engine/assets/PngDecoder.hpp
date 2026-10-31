// Soul Knight Linux — PNG decoder boundary
#pragma once

#include "TextureData.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace soulknight::engine::assets {

// Decodes a PNG from memory or a file into canonical RGBA8 TextureData.
// libpng's simplified reader normalizes RGB, RGBA, grayscale, grayscale+alpha,
// palette and tRNS images to PNG_FORMAT_RGBA.
//
// On failure the output is cleared, false is returned, and `error` (when
// supplied) receives a stable human-readable explanation. No blank texture is
// ever returned as a successful decode.
class PngDecoder {
public:
    static constexpr uint32_t kMaxDimension = 16384;
    static constexpr uint64_t kMaxPixelBytes = 512ULL * 1024ULL * 1024ULL;

    static bool Decode(const uint8_t* data, std::size_t size,
                       TextureData& output, std::string* error = nullptr);
    static bool DecodeFile(const std::filesystem::path& path,
                           TextureData& output, std::string* error = nullptr);
};

} // namespace soulknight::engine::assets
