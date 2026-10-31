// Soul Knight Linux — Vulkan instance/device/swapchain implementation
#include "VulkanContext.hpp"

#include "engine/core/Log.hpp"
#include "platform/wayland/Window.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace soulknight::engine::rendering {
namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

bool HasInstanceLayer(const char* name) {
    uint32_t count = 0;
    if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkLayerProperties> layers(count);
    if (vkEnumerateInstanceLayerProperties(&count, layers.data()) != VK_SUCCESS) return false;
    for (const VkLayerProperties& layer : layers) {
        if (std::strcmp(layer.layerName, name) == 0) return true;
    }
    return false;
}

bool HasInstanceExtension(const char* name) {
    uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS) return false;
    for (const VkExtensionProperties& extension : extensions) {
        if (std::strcmp(extension.extensionName, name) == 0) return true;
    }
    return false;
}

const char* DeviceTypeName(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
        default: return "other";
    }
}

const char* PresentModeName(VkPresentModeKHR mode) {
    switch (mode) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "immediate";
        case VK_PRESENT_MODE_MAILBOX_KHR: return "mailbox";
        case VK_PRESENT_MODE_FIFO_KHR: return "fifo";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "fifo-relaxed";
        case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR: return "shared-demand";
        case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR: return "shared-continuous";
        default: return "unknown";
    }
}

const char* FormatName(VkFormat format) {
    switch (format) {
        case VK_FORMAT_B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
        case VK_FORMAT_R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
        default: return "other";
    }
}

} // namespace

VulkanContext::~VulkanContext() {
    Shutdown();
}

