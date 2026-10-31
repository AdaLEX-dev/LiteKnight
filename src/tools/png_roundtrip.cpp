// Soul Knight Linux — native PNG canonical-RGBA8 round-trip verifier
//
// Usage:
//   soul-knight-png-roundtrip <file.png> <expected.rgba8> [width height]
//
// This is a manual integration tool for converted assets. It uses the same
// PngDecoder path as the runtime; it does not know about Unity.

#include "engine/assets/PngDecoder.hpp"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {

bool ReadRaw(const char* path, std::vector<uint8_t>& output) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "cannot open expected RGBA8 file: " << path << '\n';
        return false;
    }
    const std::streamoff size = file.tellg();
    if (size <= 0) {
        std::cerr << "expected RGBA8 file is empty: " << path << '\n';
        return false;
    }
    file.seekg(0, std::ios::beg);
    output.resize(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(output.data()),
                   static_cast<std::streamsize>(size))) {
        std::cerr << "cannot read expected RGBA8 file: " << path << '\n';
        return false;
    }
    return true;
}

bool ParseDimension(const char* text, uint32_t& value) {
    const char* end = text + std::char_traits<char>::length(text);
    const std::from_chars_result result = std::from_chars(text, end, value);
    return result.ec == std::errc{} && result.ptr == end && value > 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 3 && argc != 5) {
        std::cerr << "usage: " << argv[0]
                  << " <file.png> <expected.rgba8> [width height]\n";
        return EXIT_FAILURE;
    }

    soulknight::engine::assets::TextureData texture;
    std::string error;
    if (!soulknight::engine::assets::PngDecoder::DecodeFile(argv[1], texture, &error)) {
        std::cerr << "decode failed: " << error << '\n';
        return EXIT_FAILURE;
    }

    if (argc == 5) {
        uint32_t expected_width = 0;
        uint32_t expected_height = 0;
        if (!ParseDimension(argv[3], expected_width) ||
            !ParseDimension(argv[4], expected_height)) {
            std::cerr << "invalid expected dimensions\n";
            return EXIT_FAILURE;
        }
        if (texture.width != expected_width || texture.height != expected_height) {
            std::cerr << "dimension mismatch: decoded " << texture.width << "x"
                      << texture.height << ", expected " << expected_width << "x"
                      << expected_height << '\n';
            return EXIT_FAILURE;
        }
    }

    std::vector<uint8_t> expected;
    if (!ReadRaw(argv[2], expected)) return EXIT_FAILURE;

    if (texture.rgba8.size() != expected.size()) {
        std::cerr << "RGBA8 size mismatch: decoded " << texture.rgba8.size()
                  << ", expected " << expected.size() << '\n';
        return EXIT_FAILURE;
    }
    if (texture.rgba8 != expected) {
        for (std::size_t i = 0; i < expected.size(); ++i) {
            if (texture.rgba8[i] != expected[i]) {
                std::cerr << "RGBA8 byte mismatch at offset " << i << ": decoded "
                          << static_cast<unsigned>(texture.rgba8[i]) << ", expected "
                          << static_cast<unsigned>(expected[i]) << '\n';
                break;
            }
        }
        return EXIT_FAILURE;
    }

    std::cout << "PASS: native PNG decode == canonical RGBA8 (" << texture.width
              << "x" << texture.height << ", " << texture.rgba8.size()
              << " bytes)\n";
    return EXIT_SUCCESS;
}
