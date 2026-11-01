// Soul Knight Linux — Vulkan renderer owner implementation
#include "VulkanRenderer.hpp"

#include "SpriteBatch.hpp"
#include "engine/core/Log.hpp"

namespace soulknight::engine::rendering {

VulkanRenderer::~VulkanRenderer() {
    Shutdown();
}

bool VulkanRenderer::Initialize(platform::Window& window, const VulkanInitOptions& options,
                                const std::vector<TextureUpload>& textures,
                                const std::filesystem::path& shader_dir) {
    if (initialized_) return true;

    if (!context_.Initialize(window, options)) {
        LOG_ERROR_RENDER("Vulkan context initialization failed");
        return false;
    }
    if (!sprite_renderer_.Initialize(context_, textures, shader_dir)) {
        LOG_ERROR_RENDER("Sprite renderer initialization failed");
        context_.WaitIdle();
        sprite_renderer_.Shutdown(context_);
        context_.Shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void VulkanRenderer::Shutdown() {
    if (!initialized_ && context_.Device() == VK_NULL_HANDLE) return;

    // Renderer-side resources can still be referenced by recorded/in-flight
    // command buffers. Wait before destroying pipelines, buffers, or samplers.
    context_.WaitIdle();
    sprite_renderer_.Shutdown(context_);
    context_.Shutdown();
    initialized_ = false;
}

bool VulkanRenderer::AddTextures(const std::vector<TextureUpload>& textures) {
    if (!initialized_) return false;
    return sprite_renderer_.AddTextures(context_, textures);
}

bool VulkanRenderer::Render(SpriteBatch& batch, const Camera2D& camera) {
    if (!initialized_) return false;

    const FrameStatus status = context_.BeginFrame();
    if (status == FrameStatus::Skipped) return true;
    if (status == FrameStatus::Error) return false;

    const bool sprites_ok = sprite_renderer_.Render(context_, batch, camera);
    const bool frame_ok = context_.EndFrame();
    if (!sprites_ok) {
        LOG_ERROR_RENDER("Sprite draw recording failed; frame was submitted without sprites");
        return false;
    }
    return frame_ok;
}

} // namespace soulknight::engine::rendering
