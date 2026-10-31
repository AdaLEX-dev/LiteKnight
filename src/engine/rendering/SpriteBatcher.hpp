// Soul Knight Linux — Sprite Batcher
#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "engine/math/Vec2.hpp"
#include "engine/core/Types.hpp"

namespace soulknight::engine {

struct SpriteInstance {
    struct Mat4 {
        float m[16];
    };

    Mat4 transform;
    uint32_t color = 0xFFFFFFFF;
    float uv_x = 0.0f, uv_y = 0.0f, uv_w = 1.0f, uv_h = 1.0f;
    uint32_t texture_index = 0;
    uint32_t flags = 0;
};

struct TextureHandle {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    uint32_t width = 0, height = 0;
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
};

class SpriteBatcher {
public:
    static constexpr size_t MAX_INSTANCES = 10000;
    static constexpr size_t MAX_TEXTURES = 256;

    SpriteBatcher() = default;
    ~SpriteBatcher() = default;

    void Initialize(VkDevice device, VmaAllocator allocator);
    void Shutdown();

    void BeginFrame();
    void Draw(const SpriteInstance& instance);
    void Flush(VkCommandBuffer cmd);
    void EndFrame();

    TextureHandle CreateTexture(const void* data, uint32_t width, uint32_t height, VkFormat format);
    TextureHandle LoadKTX2(const char* path);

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;

    std::vector<SpriteInstance> instances_;
    std::vector<TextureHandle> textures_;

    VkBuffer instance_buffer_ = VK_NULL_HANDLE;
    VmaAllocation instance_allocation_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;

    size_t instance_count_ = 0;
    size_t texture_count_ = 0;
};

} // namespace soulknight::engine