bool VulkanContext::Initialize(platform::Window& window, const VulkanInitOptions& options) {
    if (initialized_) return true;
    window_ = &window;
    options_ = options;

    if (!CreateInstance()) {
        Shutdown();
        return false;
    }
    if (!CreateSurface()) {
        Shutdown();
        return false;
    }
    if (!PickPhysicalDevice()) {
        Shutdown();
        return false;
    }
    if (!CreateLogicalDevice()) {
        Shutdown();
        return false;
    }
    if (!CreateSwapchain()) {
        Shutdown();
        return false;
    }
    if (!CreateRenderPass()) {
        Shutdown();
        return false;
    }
    if (!CreateFramebuffers()) {
        Shutdown();
        return false;
    }
    if (!CreateCommandResources()) {
        Shutdown();
        return false;
    }
    if (!CreateSyncResources()) {
        Shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void VulkanContext::Shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(device_);
    }

    DestroySyncResources();
    DestroyCommandResources();
    DestroySwapchainResources(true);

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        SDL_Vulkan_DestroySurface(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    if (debug_messenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        auto destroy_messenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy_messenger != nullptr) {
            destroy_messenger(instance_, debug_messenger_, nullptr);
        }
        debug_messenger_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    physical_device_ = VK_NULL_HANDLE;
    graphics_queue_ = VK_NULL_HANDLE;
    present_queue_ = VK_NULL_HANDLE;
    graphics_queue_family_ = 0;
    present_queue_family_ = 0;
    current_frame_ = 0;
    image_index_ = 0;
    frame_begun_ = false;
    validation_enabled_ = false;
    initialized_ = false;
    device_name_.clear();
}

bool VulkanContext::CreateInstance() {
    uint32_t sdl_extension_count = 0;
    char const* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
    if (sdl_extensions == nullptr || sdl_extension_count == 0) {
        LOG_ERROR_RENDER("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        return false;
    }

    std::vector<const char*> extensions;
    extensions.reserve(sdl_extension_count + 1);
    bool has_surface = false;
    bool has_wayland_surface = false;
    for (uint32_t i = 0; i < sdl_extension_count; ++i) {
        const char* extension = sdl_extensions[i];
        extensions.push_back(extension);
        if (std::strcmp(extension, VK_KHR_SURFACE_EXTENSION_NAME) == 0) has_surface = true;
        if (std::strcmp(extension, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) == 0) has_wayland_surface = true;
    }

    if (!has_surface || !has_wayland_surface) {
        LOG_FATAL_RENDER("SDL did not provide VK_KHR_surface + VK_KHR_wayland_surface");
        return false;
    }

    std::vector<const char*> layers;
    if (options_.enable_validation) {
        const bool layer_available = HasInstanceLayer(kValidationLayer);
        const bool debug_utils_available = HasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if (layer_available && debug_utils_available) {
            layers.push_back(kValidationLayer);
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            validation_enabled_ = true;
        } else {
            LOG_WARN_RENDER("Vulkan validation requested but unavailable "
                            "(layer={}, debug_utils={})",
                            layer_available, debug_utils_available);
        }
    }

    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "Soul Knight";
    application_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    application_info.pEngineName = "SoulKnightNative";
    application_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    application_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    const VkResult result = vkCreateInstance(&create_info, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkCreateInstance failed: {}", static_cast<int>(result));
        return false;
    }

    LOG_INFO_RENDER("Vulkan instance: api=1.3, extensions={}, validation={}",
                    extensions.size(), validation_enabled_);

    if (validation_enabled_) {
        VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
        messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messenger_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messenger_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messenger_info.pfnUserCallback = DebugCallback;

        auto create_messenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        if (create_messenger == nullptr) {
            LOG_WARN_RENDER("vkCreateDebugUtilsMessengerEXT unavailable");
            validation_enabled_ = false;
        } else {
            const VkResult messenger_result = create_messenger(
                instance_, &messenger_info, nullptr, &debug_messenger_);
            if (messenger_result != VK_SUCCESS) {
                LOG_WARN_RENDER("Debug messenger creation failed: {}",
                                static_cast<int>(messenger_result));
                validation_enabled_ = false;
                debug_messenger_ = VK_NULL_HANDLE;
            }
        }
    }

    return true;
}

bool VulkanContext::CreateSurface() {
    surface_ = window_->CreateVulkanSurface(instance_);
    if (surface_ == VK_NULL_HANDLE) {
        LOG_ERROR_RENDER("Vulkan surface creation failed");
        return false;
    }
    return true;
}

bool VulkanContext::SupportsRequiredDeviceExtensions(VkPhysicalDevice device) const {
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS) {
        return false;
    }
    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data()) != VK_SUCCESS) {
        return false;
    }
    for (const VkExtensionProperties& extension : extensions) {
        if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

VulkanContext::QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        VkBool32 present_support = VK_FALSE;
        const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present_support);
        const bool supports_present = result == VK_SUCCESS && present_support == VK_TRUE;

        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && indices.graphics < 0) {
            indices.graphics = static_cast<int>(i);
        }
        if (supports_present && indices.present < 0) {
            indices.present = static_cast<int>(i);
        }
    }
    return indices;
}

VulkanContext::SwapchainSupportDetails VulkanContext::QuerySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupportDetails details;

    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities) != VK_SUCCESS) {
        return details;
    }

    uint32_t format_count = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, nullptr) == VK_SUCCESS &&
        format_count > 0) {
        details.formats.resize(format_count);
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, details.formats.data()) != VK_SUCCESS) {
            details.formats.clear();
        }
    }

    uint32_t present_count = 0;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_count, nullptr) == VK_SUCCESS &&
        present_count > 0) {
        details.present_modes.resize(present_count);
        if (vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_count, details.present_modes.data()) != VK_SUCCESS) {
            details.present_modes.clear();
        }
    }

    return details;
}

