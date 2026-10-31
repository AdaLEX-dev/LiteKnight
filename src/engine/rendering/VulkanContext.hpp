// Soul Knight Linux — Vulkan instance/device/swapchain context
#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace soulknight::platform {
class Window;
}

namespace soulknight::engine::rendering {

struct VulkanInitOptions {
    bool enable_validation = false;
    bool vsync = true;
};

enum class FrameStatus {
    Ready,
    Skipped,
    Error
};

// Owns Vulkan objects only. It does not own SDL, gameplay state, or render-world
// data. The window/surface lifetime is external and outlives the context.
class VulkanContext {
public:
    static constexpr uint32_t kFramesInFlight = 2;

    VulkanContext() = default;
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    bool Initialize(platform::Window& window, const VulkanInitOptions& options);
    void Shutdown();

    // Waits for the current frame fence, acquires an image, and begins a render
    // pass. A Skipped result must not have EndFrame called.
    FrameStatus BeginFrame();
    bool EndFrame();

    bool RecreateSwapchain();
    void WaitIdle();

    VkInstance Instance() const { return instance_; }
    VkPhysicalDevice PhysicalDevice() const { return physical_device_; }
    VkDevice Device() const { return device_; }
    VkQueue GraphicsQueue() const { return graphics_queue_; }
    VkQueue PresentQueue() const { return present_queue_; }
    uint32_t GraphicsQueueFamily() const { return graphics_queue_family_; }
    uint32_t PresentQueueFamily() const { return present_queue_family_; }
    VkRenderPass RenderPass() const { return render_pass_; }
    VkFormat SwapchainFormat() const { return swapchain_format_; }
    VkExtent2D SwapchainExtent() const { return swapchain_extent_; }
    VkCommandBuffer CurrentCommandBuffer() const;
    uint32_t CurrentFrameIndex() const { return current_frame_; }
    uint64_t SwapchainSerial() const { return swapchain_serial_; }
    const std::string& DeviceName() const { return device_name_; }
    bool ValidationEnabled() const { return validation_enabled_; }

private:
    struct QueueFamilyIndices {
        int graphics = -1;
        int present = -1;
        bool Complete() const { return graphics >= 0 && present >= 0; }
    };

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    bool CreateInstance();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandResources();
    bool CreateSyncResources();

    void DestroySwapchainResources(bool destroy_render_pass);
    void DestroySyncResources();
    void DestroyCommandResources();

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
    SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device) const;
    bool SupportsRequiredDeviceExtensions(VkPhysicalDevice device) const;
    void LogDevice(VkPhysicalDevice device, const VkPhysicalDeviceProperties& properties) const;

    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT types,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void* user_data);

    platform::Window* window_ = nullptr;
    VulkanInitOptions options_{};

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_queue_family_ = 0;
    uint32_t present_queue_family_ = 0;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_views_;
    std::vector<VkFramebuffer> framebuffers_;
    VkFormat swapchain_format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent_{0, 0};
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkFormat render_pass_format_ = VK_FORMAT_UNDEFINED;
    uint64_t swapchain_serial_ = 0;
    bool swapchain_recreate_requested_ = false;
    bool swapchain_suboptimal_ = false;

    std::array<VkCommandPool, kFramesInFlight> command_pools_{};
    std::array<VkCommandBuffer, kFramesInFlight> command_buffers_{};
    std::array<VkSemaphore, kFramesInFlight> image_available_{};
    std::array<VkFence, kFramesInFlight> in_flight_fences_{};
    std::vector<VkSemaphore> render_finished_;

    uint32_t current_frame_ = 0;
    uint32_t image_index_ = 0;
    bool frame_begun_ = false;
    bool validation_enabled_ = false;
    bool initialized_ = false;
    std::string device_name_;
};

} // namespace soulknight::engine::rendering
