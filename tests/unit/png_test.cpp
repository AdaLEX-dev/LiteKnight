// Soul Knight Linux — PNG decoder / canonical texture tests
#include <doctest/doctest.h>

#include "engine/assets/PngDecoder.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifndef SOUL_KNIGHT_TEST_ASSET_DIR
#define SOUL_KNIGHT_TEST_ASSET_DIR "assets/test"
#endif

using soulknight::engine::assets::PngDecoder;
using soulknight::engine::assets::TextureData;

namespace {

std::filesystem::path TestAsset(const char* name) {
    return std::filesystem::path(SOUL_KNIGHT_TEST_ASSET_DIR) / name;
}

bool DecodeAsset(const char* name, TextureData& texture, std::string& error) {
    return PngDecoder::DecodeFile(TestAsset(name), texture, &error);
}

} // namespace

TEST_CASE("PNG decoder produces exact canonical RGBA8 bytes" * doctest::test_suite("unit")) {
    TextureData texture;
    std::string error;
    CHECK_FALSE(texture.IsValid());
    CHECK(DecodeAsset("phase4_rgba.png", texture, error));
    if (!texture.IsValid()) return;

    CHECK(texture.width == 4);
    CHECK(texture.height == 4);
    CHECK(texture.ExpectedByteSize() == 64);
    CHECK(texture.rgba8.size() == texture.ExpectedByteSize());

    const std::vector<uint8_t> expected = {
        255, 0, 0, 255,       0, 255, 0, 128,     0, 0, 255, 0,       255, 255, 0, 255,
        0, 255, 255, 255,     255, 0, 255, 64,    255, 255, 255, 255, 0, 0, 0, 255,
        128, 64, 32, 255,     32, 64, 128, 255,   10, 20, 30, 255,   240, 230, 220, 128,
        0, 0, 0, 0,           255, 128, 0, 255,   0, 128, 255, 192, 16, 32, 48, 255
    };
    CHECK(texture.rgba8 == expected);

    TextureData repeated;
    std::string repeated_error;
    CHECK(DecodeAsset("phase4_rgba.png", repeated, repeated_error));
    CHECK(repeated.rgba8 == texture.rgba8);
    CHECK(repeated.width == texture.width);
    CHECK(repeated.height == texture.height);

    TextureData from_file;
    std::string file_error;
    CHECK(PngDecoder::DecodeFile(TestAsset("phase4_rgba.png"), from_file, &file_error));
    CHECK(from_file.rgba8 == texture.rgba8);
}

TEST_CASE("PNG decoder normalizes RGB, grayscale, gray-alpha and palette" * doctest::test_suite("unit")) {
    TextureData rgb;
    std::string error;
    CHECK(DecodeAsset("phase4_rgb.png", rgb, error));
    CHECK(rgb.width == 2);
    CHECK(rgb.height == 2);
    CHECK(rgb.rgba8 == std::vector<uint8_t>{
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255
    });

    TextureData gray;
    error.clear();
    CHECK(DecodeAsset("phase4_gray.png", gray, error));
    CHECK(gray.rgba8 == std::vector<uint8_t>{
        0, 0, 0, 255, 85, 85, 85, 255,
        170, 170, 170, 255, 255, 255, 255, 255
    });

    TextureData gray_alpha;
    error.clear();
    CHECK(DecodeAsset("phase4_gray_alpha.png", gray_alpha, error));
    CHECK(gray_alpha.rgba8 == std::vector<uint8_t>{
        0, 0, 0, 0, 85, 85, 85, 128,
        170, 170, 170, 255, 255, 255, 255, 64
    });

    TextureData palette;
    error.clear();
    CHECK(DecodeAsset("phase4_palette.png", palette, error));
    CHECK(palette.rgba8 == std::vector<uint8_t>{
        255, 0, 0, 255, 0, 255, 0, 128,
        0, 0, 255, 255, 0, 0, 0, 0
    });
}

TEST_CASE("PNG decoder rejects malformed input without a blank texture" * doctest::test_suite("unit")) {
    const uint8_t malformed[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52
    };

    TextureData texture;
    texture.width = 7;
    texture.height = 9;
    texture.rgba8 = {1, 2, 3, 4};
    std::string error;
    CHECK_FALSE(PngDecoder::Decode(malformed, sizeof(malformed), texture, &error));
    CHECK_FALSE(error.empty());
    CHECK(texture.width == 0);
    CHECK(texture.height == 0);
    CHECK(texture.rgba8.empty());
    CHECK_FALSE(texture.IsValid());
}

TEST_CASE("PNG decoder reports missing files explicitly" * doctest::test_suite("unit")) {
    TextureData texture;
    std::string error;
    CHECK_FALSE(PngDecoder::DecodeFile(
        std::filesystem::path("/definitely/not/a/soul-knight-test.png"), texture, &error));
    CHECK_FALSE(error.empty());
    CHECK_FALSE(texture.IsValid());
}