void VulkanContext::LogDevice(VkPhysicalDevice device, const VkPhysicalDeviceProperties& properties) const {
    LOG_INFO_RENDER("GPU: {} [{}] vendor=0x{:04x} device=0x{:04x}",
                    properties.deviceName, DeviceTypeName(properties.deviceType),
                    properties.vendorID, properties.deviceID);
    LOG_INFO_RENDER("  api={}.{}.{} driver=0x{:08x}",
                    VK_VERSION_MAJOR(properties.apiVersion),
                    VK_VERSION_MINOR(properties.apiVersion),
                    VK_VERSION_PATCH(properties.apiVersion),
                    properties.driverVersion);

    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families.data());
    const QueueFamilyIndices indices = FindQueueFamilies(device);
    for (uint32_t i = 0; i < family_count; ++i) {
        const bool graphics = (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        const bool present = indices.present == static_cast<int>(i);
        LOG_INFO_RENDER("  queue_family[{}]: count={} graphics={} present={}",
                        i, families[i].queueCount, graphics, present);
    }

    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceFeatures(device, &features);
    LOG_INFO_RENDER("  features: geometry={} anisotropy={} non_solid_fill={} wide_lines={}",
                    features.geometryShader, features.samplerAnisotropy,
                    features.fillModeNonSolid, features.wideLines);
}

bool VulkanContext::PickPhysicalDevice() {
    uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(instance_, &count, nullptr) != VK_SUCCESS || count == 0) {
        LOG_ERROR_RENDER("No Vulkan physical devices found");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(count);
    if (vkEnumeratePhysicalDevices(instance_, &count, devices.data()) != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkEnumeratePhysicalDevices failed");
        return false;
    }

    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    int best_score = -1;
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        LogDevice(device, properties);

        if (properties.apiVersion < VK_API_VERSION_1_3) {
            LOG_WARN_RENDER("  rejected: Vulkan 1.3 required");
            continue;
        }

        const QueueFamilyIndices indices = FindQueueFamilies(device);
        if (!indices.Complete()) {
            LOG_WARN_RENDER("  rejected: graphics/present queue family missing");
            continue;
        }
        if (!SupportsRequiredDeviceExtensions(device)) {
            LOG_WARN_RENDER("  rejected: VK_KHR_swapchain missing");
            continue;
        }
        const SwapchainSupportDetails support = QuerySwapchainSupport(device);
        if (support.formats.empty() || support.present_modes.empty()) {
            LOG_WARN_RENDER("  rejected: incomplete swapchain support");
            continue;
        }

        int score = 100;
        switch (properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score = 1000; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 500; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score = 250; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU: score = 50; break;
            default: score = 100; break;
        }

        if (score > best_score) {
            best_score = score;
            best_device = device;
        }
    }

    if (best_device == VK_NULL_HANDLE) {
        LOG_ERROR_RENDER("No Vulkan device satisfies graphics/present/swapchain/Vulkan 1.3");
        return false;
    }

    physical_device_ = best_device;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device_, &properties);
    device_name_ = properties.deviceName;
    LOG_INFO_RENDER("Selected GPU: {}", device_name_);
    return true;
}

bool VulkanContext::CreateLogicalDevice() {
    const QueueFamilyIndices indices = FindQueueFamilies(physical_device_);
    if (!indices.Complete()) {
        LOG_ERROR_RENDER("Cannot create device without graphics and present queues");
        return false;
    }

    graphics_queue_family_ = static_cast<uint32_t>(indices.graphics);
    present_queue_family_ = static_cast<uint32_t>(indices.present);

    std::array<uint32_t, 2> unique_families{graphics_queue_family_, present_queue_family_};
    uint32_t unique_count = 1;
    if (present_queue_family_ != graphics_queue_family_) {
        unique_count = 2;
    }

    float queue_priority = 1.0f;
    std::array<VkDeviceQueueCreateInfo, 2> queue_infos{};
    for (uint32_t i = 0; i < unique_count; ++i) {
        queue_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_infos[i].queueFamilyIndex = unique_families[i];
        queue_infos[i].queueCount = 1;
        queue_infos[i].pQueuePriorities = &queue_priority;
    }

    VkPhysicalDeviceFeatures features{};
    const char* extensions[1] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = unique_count;
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = extensions;
    create_info.pEnabledFeatures = &features;

    const VkResult result = vkCreateDevice(physical_device_, &create_info, nullptr, &device_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkCreateDevice failed: {}", static_cast<int>(result));
        return false;
    }

    vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_queue_family_, 0, &present_queue_);

    LOG_INFO_RENDER("Logical device: graphics_family={}, present_family={}, separate={}",
                    graphics_queue_family_, present_queue_family_,
                    graphics_queue_family_ != present_queue_family_);
    return true;
}

