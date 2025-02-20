#include "rasteriser.h"
#include "dispatch.h"

#include <cassert>
#include <chrono>

#include <glm/gtc/matrix_transform.hpp>
#include <imgui_impl_vulkan.h>

struct UniformBufferObject
{
    alignas(16) glm::mat4 mvp;
    alignas(16) glm::mat4 normal;
    alignas(16) glm::vec3 view_pos;
    alignas(16) glm::vec3 light_dir_view_space_norm;
};

Rasteriser::Rasteriser(const Shader& vs, const Shader& ps, Dispatcher& dispatcher)
    : dispatcher(&dispatcher)
    , render_pass(VK_NULL_HANDLE)
    , pipeline_layout(VK_NULL_HANDLE)
    , pipeline(VK_NULL_HANDLE)
    , scene(nullptr)
{
    create_render_pass();
    create_descriptor_set_layout();
    create_pipeline(vs, ps);
    create_descriptor_pool();
    create_command_pool();
    create_depth_image();
    create_framebuffers();

    create_buffer(uniform_buffer, uniform_buffer_memory, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkMapMemory(dispatcher.device, uniform_buffer_memory, 0, sizeof(UniformBufferObject), 0, &uniform_buffer_map);

    create_descriptor_set();
    create_command_buffer();
    create_sync_objects();
}

Rasteriser::~Rasteriser()
{
    vkDestroyImage(dispatcher->device, depth_image, nullptr);
    vkFreeMemory(dispatcher->device, depth_image_memory, nullptr);
    vkDestroyBuffer(dispatcher->device, vertex_buffer, nullptr);
    vkFreeMemory(dispatcher->device, vertex_buffer_memory, nullptr);
    vkDestroyBuffer(dispatcher->device, index_buffer, nullptr);
    vkFreeMemory(dispatcher->device, index_buffer_memory, nullptr);
    vkDestroyBuffer(dispatcher->device, uniform_buffer, nullptr);
    vkFreeMemory(dispatcher->device, uniform_buffer_memory, nullptr);

    vkDestroySemaphore(dispatcher->device, image_semaphore, nullptr);
    vkDestroySemaphore(dispatcher->device, render_semaphore, nullptr);
    vkDestroyFence(dispatcher->device, render_fence, nullptr);
    vkDestroyCommandPool(dispatcher->device, command_pool, nullptr);
    vkDestroyDescriptorPool(dispatcher->device, descriptor_pool, nullptr);
    for (auto framebuffer : framebuffers)
        vkDestroyFramebuffer(dispatcher->device, framebuffer, nullptr);
    vkDestroyPipeline(dispatcher->device, pipeline, nullptr);
    vkDestroyPipelineLayout(dispatcher->device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(dispatcher->device, descriptor_set_layout, nullptr);
    vkDestroyRenderPass(dispatcher->device, render_pass, nullptr);
}

void Rasteriser::create_render_pass()
{
    VkAttachmentDescription colour_attachment{};
    colour_attachment.format = dispatcher->swap_chain.image_format;
    colour_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colour_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colour_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colour_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colour_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colour_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colour_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colour_attachment_ref{};
    colour_attachment_ref.attachment = 0;
    colour_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_format();
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colour_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = { colour_attachment, depth_attachment };
    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = static_cast<uint32_t>(SIZE_OF_ARRAY(attachments));
    render_pass_create_info.pAttachments = attachments;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &dependency;

    if (vkCreateRenderPass(dispatcher->device, &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass.");
}

void Rasteriser::create_descriptor_set_layout()
{
    VkDescriptorSetLayoutBinding ubo_layout_binding{};
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ubo_layout_binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layout_create_info{};
    layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_create_info.bindingCount = 1;
    layout_create_info.pBindings = &ubo_layout_binding;

    if (vkCreateDescriptorSetLayout(dispatcher->device, &layout_create_info, nullptr, &descriptor_set_layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout.");
}

void Rasteriser::create_pipeline(const Shader& vs, const Shader& ps)
{
    VkPipelineShaderStageCreateInfo shader_stage_create_infos[2] = { {}, {} };

    shader_stage_create_infos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stage_create_infos[0].module = vs.shader_module;
    shader_stage_create_infos[0].pName = "vs_main";

    shader_stage_create_infos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_create_infos[1].module = ps.shader_module;
    shader_stage_create_infos[1].pName = "ps_main";

    const std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state_create_info.pDynamicStates = dynamic_states.data();

    auto binding_description = Vertex::binding_description();
    auto attribute_descriptions = Vertex::attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_create_info{};
    vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_create_info.vertexBindingDescriptionCount = 1;
    vertex_input_create_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_create_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_create_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
    input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state_create_info{};
    viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasteriser_create_info{};
    rasteriser_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasteriser_create_info.depthClampEnable = VK_FALSE;
    rasteriser_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasteriser_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasteriser_create_info.lineWidth = 1.0f;
    rasteriser_create_info.cullMode = VK_CULL_MODE_NONE;
    rasteriser_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasteriser_create_info.depthBiasEnable = VK_FALSE;
    rasteriser_create_info.depthBiasConstantFactor = 0.0f;
    rasteriser_create_info.depthBiasClamp = 0.0f;
    rasteriser_create_info.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colour_blend_attachment{};
    colour_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colour_blend_attachment.blendEnable = VK_FALSE;
    colour_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colour_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colour_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    colour_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colour_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colour_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    // colour_blend_attachment.blendEnable = VK_TRUE;
    // colour_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    // colour_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    // colour_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    // colour_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    // colour_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    // colour_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colour_blend_create_info{};
    colour_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colour_blend_create_info.logicOpEnable = VK_FALSE;
    colour_blend_create_info.logicOp = VK_LOGIC_OP_COPY;
    colour_blend_create_info.attachmentCount = 1;
    colour_blend_create_info.pAttachments = &colour_blend_attachment;
    colour_blend_create_info.blendConstants[0] = 0.0f;
    colour_blend_create_info.blendConstants[1] = 0.0f;
    colour_blend_create_info.blendConstants[2] = 0.0f;
    colour_blend_create_info.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout;
    pipeline_layout_create_info.pushConstantRangeCount = 0;
    pipeline_layout_create_info.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(dispatcher->device, &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout.");

    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info{};
    depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_create_info.depthTestEnable = VK_TRUE;
    depth_stencil_create_info.depthWriteEnable = VK_TRUE;
    depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_create_info.minDepthBounds = 0.0f;
    depth_stencil_create_info.maxDepthBounds = 1.0f;
    depth_stencil_create_info.stencilTestEnable = VK_FALSE;
    depth_stencil_create_info.front = {};
    depth_stencil_create_info.back = {};

    VkGraphicsPipelineCreateInfo pipeline_create_info{};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.stageCount = 2;
    pipeline_create_info.pStages = shader_stage_create_infos;
    pipeline_create_info.layout = pipeline_layout;

    pipeline_create_info.pVertexInputState = &vertex_input_create_info;
    pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
    pipeline_create_info.pViewportState = &viewport_state_create_info;
    pipeline_create_info.pRasterizationState = &rasteriser_create_info;
    pipeline_create_info.pMultisampleState = &multisampling;
    pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
    pipeline_create_info.pColorBlendState = &colour_blend_create_info;
    pipeline_create_info.pDynamicState = &dynamic_state_create_info;

    assert(render_pass != VK_NULL_HANDLE);
    pipeline_create_info.renderPass = render_pass;
    pipeline_create_info.subpass = 0;

    pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_create_info.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(dispatcher->device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline.");
}

void Rasteriser::create_framebuffers()
{
    framebuffers.resize(dispatcher->swap_chain.image_views.size());

    for (size_t i = 0; i < dispatcher->swap_chain.image_views.size(); i++) {

        VkImageView attachments[]{ dispatcher->swap_chain.image_views[i], depth_image_view };
        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = render_pass;
        framebuffer_create_info.attachmentCount = static_cast<uint32_t>(SIZE_OF_ARRAY(attachments));
        framebuffer_create_info.pAttachments = attachments;
        framebuffer_create_info.width = dispatcher->swap_chain.extent.width;
        framebuffer_create_info.height = dispatcher->swap_chain.extent.height;
        framebuffer_create_info.layers = 1;

        if (vkCreateFramebuffer(dispatcher->device, &framebuffer_create_info, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer.");
    }
}

void Rasteriser::create_descriptor_pool()
{
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 0;
    for (VkDescriptorPoolSize& pool_size : pool_sizes)
        pool_info.maxSets += pool_size.descriptorCount;

    if (vkCreateDescriptorPool(dispatcher->device, &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool.");
}

void Rasteriser::create_command_pool()
{
    VkCommandPoolCreateInfo cmd_pool_create_info{};
    cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_create_info.queueFamilyIndex = dispatcher->physical_device.graphics_family_idx.value();

    if (vkCreateCommandPool(dispatcher->device, &cmd_pool_create_info, nullptr, &command_pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool.");
}

VkFormat Rasteriser::find_image_format(
    const std::vector<VkFormat>& desirable,
    VkImageTiling tiling,
    VkFormatFeatureFlags features)
{
    for (VkFormat format : desirable) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(dispatcher->physical_device.handle, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
    }

    throw std::runtime_error("Failed to find format.");
}

uint32_t Rasteriser::find_suitable_memory_type(int32_t typeFilter,
                                               VkMemoryPropertyFlags desired_flags)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(dispatcher->physical_device.handle, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & desired_flags) == desired_flags) {
            return i;
        }
    }
    throw std::runtime_error("No suitable memory type found.");
}

void Rasteriser::create_image(
    VkImage& image,
    VkDeviceMemory& memory,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties)
{
    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.format = format;
    image_create_info.tiling = tiling;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = usage;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(dispatcher->device, &image_create_info, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image.");

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(dispatcher->device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_suitable_memory_type(mem_requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(dispatcher->device, &alloc_info, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory.");

    vkBindImageMemory(dispatcher->device, image, memory, 0);
}

VkImageView Rasteriser::create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask)
{
    VkImageViewCreateInfo view_create_info{};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = image;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = format;
    view_create_info.subresourceRange.aspectMask = aspect_mask;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(dispatcher->device, &view_create_info, nullptr, &image_view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture image view.");

    return image_view;
}

VkFormat Rasteriser::depth_format()
{
    return find_image_format(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void Rasteriser::create_depth_image()
{
    VkFormat format = depth_format();
    create_image(depth_image,
                 depth_image_memory,
                 dispatcher->swap_chain.extent.width,
                 dispatcher->swap_chain.extent.height,
                 format,
                 VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    depth_image_view = create_image_view(depth_image, format, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Rasteriser::create_buffer(VkBuffer& buffer,
                               VkDeviceMemory& memory,
                               VkDeviceSize size,
                               VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags mem_flags)
{
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = usage;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(dispatcher->device, &buffer_create_info, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create vertex buffer.");

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(dispatcher->device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_suitable_memory_type(mem_requirements.memoryTypeBits, mem_flags);

    if (vkAllocateMemory(dispatcher->device, &alloc_info, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate memory for vertex buffer.");

    vkBindBufferMemory(dispatcher->device, buffer, memory, 0);
}

void Rasteriser::transfer_to_buffer(VkBuffer& buffer, VkDeviceMemory& memory, const void* src_data, size_t size, VkBufferUsageFlags flags)
{
    VkDeviceSize buffer_size = size;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(staging_buffer,
                  staging_buffer_memory,
                  buffer_size,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* mapped;
    vkMapMemory(dispatcher->device, staging_buffer_memory, 0, buffer_size, 0, &mapped);
    memcpy(mapped, src_data, size);
    vkUnmapMemory(dispatcher->device, staging_buffer_memory);

    create_buffer(buffer,
                  memory,
                  buffer_size,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | flags,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    copy_buffer(staging_buffer, buffer, buffer_size);

    vkDestroyBuffer(dispatcher->device, staging_buffer, nullptr);
    vkFreeMemory(dispatcher->device, staging_buffer_memory, nullptr);
}

void Rasteriser::copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(dispatcher->device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = size;
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(dispatcher->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(dispatcher->graphics_queue);

    vkFreeCommandBuffers(dispatcher->device, command_pool, 1, &command_buffer);
}

void Rasteriser::create_descriptor_set()
{
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout;

    if (vkAllocateDescriptorSets(dispatcher->device, &alloc_info, &descriptor_set) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set.");

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = uniform_buffer;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(UniformBufferObject);

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = descriptor_set;
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = &buffer_info;
    descriptor_write.pImageInfo = nullptr;
    descriptor_write.pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(dispatcher->device, 1, &descriptor_write, 0, nullptr);
}

void Rasteriser::create_command_buffer()
{
    VkCommandBufferAllocateInfo cmd_buffer_alloc_info{};
    cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_alloc_info.commandPool = command_pool;
    cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(dispatcher->device, &cmd_buffer_alloc_info, &command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers.");
}

void Rasteriser::write_command_buffer(size_t image_idx)
{
    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass;
    render_pass_begin_info.framebuffer = framebuffers[image_idx];
    render_pass_begin_info.renderArea.offset = { 0, 0 };
    render_pass_begin_info.renderArea.extent = dispatcher->swap_chain.extent;

    VkClearValue clear_values[2]{};
    clear_values[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clear_values[1].depthStencil = { 1.0f, 0 };
    render_pass_begin_info.clearValueCount = static_cast<uint32_t>(SIZE_OF_ARRAY(clear_values));
    render_pass_begin_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)dispatcher->swap_chain.extent.width;
    viewport.height = (float)dispatcher->swap_chain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = dispatcher->swap_chain.extent;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkBuffer vertex_buffers[] = { vertex_buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

    vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

    // TODO change vertex count
    vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(scene->get_meshes()[0].get_indexed_triangles().size() * 3), 1, 0, 0, 0);
}

void Rasteriser::create_sync_objects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(dispatcher->device, &semaphoreInfo, nullptr, &image_semaphore) != VK_SUCCESS ||
        vkCreateSemaphore(dispatcher->device, &semaphoreInfo, nullptr, &render_semaphore) != VK_SUCCESS)
        throw std::runtime_error("Failed to create semaphore.");

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(dispatcher->device, &fenceInfo, nullptr, &render_fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to create fence.");
}

void Rasteriser::update_uniforms()
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    glm::vec3 view_pos(4.0f, 4.0f, 4.0f);
    glm::vec3 light_dir(0.0f, 0.0f, -10.0f);

    UniformBufferObject ubo;
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::rotate(model, time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 view = glm::lookAt(view_pos, glm::vec3(0.0f, 0.0f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), dispatcher->swap_chain.extent.width / (float)dispatcher->swap_chain.extent.height, 0.1f, 10.0f);
    proj[1][1] *= -1.0f;

    ubo.mvp = proj * view * model;
    ubo.normal = glm::mat3(glm::transpose(glm::inverse(view * model)));
    ubo.view_pos = view_pos;
    ubo.light_dir_view_space_norm = glm::normalize(view * glm::vec4(light_dir, 1.0f));

    memcpy(uniform_buffer_map, &ubo, sizeof(UniformBufferObject));
}

void Rasteriser::set_scene(const Scene& scene)
{
    this->scene = &scene;

    const Mesh& mesh = scene.get_meshes()[0];

    transfer_to_buffer(
        vertex_buffer,
        vertex_buffer_memory,
        mesh.get_vertices().data(),
        mesh.get_vertices().size() * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    transfer_to_buffer(
        index_buffer,
        index_buffer_memory,
        mesh.get_indexed_triangles().data(),
        mesh.get_indexed_triangles().size() * sizeof(IndexedTriangle),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void Rasteriser::new_frame()
{
    if (scene == nullptr)
        throw std::runtime_error("No scene has been set.");

    vkWaitForFences(dispatcher->device, 1, &render_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(dispatcher->device, 1, &render_fence);
    vkAcquireNextImageKHR(dispatcher->device, dispatcher->swap_chain.handle, UINT64_MAX, image_semaphore, VK_NULL_HANDLE, &current_image_index);

    vkResetCommandBuffer(command_buffer, 0);

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin command buffer.");

    write_command_buffer(current_image_index);

    update_uniforms();
}

void Rasteriser::end_frame()
{
    // TODO: test frame begun

    vkCmdEndRenderPass(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to write command buffer.");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { image_semaphore };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkSemaphore signal_semaphores[] = { render_semaphore };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(dispatcher->graphics_queue, 1, &submit_info, render_fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit to queue.");

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swap_chains[] = { dispatcher->swap_chain.handle };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &current_image_index;

    present_info.pResults = nullptr;

    vkQueuePresentKHR(dispatcher->present_queue, &present_info);
}