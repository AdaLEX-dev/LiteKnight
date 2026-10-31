// Soul Knight Linux — Batched textured sprite renderer implementation
#include "SpriteRenderer.hpp"

#include "SpriteBatch.hpp"
#include "VulkanContext.hpp"
#include "engine/core/Log.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#ifndef SOUL_KNIGHT_SHADER_DIR
#define SOUL_KNIGHT_SHADER_DIR "shaders"
#endif

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

static_assert(2 == VulkanContext::kFramesInFlight,
              "SpriteRenderer frame-buffer count must match VulkanContext");

SpriteRenderer::~SpriteRenderer() {
    // The owner calls Shutdown while the Vulkan device is alive.
}

bool SpriteRenderer::Initialize(VulkanContext& context) {
    if (initialized_) return true;

    vertices_.reserve(kMaxQuads * 4);

    if (!CreateDescriptorResources(context)) {
        Shutdown(context);
        return false;
    }
    if (!CreatePipeline(context)) {
        Shutdown(context);
        return false;
    }
    if (!CreateFrameBuffers(context)) {
        Shutdown(context);
        return false;
    }
    if (!CreateIndexBuffer(context)) {
        Shutdown(context);
        return false;
    }
    if (!CreateTestTexture(context)) {
        Shutdown(context);
        return false;
    }
    if (!UpdateDescriptor(context)) {
        Shutdown(context);
        return false;
    }

    initialized_ = true;
    LOG_INFO_RENDER("Sprite renderer ready: max_quads={}, texture={}x{}",
                    kMaxQuads, test_texture_.Width(), test_texture_.Height());
    return true;
}

void SpriteRenderer::Shutdown(VulkanContext& context) {
    VkDevice device = context.Device();
    if (device != VK_NULL_HANDLE) {
        // Release the descriptor pool first: its descriptor set still refers to
        // the test sampler/image until this point.
        if (descriptor_pool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptor_pool_, nullptr);
            descriptor_pool_ = VK_NULL_HANDLE;
        }
        test_texture_.Destroy(device);

        for (FrameBuffer& frame : frame_buffers_) {
            if (frame.buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, frame.buffer, nullptr);
                frame.buffer = VK_NULL_HANDLE;
            }
            if (frame.memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, frame.memory, nullptr);
                frame.memory = VK_NULL_HANDLE;
            }
            frame.mapped = nullptr;
        }

        if (index_buffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, index_buffer_, nullptr);
            index_buffer_ = VK_NULL_HANDLE;
        }
        if (index_memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device, index_memory_, nullptr);
            index_memory_ = VK_NULL_HANDLE;
        }

        if (pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline_, nullptr);
            pipeline_ = VK_NULL_HANDLE;
        }
        if (pipeline_layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
            pipeline_layout_ = VK_NULL_HANDLE;
        }
        if (descriptor_set_layout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptor_set_layout_, nullptr);
            descriptor_set_layout_ = VK_NULL_HANDLE;
        }
    }

    descriptor_set_ = VK_NULL_HANDLE;
    pipeline_format_ = VK_FORMAT_UNDEFINED;
    pipeline_serial_ = 0;
    vertices_.clear();
    initialized_ = false;
}

bool SpriteRenderer::CreateDescriptorResources(VulkanContext& context) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(context.Device(), &layout_info, nullptr,
                                    &descriptor_set_layout_) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Sprite pipeline: descriptor set layout creation failed");
        return false;
    }

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 1;
    if (vkCreateDescriptorPool(context.Device(), &pool_info, nullptr,
                               &descriptor_pool_) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Sprite pipeline: descriptor pool creation failed");
        return false;
    }

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = descriptor_pool_;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_set_layout_;
    if (vkAllocateDescriptorSets(context.Device(), &allocate_info,
                                 &descriptor_set_) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Sprite pipeline: descriptor set allocation failed");
        return false;
    }
    return true;
}