VkSurfaceFormatKHR VulkanContext::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    for (const VkSurfaceFormatKHR& format : formats) {
        if ((format.format == VK_FORMAT_B8G8R8A8_SRGB ||
             format.format == VK_FORMAT_R8G8B8A8_SRGB) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    for (const VkSurfaceFormatKHR& format : formats) {
        if (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return format;
    }
    return formats.front();
}

VkPresentModeKHR VulkanContext::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const {
    if (options_.vsync) {
        for (VkPresentModeKHR mode : modes) {
            if (mode == VK_PRESENT_MODE_FIFO_KHR) return mode;
        }
    } else {
        for (VkPresentModeKHR mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
        }
        for (VkPresentModeKHR mode : modes) {
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent{};
    const uint32_t width = static_cast<uint32_t>(std::max(window_->GetPixelWidth(), 1));
    const uint32_t height = static_cast<uint32_t>(std::max(window_->GetPixelHeight(), 1));
    extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

bool VulkanContext::CreateSwapchain() {
    const SwapchainSupportDetails support = QuerySwapchainSupport(physical_device_);
    if (support.formats.empty() || support.present_modes.empty()) {
        LOG_ERROR_RENDER("Swapchain support is incomplete");
        return false;
    }

    const VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(support.formats);
    const VkPresentModeKHR present_mode = ChoosePresentMode(support.present_modes);
    const VkExtent2D extent = ChooseExtent(support.capabilities);
    if (extent.width == 0 || extent.height == 0) {
        LOG_WARN_RENDER("Swapchain extent is zero; deferring creation");
        return false;
    }

    uint32_t image_count = support.capabilities.minImageCount + 1;
    if (image_count > 3) image_count = 3;
    if (image_count < support.capabilities.minImageCount) {
        image_count = support.capabilities.minImageCount;
    }
    if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount) {
        image_count = support.capabilities.maxImageCount;
    }

    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    const VkCompositeAlphaFlagsKHR supported_alpha = support.capabilities.supportedCompositeAlpha;
    const VkCompositeAlphaFlagsKHR opaque_alpha =
        static_cast<VkCompositeAlphaFlagsKHR>(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
    if ((supported_alpha & opaque_alpha) == 0) {
        if ((supported_alpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) != 0) {
            composite_alpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        } else if ((supported_alpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) != 0) {
            composite_alpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        } else {
            composite_alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        }
    }

    const uint32_t queue_families[2] = {graphics_queue_family_, present_queue_family_};
    const bool separate_families = graphics_queue_family_ != present_queue_family_;

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface_;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = separate_families
        ? VK_SHARING_MODE_CONCURRENT
        : VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = separate_families ? 2U : 0U;
    create_info.pQueueFamilyIndices = separate_families ? queue_families : nullptr;
    create_info.preTransform = support.capabilities.currentTransform;
    create_info.compositeAlpha = composite_alpha;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    const VkResult result = vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkCreateSwapchainKHR failed: {}", static_cast<int>(result));
        return false;
    }

    uint32_t actual_count = 0;
    if (vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, nullptr) != VK_SUCCESS ||
        actual_count == 0) {
        LOG_ERROR_RENDER("vkGetSwapchainImagesKHR (count) failed");
        return false;
    }
    swapchain_images_.resize(actual_count);
    if (vkGetSwapchainImagesKHR(device_, swapchain_, &actual_count, swapchain_images_.data()) != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkGetSwapchainImagesKHR failed");
        return false;
    }

    swapchain_views_.resize(actual_count, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < actual_count; ++i) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swapchain_images_[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = surface_format.format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &view_info, nullptr, &swapchain_views_[i]) != VK_SUCCESS) {
            LOG_ERROR_RENDER("vkCreateImageView failed for swapchain image {}", i);
            return false;
        }
    }

    swapchain_format_ = surface_format.format;
    swapchain_extent_ = extent;
    swapchain_recreate_requested_ = false;
    swapchain_suboptimal_ = false;
    ++swapchain_serial_;

    LOG_INFO_RENDER("Swapchain: {}x{}, format={}, present={}, images={}",
                    extent.width, extent.height, FormatName(surface_format.format),
                    PresentModeName(present_mode), actual_count);
    return true;
}

bool VulkanContext::CreateRenderPass() {
    if (render_pass_ != VK_NULL_HANDLE && render_pass_format_ == swapchain_format_) {
        return true;
    }
    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
    }

    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_format_;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_reference{};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &color_attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    if (vkCreateRenderPass(device_, &create_info, nullptr, &render_pass_) != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkCreateRenderPass failed");
        render_pass_ = VK_NULL_HANDLE;
        return false;
    }

    render_pass_format_ = swapchain_format_;
    return true;
}

