// Soul Knight Linux — Batched textured sprite renderer
#pragma once

#include "Texture2D.hpp"
#include "engine/math/Mat4.hpp"
#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

namespace soulknight::engine {
struct Camera2D;
}

namespace soulknight::engine::rendering {

class SpriteBatch;
class VulkanContext;

struct TextureUpload {
    uint32_t id = 0;
    const soulknight::engine::assets::TextureData* data = nullptr;
    soulknight::engine::assets::TextureFilter filter =
        soulknight::engine::assets::TextureFilter::Point;
    soulknight::engine::assets::TextureWrap wrap =
        soulknight::engine::assets::TextureWrap::Clamp;
};

class SpriteRenderer {
public:
    SpriteRenderer() = default;
    ~SpriteRenderer();

    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    bool Initialize(VulkanContext& context,
                    const std::vector<TextureUpload>& textures,
                    const std::filesystem::path& shader_dir);
    // Adds textures discovered after initialization (dungeon level loading).
    // Descriptor capacity is reserved at initialization.
    bool AddTextures(VulkanContext& context,
                     const std::vector<TextureUpload>& textures);
    void Shutdown(VulkanContext& context);
    bool Render(VulkanContext& context, SpriteBatch& batch, const Camera2D& camera);

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

    static constexpr std::size_t kMaxQuads = 16384;

    bool CreateDescriptorResources(VulkanContext& context, std::size_t texture_count);
    bool CreatePipeline(VulkanContext& context);
    bool CreateFrameBuffers(VulkanContext& context);
    bool CreateIndexBuffer(VulkanContext& context);
    bool CreateTextures(VulkanContext& context, const std::vector<TextureUpload>& textures);
    bool UpdateDescriptor(VulkanContext& context, std::size_t texture_index);
    bool CreateHostVisibleBuffer(VulkanContext& context, VkDeviceSize size,
                                 VkBufferUsageFlags usage, VkBuffer& buffer,
                                 VkDeviceMemory& memory, void** mapped) const;

    std::vector<char> LoadShaderFile(const char* filename) const;
    static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code);

    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkFormat pipeline_format_ = VK_FORMAT_UNDEFINED;
    uint64_t pipeline_serial_ = 0;

    std::array<FrameBuffer, 2> frame_buffers_{};
    VkBuffer index_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory index_memory_ = VK_NULL_HANDLE;
    std::vector<std::unique_ptr<Texture2D>> textures_;
    std::vector<VkDescriptorSet> descriptor_sets_;
    std::unordered_map<uint32_t, std::size_t> texture_indices_;
    std::filesystem::path shader_dir_;
    std::vector<SpriteVertex> vertices_;

    bool initialized_ = false;
};

} // namespace soulknight::engine::rendering
