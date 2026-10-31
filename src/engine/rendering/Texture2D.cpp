// Soul Knight Linux — GPU texture implementation
#include "Texture2D.hpp"

#include "engine/core/Log.hpp"

#include <cstring>
#include <limits>

namespace soulknight::engine::rendering {
namespace {

uint32_t FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter,
                        VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        const bool allowed = (type_filter & (1U << i)) != 0;
        const bool has_properties =
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties;
        if (allowed && has_properties) return i;
    }
    return std::numeric_limits<uint32_t>::max();
}

} // namespace

Texture2D::~Texture2D() {
    // The owner (SpriteRenderer) calls Destroy while the logical device is alive.
}

bool Texture2D::CreateFromTextureData(VkDevice device, VkPhysicalDevice physical_device,
                                      VkQueue queue, uint32_t queue_family,
                                      const soulknight::engine::assets::TextureData& texture,
                                      soulknight::engine::assets::TextureFilter filter,
                                      soulknight::engine::assets::TextureWrap wrap) {
    if (!texture.IsValid()) {
        LOG_ERROR_RENDER("Texture: canonical TextureData is invalid");
        return false;
    }
    return CreateFromRGBA8(device, physical_device, queue, queue_family,
                           texture.width, texture.height, texture.rgba8.data(),
                           filter, wrap);
}