bool VulkanContext::CreateFramebuffers() {
    for (VkFramebuffer framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    framebuffers_.clear();

    if (render_pass_ == VK_NULL_HANDLE) return false;

    framebuffers_.resize(swapchain_views_.size(), VK_NULL_HANDLE);
    for (std::size_t i = 0; i < swapchain_views_.size(); ++i) {
        VkFramebufferCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = render_pass_;
        create_info.attachmentCount = 1;
        create_info.pAttachments = &swapchain_views_[i];
        create_info.width = swapchain_extent_.width;
        create_info.height = swapchain_extent_.height;
        create_info.layers = 1;

        if (vkCreateFramebuffer(device_, &create_info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            LOG_ERROR_RENDER("vkCreateFramebuffer failed for image {}", i);
            return false;
        }
    }
    return true;
}

bool VulkanContext::CreateCommandResources() {
    for (uint32_t i = 0; i < kFramesInFlight; ++i) {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = graphics_queue_family_;
        if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pools_[i]) != VK_SUCCESS) {
            LOG_ERROR_RENDER("vkCreateCommandPool failed for frame {}", i);
            return false;
        }

        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = command_pools_[i];
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device_, &alloc_info, &command_buffers_[i]) != VK_SUCCESS) {
            LOG_ERROR_RENDER("vkAllocateCommandBuffers failed for frame {}", i);
            return false;
        }
    }
    return true;
}

bool VulkanContext::CreateSyncResources() {
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kFramesInFlight; ++i) {
        if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fence_info, nullptr, &in_flight_fences_[i]) != VK_SUCCESS) {
            LOG_ERROR_RENDER("Vulkan sync object creation failed for frame {}", i);
            return false;
        }
    }

    // A present semaphore must be tied to the swapchain image, not to the
    // frame slot. The presentation engine may still be using the image's
    // semaphore after the rendering fence has signaled.
    render_finished_.resize(swapchain_images_.size(), VK_NULL_HANDLE);
    for (std::size_t i = 0; i < render_finished_.size(); ++i) {
        if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_[i]) != VK_SUCCESS) {
            LOG_ERROR_RENDER("Vulkan present semaphore creation failed for image {}", i);
            return false;
        }
    }
    return true;
}

