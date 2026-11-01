// Soul Knight Linux — Vulkan renderer owner
#pragma once

#include "VulkanContext.hpp"
#include "SpriteRenderer.hpp"

#include <filesystem>

namespace soulknight::engine {
struct Camera2D;
}

namespace soulknight::platform {
class Window;
}

namespace soulknight::engine::rendering {

class SpriteBatch;

// Owns the Vulkan context and renderer-side resources. Gameplay supplies a
// SpriteBatch render world; it never receives a VkDevice or VkCommandBuffer.
class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    bool Initialize(platform::Window& window, const VulkanInitOptions& options,
                    const std::vector<TextureUpload>& textures,
                    const std::filesystem::path& shader_dir);
    void Shutdown();
    bool Render(SpriteBatch& batch, const Camera2D& camera);
    bool AddTextures(const std::vector<TextureUpload>& textures);

    const std::string& DeviceName() const { return context_.DeviceName(); }
    bool ValidationEnabled() const { return context_.ValidationEnabled(); }
    VkExtent2D SwapchainExtent() const { return context_.SwapchainExtent(); }
    VkFormat SwapchainFormat() const { return context_.SwapchainFormat(); }
    uint64_t SwapchainSerial() const { return context_.SwapchainSerial(); }
    bool IsReady() const { return initialized_; }

private:
    VulkanContext context_;
    SpriteRenderer sprite_renderer_;
    bool initialized_ = false;
};

} // namespace soulknight::engine::rendering