bool Texture2D::CreateFromRGBA8(VkDevice device, VkPhysicalDevice physical_device,
                                VkQueue queue, uint32_t queue_family,
                                uint32_t width, uint32_t height,
                                const uint8_t* pixels,
                                soulknight::engine::assets::TextureFilter filter,
                                soulknight::engine::assets::TextureWrap wrap) {
    if (device == VK_NULL_HANDLE || physical_device == VK_NULL_HANDLE ||
        queue == VK_NULL_HANDLE || pixels == nullptr || width == 0 || height == 0) {
        return false;
    }
    Destroy(device);

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &image_info, nullptr, &image_) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: vkCreateImage failed");
        Destroy(device);
        return false;
    }

    VkMemoryRequirements image_requirements{};
    vkGetImageMemoryRequirements(device, image_, &image_requirements);
    const uint32_t image_memory_type = FindMemoryType(
        physical_device, image_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (image_memory_type == std::numeric_limits<uint32_t>::max()) {
        LOG_ERROR_RENDER("Texture: no device-local memory type");
        Destroy(device);
        return false;
    }

    VkMemoryAllocateInfo image_allocate{};
    image_allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    image_allocate.allocationSize = image_requirements.size;
    image_allocate.memoryTypeIndex = image_memory_type;
    if (vkAllocateMemory(device, &image_allocate, nullptr, &memory_) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: image memory allocation failed");
        Destroy(device);
        return false;
    }
    if (vkBindImageMemory(device, image_, memory_, 0) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: vkBindImageMemory failed");
        Destroy(device);
        return false;
    }

    const VkDeviceSize pixel_bytes =
        static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4U;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    void* staging_mapped = nullptr;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = pixel_bytes;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer_info, nullptr, &staging_buffer) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: staging buffer creation failed");
        Destroy(device);
        return false;
    }

    VkMemoryRequirements buffer_requirements{};
    vkGetBufferMemoryRequirements(device, staging_buffer, &buffer_requirements);
    const uint32_t staging_memory_type = FindMemoryType(
        physical_device, buffer_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (staging_memory_type == std::numeric_limits<uint32_t>::max()) {
        LOG_ERROR_RENDER("Texture: no host-visible staging memory type");
        vkDestroyBuffer(device, staging_buffer, nullptr);
        Destroy(device);
        return false;
    }

    VkMemoryAllocateInfo staging_allocate{};
    staging_allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    staging_allocate.allocationSize = buffer_requirements.size;
    staging_allocate.memoryTypeIndex = staging_memory_type;
    if (vkAllocateMemory(device, &staging_allocate, nullptr, &staging_memory) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: staging memory allocation failed");
        vkDestroyBuffer(device, staging_buffer, nullptr);
        Destroy(device);
        return false;
    }
    if (vkBindBufferMemory(device, staging_buffer, staging_memory, 0) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: vkBindBufferMemory failed");
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
        Destroy(device);
        return false;
    }
    if (vkMapMemory(device, staging_memory, 0, VK_WHOLE_SIZE, 0, &staging_mapped) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: vkMapMemory failed");
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
        Destroy(device);
        return false;
    }
    std::memcpy(staging_mapped, pixels, static_cast<std::size_t>(pixel_bytes));

    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    if ((memory_properties.memoryTypes[staging_memory_type].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
        VkMappedMemoryRange range{};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = staging_memory;
        range.offset = 0;
        range.size = VK_WHOLE_SIZE;
        (void)vkFlushMappedMemoryRanges(device, 1, &range);
    }
    vkUnmapMemory(device, staging_memory);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = queue_family;

    VkCommandPool command_pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: command pool creation failed");
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
        Destroy(device);
        return false;
    }

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo command_allocate{};
    command_allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_allocate.commandPool = command_pool;
    command_allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_allocate.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &command_allocate, &command_buffer) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: command buffer allocation failed");
        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
        Destroy(device);
        return false;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    (void)vkBeginCommandBuffer(command_buffer, &begin_info);

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier to_transfer{};
    to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = image_;
    to_transfer.subresourceRange = range;
    to_transfer.srcAccessMask = 0;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_transfer);

    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset = {0, 0, 0};
    copy.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(command_buffer, staging_buffer, image_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier to_shader_read{};
    to_shader_read.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_shader_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader_read.image = image_;
    to_shader_read.subresourceRange = range;
    to_shader_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_shader_read);

    (void)vkEndCommandBuffer(command_buffer);

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: upload fence creation failed");
        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
        Destroy(device);
        return false;
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    const VkResult submit_result = vkQueueSubmit(queue, 1, &submit_info, fence);
    if (submit_result != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: upload submit failed: {}", static_cast<int>(submit_result));
        vkDestroyFence(device, fence, nullptr);
        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_memory, nullptr);
        Destroy(device);
        return false;
    }

    const VkResult wait_result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, command_pool, nullptr);
    vkDestroyBuffer(device, staging_buffer, nullptr);
    vkFreeMemory(device, staging_memory, nullptr);

    if (wait_result != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: upload fence wait failed");
        Destroy(device);
        return false;
    }

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image_;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange = range;
    if (vkCreateImageView(device, &view_info, nullptr, &view_) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: vkCreateImageView failed");
        Destroy(device);
        return false;
    }

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    const bool nearest = filter == soulknight::engine::assets::TextureFilter::Point;
    const VkFilter vk_filter = nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    sampler_info.magFilter = vk_filter;
    sampler_info.minFilter = vk_filter;
    sampler_info.mipmapMode =
        filter == soulknight::engine::assets::TextureFilter::Trilinear
            ? VK_SAMPLER_MIPMAP_MODE_LINEAR
            : (nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR);

    VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (wrap == soulknight::engine::assets::TextureWrap::Repeat) {
        address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    } else if (wrap == soulknight::engine::assets::TextureWrap::Mirror) {
        address_mode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    if (vkCreateSampler(device, &sampler_info, nullptr, &sampler_) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Texture: vkCreateSampler failed");
        Destroy(device);
        return false;
    }

    width_ = width;
    height_ = height;
    return true;
}

void Texture2D::Destroy(VkDevice device) {
    if (device == VK_NULL_HANDLE) return;
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
    if (view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, view_, nullptr);
        view_ = VK_NULL_HANDLE;
    }
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device, image_, nullptr);
        image_ = VK_NULL_HANDLE;
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
    width_ = 0;
    height_ = 0;
}

} // namespace soulknight::engine::rendering