void VulkanContext::DestroySwapchainResources(bool destroy_render_pass) {
    if (device_ == VK_NULL_HANDLE) {
        swapchain_images_.clear();
        swapchain_views_.clear();
        framebuffers_.clear();
        swapchain_ = VK_NULL_HANDLE;
        if (destroy_render_pass) {
            render_pass_ = VK_NULL_HANDLE;
            render_pass_format_ = VK_FORMAT_UNDEFINED;
        }
        return;
    }

    for (VkFramebuffer framebuffer : framebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
    }
    framebuffers_.clear();

    for (VkImageView view : swapchain_views_) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, view, nullptr);
        }
    }
    swapchain_views_.clear();
    swapchain_images_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    if (destroy_render_pass && render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, render_pass_, nullptr);
        render_pass_ = VK_NULL_HANDLE;
        render_pass_format_ = VK_FORMAT_UNDEFINED;
    }
}

void VulkanContext::DestroySyncResources() {
    if (device_ == VK_NULL_HANDLE) return;
    for (uint32_t i = 0; i < kFramesInFlight; ++i) {
        if (image_available_[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, image_available_[i], nullptr);
            image_available_[i] = VK_NULL_HANDLE;
        }
        if (in_flight_fences_[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device_, in_flight_fences_[i], nullptr);
            in_flight_fences_[i] = VK_NULL_HANDLE;
        }
    }
    for (VkSemaphore semaphore : render_finished_) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device_, semaphore, nullptr);
        }
    }
    render_finished_.clear();
}

void VulkanContext::DestroyCommandResources() {
    if (device_ == VK_NULL_HANDLE) return;
    for (uint32_t i = 0; i < kFramesInFlight; ++i) {
        if (command_pools_[i] != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, command_pools_[i], nullptr);
            command_pools_[i] = VK_NULL_HANDLE;
            command_buffers_[i] = VK_NULL_HANDLE;
        }
    }
}

bool VulkanContext::RecreateSwapchain() {
    if (device_ == VK_NULL_HANDLE || window_ == nullptr) return false;
    if (window_->IsMinimized() || window_->GetPixelWidth() <= 0 || window_->GetPixelHeight() <= 0) {
        return false;
    }

    (void)vkDeviceWaitIdle(device_);

    DestroySwapchainResources(false);

    if (!CreateSwapchain()) {
        return false;
    }
    if (render_pass_ == VK_NULL_HANDLE || render_pass_format_ != swapchain_format_) {
        if (!CreateRenderPass()) {
            return false;
        }
    }
    if (!CreateFramebuffers()) {
        return false;
    }

    DestroySyncResources();
    if (!CreateSyncResources()) {
        LOG_ERROR_RENDER("Sync resources could not be recreated for the new swapchain");
        return false;
    }

    swapchain_recreate_requested_ = false;
    swapchain_suboptimal_ = false;
    image_index_ = 0;
    LOG_INFO_RENDER("Swapchain recreated: {}x{} (serial {})",
                    swapchain_extent_.width, swapchain_extent_.height, swapchain_serial_);
    return true;
}

