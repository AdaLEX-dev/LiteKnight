// Soul Knight Linux — PNG decoder implementation
#include "PngDecoder.hpp"

#include <png.h>

#include <fstream>
#include <utility>
#include <vector>

namespace soulknight::engine::assets {
namespace {

void SetError(std::string* error, const char* message) {
    if (error != nullptr) *error = message;
}

void SetLibpngError(std::string* error, const png_image& image) {
    if (error == nullptr) return;
    if (image.message[0] != '\0') {
        *error = image.message;
    } else {
        *error = "libpng decode failed";
    }
}

} // namespace

bool PngDecoder::Decode(const uint8_t* data, std::size_t size,
                        TextureData& output, std::string* error) {
    output.Clear();
    if (error != nullptr) error->clear();

    if (data == nullptr || size == 0) {
        SetError(error, "PNG input is empty");
        return false;
    }

    png_image image{};
    image.version = PNG_IMAGE_VERSION;

    if (png_image_begin_read_from_memory(&image, data, size) == 0) {
        SetLibpngError(error, image);
        png_image_free(&image);
        return false;
    }

    if (image.width == 0 || image.height == 0) {
        SetError(error, "PNG has zero dimensions");
        png_image_free(&image);
        return false;
    }
    if (image.width > kMaxDimension || image.height > kMaxDimension) {
        SetError(error, "PNG dimensions exceed the native decoder limit");
        png_image_free(&image);
        return false;
    }

    const uint64_t pixel_count = static_cast<uint64_t>(image.width) *
                                 static_cast<uint64_t>(image.height);
    const uint64_t byte_count = pixel_count * TextureData::kBytesPerPixel;
    if (byte_count == 0 || byte_count > kMaxPixelBytes) {
        SetError(error, "PNG pixel buffer exceeds the native decoder limit");
        png_image_free(&image);
        return false;
    }

    const uint32_t width = static_cast<uint32_t>(image.width);
    const uint32_t height = static_cast<uint32_t>(image.height);

    image.format = PNG_FORMAT_RGBA;
    std::vector<uint8_t> pixels(static_cast<std::size_t>(byte_count));

    if (png_image_finish_read(&image, nullptr, pixels.data(), 0, nullptr) == 0) {
        SetLibpngError(error, image);
        png_image_free(&image);
        return false;
    }
    png_image_free(&image);

    output.width = width;
    output.height = height;
    output.rgba8 = std::move(pixels);
    return true;
}

bool PngDecoder::DecodeFile(const std::filesystem::path& path,
                            TextureData& output, std::string* error) {
    output.Clear();
    if (error != nullptr) error->clear();

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        SetError(error, "PNG file could not be opened");
        return false;
    }

    const std::streamoff size = file.tellg();
    if (size <= 0) {
        SetError(error, "PNG file is empty");
        return false;
    }
    file.seekg(0, std::ios::beg);
    if (!file) {
        SetError(error, "PNG file seek failed");
        return false;
    }

    std::vector<uint8_t> bytes(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(size))) {
        SetError(error, "PNG file read failed");
        return false;
    }

    return Decode(bytes.data(), bytes.size(), output, error);
}

} // namespace soulknight::engine::assets
