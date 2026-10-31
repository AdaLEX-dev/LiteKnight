// Soul Knight Linux — Vulkan Context
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <engine/core/Types.hpp>

namespace soulknight::engine {

struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    uint32_t graphics_queue_family = UINT32_MAX;
    uint32_t present_queue_family = UINT32_MAX;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_views;
    VkFormat swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D swapchain_extent = {1920, 1080};

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;

    // Frame resources (triple-buffered)
    struct FrameData {
        VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkSemaphore image_available = VK_NULL_HANDLE;
        VkSemaphore render_finished = VK_NULL_HANDLE;
    } frames[3];

    uint32_t current_frame = 0;
    bool initialized = false;

    bool Initialize(void* sdl_window);
    void Shutdown();
    void Resize(uint32_t width, uint32_t height);
    bool BeginFrame();
    void EndFrame();
};

} // namespace soulknight::engine