FrameStatus VulkanContext::BeginFrame() {
    if (!initialized_ || device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
        return FrameStatus::Error;
    }
    if (window_ == nullptr || window_->IsMinimized() ||
        window_->GetPixelWidth() <= 0 || window_->GetPixelHeight() <= 0) {
        return FrameStatus::Skipped;
    }

    // The window state is updated from the compositor's resize/pixel-size
    // events. Do not compare the current extent to a guessed pixel size: on a
    // scaled Wayland output the compositor's currentExtent is authoritative.
    if (swapchain_recreate_requested_ || window_->HasPendingResize()) {
        if (!RecreateSwapchain()) {
            if (window_->IsMinimized() || window_->GetPixelWidth() <= 0 ||
                window_->GetPixelHeight() <= 0) {
                return FrameStatus::Skipped;
            }
            return FrameStatus::Error;
        }
    }

    const VkFence fence = in_flight_fences_[current_frame_];
    if (vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkWaitForFences failed");
        return FrameStatus::Error;
    }

    const VkResult acquire_result = vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX, image_available_[current_frame_], VK_NULL_HANDLE, &image_index_);
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!RecreateSwapchain()) {
            return FrameStatus::Skipped;
        }
        return FrameStatus::Skipped;
    }
    if (acquire_result == VK_SUBOPTIMAL_KHR) {
        swapchain_suboptimal_ = true;
    } else if (acquire_result != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkAcquireNextImageKHR failed: {}", static_cast<int>(acquire_result));
        return FrameStatus::Error;
    }

    if (image_index_ >= framebuffers_.size()) {
        LOG_ERROR_RENDER("Acquired image index {} is out of range", image_index_);
        return FrameStatus::Error;
    }

    if (vkResetFences(device_, 1, &fence) != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkResetFences failed");
        return FrameStatus::Error;
    }
    if (vkResetCommandPool(device_, command_pools_[current_frame_], 0) != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkResetCommandPool failed");
        return FrameStatus::Error;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(command_buffers_[current_frame_], &begin_info) != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkBeginCommandBuffer failed");
        return FrameStatus::Error;
    }

    VkClearValue clear_value{};
    clear_value.color.float32[0] = 0.035f;
    clear_value.color.float32[1] = 0.045f;
    clear_value.color.float32[2] = 0.075f;
    clear_value.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass_;
    render_pass_info.framebuffer = framebuffers_[image_index_];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = swapchain_extent_;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(command_buffers_[current_frame_], &render_pass_info,
                         VK_SUBPASS_CONTENTS_INLINE);
    frame_begun_ = true;
    return FrameStatus::Ready;
}

bool VulkanContext::EndFrame() {
    if (!frame_begun_) {
        LOG_ERROR_RENDER("EndFrame called without a begun frame");
        return false;
    }

    vkCmdEndRenderPass(command_buffers_[current_frame_]);
    if (vkEndCommandBuffer(command_buffers_[current_frame_]) != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkEndCommandBuffer failed");
        frame_begun_ = false;
        return false;
    }

    if (image_index_ >= render_finished_.size()) {
        LOG_ERROR_RENDER("No present semaphore for acquired image {}", image_index_);
        frame_begun_ = false;
        return false;
    }

    VkSemaphore wait_semaphores[1] = {image_available_[current_frame_]};
    VkPipelineStageFlags wait_stages[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[1] = {render_finished_[image_index_]};
    VkCommandBuffer command_buffers[1] = {command_buffers_[current_frame_]};

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = command_buffers;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    const VkResult submit_result = vkQueueSubmit(
        graphics_queue_, 1, &submit_info, in_flight_fences_[current_frame_]);
    if (submit_result != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkQueueSubmit failed: {}", static_cast<int>(submit_result));
        frame_begun_ = false;
        return false;
    }

    VkSwapchainKHR swapchains[1] = {swapchain_};
    uint32_t image_indices[1] = {image_index_};
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = image_indices;

    const VkResult present_result = vkQueuePresentKHR(present_queue_, &present_info);
    bool success = true;
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchain_recreate_requested_ = true;
    } else if (present_result == VK_SUBOPTIMAL_KHR) {
        swapchain_recreate_requested_ = true;
        swapchain_suboptimal_ = true;
    } else if (present_result != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkQueuePresentKHR failed: {}", static_cast<int>(present_result));
        success = false;
    }

    if (swapchain_suboptimal_) {
        swapchain_recreate_requested_ = true;
    }

    frame_begun_ = false;
    current_frame_ = (current_frame_ + 1) % kFramesInFlight;
    return success;
}

void VulkanContext::WaitIdle() {
    if (device_ != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(device_);
    }
}

VkCommandBuffer VulkanContext::CurrentCommandBuffer() const {
    return command_buffers_[current_frame_];
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* user_data) {
    (void)types;
    (void)user_data;

    if (data == nullptr || data->pMessage == nullptr) return VK_FALSE;

    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
        LOG_ERROR_RENDER("Vulkan validation: {}", data->pMessage);
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
        LOG_WARN_RENDER("Vulkan validation: {}", data->pMessage);
    }
    return VK_FALSE;
}

} // namespace soulknight::engine::rendering
