// Soul Knight Linux — GPU texture
#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace soulknight::engine::rendering {

// Minimal RGBA8 texture path. PNG/KTX decoding is intentionally not part of this
// milestone; the renderer smoke test supplies generated RGBA8 bytes.
class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    bool CreateFromRGBA8(VkDevice device, VkPhysicalDevice physical_device,
                         VkQueue queue, uint32_t queue_family,
                         uint32_t width, uint32_t height,
                         const uint8_t* pixels);
    void Destroy(VkDevice device);

    bool IsValid() const { return image_ != VK_NULL_HANDLE && view_ != VK_NULL_HANDLE; }
    VkImageView View() const { return view_; }
    VkSampler Sampler() const { return sampler_; }
    uint32_t Width() const { return width_; }
    uint32_t Height() const { return height_; }

private:
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

} // namespace soulknight::engine::rendering
