// Soul Knight Linux — Batched textured sprite renderer
#pragma once

#include "Texture2D.hpp"
#include "engine/math/Mat4.hpp"
#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace soulknight::engine::assets {
struct TextureData;
}

namespace soulknight::engine::rendering {

class SpriteBatch;
class VulkanContext;

class SpriteRenderer {
public:
    SpriteRenderer() = default;
    ~SpriteRenderer();

    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    bool Initialize(VulkanContext& context, const soulknight::engine::assets::TextureData& texture,
                    const std::filesystem::path& shader_dir);
    void Shutdown(VulkanContext& context);
    bool Render(VulkanContext& context, SpriteBatch& batch);

    bool IsReady() const { return initialized_; }

private:
    struct SpriteVertex {
        float x;
        float y;
        float u;
        float v;
        float r;
        float g;
        float b;
        float a;
    };

    struct FrameBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
    };

    static constexpr std::size_t kMaxQuads = 4096;

    bool CreateDescriptorResources(VulkanContext& context);
    bool CreatePipeline(VulkanContext& context);
    bool CreateFrameBuffers(VulkanContext& context);
    bool CreateIndexBuffer(VulkanContext& context);
    bool CreateTexture(VulkanContext& context, const soulknight::engine::assets::TextureData& texture);
    bool UpdateDescriptor(VulkanContext& context);
    bool CreateHostVisibleBuffer(VulkanContext& context, VkDeviceSize size,
                                 VkBufferUsageFlags usage, VkBuffer& buffer,
                                 VkDeviceMemory& memory, void** mapped) const;

    std::vector<char> LoadShaderFile(const char* filename) const;
    static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code);

    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkFormat pipeline_format_ = VK_FORMAT_UNDEFINED;
    uint64_t pipeline_serial_ = 0;

    std::array<FrameBuffer, 2> frame_buffers_{};
    VkBuffer index_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory index_memory_ = VK_NULL_HANDLE;
    Texture2D texture_;
    std::filesystem::path shader_dir_;
    std::vector<SpriteVertex> vertices_;

    bool initialized_ = false;
};

} // namespace soulknight::engine::rendering