std::vector<char> SpriteRenderer::LoadShaderFile(const char* filename) {
    const std::string path = std::string(SOUL_KNIGHT_SHADER_DIR) + "/" + filename;
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_RENDER("Shader file missing: {}", path);
        return {};
    }

    const std::streamsize size = file.tellg();
    if (size <= 0) {
        LOG_ERROR_RENDER("Shader file is empty: {}", path);
        return {};
    }

    std::vector<char> data(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(data.data(), size)) {
        LOG_ERROR_RENDER("Shader file read failed: {}", path);
        return {};
    }
    return data;
}

VkShaderModule SpriteRenderer::CreateShaderModule(VkDevice device,
                                                  const std::vector<char>& code) {
    if (code.empty() || code.size() % 4 != 0) {
        LOG_ERROR_RENDER("Invalid SPIR-V byte count");
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &create_info, nullptr, &module) != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkCreateShaderModule failed");
        return VK_NULL_HANDLE;
    }
    return module;
}

bool SpriteRenderer::CreatePipeline(VulkanContext& context) {
    const std::vector<char> vertex_code = LoadShaderFile("sprite.vert.spv");
    const std::vector<char> fragment_code = LoadShaderFile("sprite.frag.spv");
    if (vertex_code.empty() || fragment_code.empty()) return false;

    VkShaderModule vertex_module = CreateShaderModule(context.Device(), vertex_code);
    VkShaderModule fragment_module = CreateShaderModule(context.Device(), fragment_code);
    if (vertex_module == VK_NULL_HANDLE || fragment_module == VK_NULL_HANDLE) {
        if (vertex_module != VK_NULL_HANDLE) vkDestroyShaderModule(context.Device(), vertex_module, nullptr);
        if (fragment_module != VK_NULL_HANDLE) vkDestroyShaderModule(context.Device(), fragment_module, nullptr);
        return false;
    }

    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.Device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }

    if (pipeline_layout_ == VK_NULL_HANDLE) {
        VkPushConstantRange push_range{};
        push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_range.offset = 0;
        push_range.size = sizeof(Mat4);

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &descriptor_set_layout_;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_range;
        if (vkCreatePipelineLayout(context.Device(), &layout_info, nullptr,
                                   &pipeline_layout_) != VK_SUCCESS) {
            LOG_ERROR_RENDER("Sprite pipeline: pipeline layout creation failed");
            vkDestroyShaderModule(context.Device(), vertex_module, nullptr);
            vkDestroyShaderModule(context.Device(), fragment_module, nullptr);
            return false;
        }
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex_module;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment_module;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(SpriteVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[3]{};
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = static_cast<uint32_t>(offsetof(SpriteVertex, x));
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = static_cast<uint32_t>(offsetof(SpriteVertex, u));
    attributes[2].location = 2;
    attributes[2].binding = 0;
    attributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributes[2].offset = static_cast<uint32_t>(offsetof(SpriteVertex, r));

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.blendEnable = VK_TRUE;
    blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &blend_attachment;

    VkDynamicState dynamic_states[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_layout_;
    pipeline_info.renderPass = context.RenderPass();
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineIndex = -1;

    const VkResult result = vkCreateGraphicsPipelines(
        context.Device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_);
    vkDestroyShaderModule(context.Device(), vertex_module, nullptr);
    vkDestroyShaderModule(context.Device(), fragment_module, nullptr);

    if (result != VK_SUCCESS) {
        LOG_ERROR_RENDER("vkCreateGraphicsPipelines failed: {}", static_cast<int>(result));
        pipeline_ = VK_NULL_HANDLE;
        return false;
    }

    pipeline_format_ = context.SwapchainFormat();
    pipeline_serial_ = context.SwapchainSerial();
    LOG_INFO_RENDER("Sprite pipeline created for render pass {}, format {}",
                    static_cast<const void*>(context.RenderPass()),
                    static_cast<int>(pipeline_format_));
    return true;
}

bool SpriteRenderer::CreateHostVisibleBuffer(VulkanContext& context, VkDeviceSize size,
                                             VkBufferUsageFlags usage, VkBuffer& buffer,
                                             VkDeviceMemory& memory, void** mapped) const {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(context.Device(), &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Sprite renderer: buffer creation failed ({} bytes)", size);
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(context.Device(), buffer, &requirements);
    const uint32_t memory_type = FindMemoryType(
        context.PhysicalDevice(), requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_type == std::numeric_limits<uint32_t>::max()) {
        LOG_ERROR_RENDER("Sprite renderer: no host-visible coherent memory type");
        vkDestroyBuffer(context.Device(), buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = memory_type;
    if (vkAllocateMemory(context.Device(), &allocate_info, nullptr, &memory) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Sprite renderer: buffer memory allocation failed");
        vkDestroyBuffer(context.Device(), buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }
    if (vkBindBufferMemory(context.Device(), buffer, memory, 0) != VK_SUCCESS ||
        vkMapMemory(context.Device(), memory, 0, VK_WHOLE_SIZE, 0, mapped) != VK_SUCCESS) {
        LOG_ERROR_RENDER("Sprite renderer: buffer bind/map failed");
        vkFreeMemory(context.Device(), memory, nullptr);
        vkDestroyBuffer(context.Device(), buffer, nullptr);
        memory = VK_NULL_HANDLE;
        buffer = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

bool SpriteRenderer::CreateFrameBuffers(VulkanContext& context) {
    const VkDeviceSize vertex_buffer_size =
        static_cast<VkDeviceSize>(kMaxQuads) * 4U * sizeof(SpriteVertex);

    for (std::size_t i = 0; i < frame_buffers_.size(); ++i) {
        if (!CreateHostVisibleBuffer(context, vertex_buffer_size,
                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     frame_buffers_[i].buffer,
                                     frame_buffers_[i].memory,
                                     &frame_buffers_[i].mapped)) {
            return false;
        }
    }
    return true;
}

bool SpriteRenderer::CreateIndexBuffer(VulkanContext& context) {
    std::vector<uint32_t> indices;
    indices.reserve(kMaxQuads * 6);
    for (std::size_t i = 0; i < kMaxQuads; ++i) {
        const uint32_t base = static_cast<uint32_t>(i * 4);
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base + 0);
    }

    const VkDeviceSize index_buffer_size =
        static_cast<VkDeviceSize>(indices.size() * sizeof(uint32_t));
    void* mapped = nullptr;
    if (!CreateHostVisibleBuffer(context, index_buffer_size,
                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 index_buffer_, index_memory_, &mapped)) {
        return false;
    }
    std::memcpy(mapped, indices.data(), static_cast<std::size_t>(index_buffer_size));
    return true;
}

bool SpriteRenderer::CreateTestTexture(VulkanContext& context) {
    constexpr uint32_t width = 64;
    constexpr uint32_t height = 64;
    std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * height * 4U);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const bool checker = (((x / 8U) + (y / 8U)) % 2U) == 0U;
            const bool border = x < 4U || y < 4U || x >= width - 4U || y >= height - 4U;
            const std::size_t index =
                (static_cast<std::size_t>(y) * width + x) * 4U;
            pixels[index + 0] = static_cast<uint8_t>(checker ? 232U : 48U);
            pixels[index + 1] = static_cast<uint8_t>(checker ? 168U : 96U);
            pixels[index + 2] = static_cast<uint8_t>(checker ? 64U : 208U);
            pixels[index + 3] = static_cast<uint8_t>(border ? 0U : 255U);
        }
    }

    return test_texture_.CreateFromRGBA8(
        context.Device(), context.PhysicalDevice(),
        context.GraphicsQueue(), context.GraphicsQueueFamily(),
        width, height, pixels.data());
}

bool SpriteRenderer::UpdateDescriptor(VulkanContext& context) {
    if (!test_texture_.IsValid()) return false;

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = test_texture_.View();
    image_info.sampler = test_texture_.Sampler();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set_;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(context.Device(), 1, &write, 0, nullptr);
    return true;
}

bool SpriteRenderer::Render(VulkanContext& context, SpriteBatch& batch) {
    if (!initialized_) return false;

    if (pipeline_ == VK_NULL_HANDLE ||
        pipeline_serial_ != context.SwapchainSerial() ||
        pipeline_format_ != context.SwapchainFormat()) {
        // Swapchain recreation waits for device idle before this point, so the
        // old pipeline cannot still be in use by an in-flight submission.
        if (!CreatePipeline(context)) return false;
    }

    batch.Sort();
    const std::vector<SpriteDraw>& draws = batch.GetDraws();
    if (draws.empty()) return true;
    if (draws.size() > kMaxQuads) {
        LOG_ERROR_RENDER("Sprite batch exceeds {} quads", kMaxQuads);
        return false;
    }

    vertices_.clear();
    for (const SpriteDraw& draw : draws) {
        const float r = static_cast<float>(draw.color.r) / 255.0f;
        const float g = static_cast<float>(draw.color.g) / 255.0f;
        const float b = static_cast<float>(draw.color.b) / 255.0f;
        const float a = static_cast<float>(draw.color.a) / 255.0f;
        const float x0 = draw.position.x;
        const float y0 = draw.position.y;
        const float x1 = draw.position.x + draw.size.x;
        const float y1 = draw.position.y + draw.size.y;
        const float u0 = draw.uv.x;
        const float v0 = draw.uv.y;
        const float u1 = draw.uv.x + draw.uv.w;
        const float v1 = draw.uv.y + draw.uv.h;

        vertices_.push_back(SpriteVertex{x0, y0, u0, v0, r, g, b, a});
        vertices_.push_back(SpriteVertex{x1, y0, u1, v0, r, g, b, a});
        vertices_.push_back(SpriteVertex{x1, y1, u1, v1, r, g, b, a});
        vertices_.push_back(SpriteVertex{x0, y1, u0, v1, r, g, b, a});
    }

    const uint32_t frame_index = context.CurrentFrameIndex();
    if (static_cast<std::size_t>(frame_index) >= frame_buffers_.size()) return false;
    FrameBuffer& frame = frame_buffers_[frame_index];
    if (frame.mapped == nullptr || frame.buffer == VK_NULL_HANDLE) return false;

    const std::size_t vertex_bytes = vertices_.size() * sizeof(SpriteVertex);
    std::memcpy(frame.mapped, vertices_.data(), vertex_bytes);

    VkCommandBuffer command_buffer = context.CurrentCommandBuffer();
    if (command_buffer == VK_NULL_HANDLE) return false;

    const VkExtent2D extent = context.SwapchainExtent();
    if (extent.width == 0 || extent.height == 0) return false;

    constexpr float logical_width = 1920.0f;
    constexpr float logical_height = 1080.0f;
    const float target_aspect = logical_width / logical_height;
    const float window_aspect = static_cast<float>(extent.width) /
                                static_cast<float>(extent.height);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    if (window_aspect > target_aspect) {
        viewport.width = static_cast<float>(extent.height) * target_aspect;
        viewport.x = (static_cast<float>(extent.width) - viewport.width) * 0.5f;
    } else if (window_aspect < target_aspect) {
        viewport.height = static_cast<float>(extent.width) / target_aspect;
        viewport.y = (static_cast<float>(extent.height) - viewport.height) * 0.5f;
    }

    VkRect2D scissor{};
    scissor.offset.x = static_cast<int32_t>(viewport.x);
    scissor.offset.y = static_cast<int32_t>(viewport.y);
    scissor.extent.width = static_cast<uint32_t>(viewport.width);
    scissor.extent.height = static_cast<uint32_t>(viewport.height);

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkDeviceSize vertex_offset = 0;
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &frame.buffer, &vertex_offset);
    vkCmdBindIndexBuffer(command_buffer, index_buffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

    const Mat4 projection = Mat4::Orthographic(
        0.0f, logical_width, 0.0f, logical_height, -1.0f, 1.0f);
    vkCmdPushConstants(command_buffer, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(Mat4), projection.m);

    const uint32_t index_count = static_cast<uint32_t>(draws.size() * 6U);
    vkCmdDrawIndexed(command_buffer, index_count, 1, 0, 0, 0);
    return true;
}

} // namespace soulknight::engine::rendering
