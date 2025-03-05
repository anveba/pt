#include "rasterise.h"
#include "dispatch.h"
#include "display.h"

#include "scene.h"
#include <cassert>

struct InstanceData
{
    Mat4 transform;
    Mat3x4 normal;

    static VkVertexInputBindingDescription binding_description(uint32_t binding)
    {
        VkVertexInputBindingDescription binding_description{};
        binding_description.binding = binding;
        binding_description.stride = sizeof(InstanceData);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        return binding_description;
    }

    static std::array<VkVertexInputAttributeDescription, 7> attribute_descriptions(uint32_t binding, uint32_t location_offset)
    {
        std::array<VkVertexInputAttributeDescription, 7> attribute_descriptions;
        attribute_descriptions[0].binding = binding;
        attribute_descriptions[0].location = location_offset + 0;
        attribute_descriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attribute_descriptions[0].offset = sizeof(float) * 4 * 0;
        attribute_descriptions[1].binding = binding;
        attribute_descriptions[1].location = location_offset + 1;
        attribute_descriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attribute_descriptions[1].offset = sizeof(float) * 4 * 1;
        attribute_descriptions[2].binding = binding;
        attribute_descriptions[2].location = location_offset + 2;
        attribute_descriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attribute_descriptions[2].offset = sizeof(float) * 4 * 2;
        attribute_descriptions[3].binding = binding;
        attribute_descriptions[3].location = location_offset + 3;
        attribute_descriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attribute_descriptions[3].offset = sizeof(float) * 4 * 3;
        attribute_descriptions[4].binding = binding;
        attribute_descriptions[4].location = location_offset + 4;
        attribute_descriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[4].offset = sizeof(float) * 4 * 4;
        attribute_descriptions[5].binding = binding;
        attribute_descriptions[5].location = location_offset + 5;
        attribute_descriptions[5].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[5].offset = sizeof(float) * 4 * 5;
        attribute_descriptions[6].binding = binding;
        attribute_descriptions[6].location = location_offset + 6;
        attribute_descriptions[6].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[6].offset = sizeof(float) * 4 * 6;
        return attribute_descriptions;
    }
};

std::vector<VkDescriptorPoolSize> Rasteriser::get_descriptor_pool_sizes()
{
    return { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
}

Rasteriser::Rasteriser(
    Device& device,
    Dispatcher& dispatcher,
    const Shader& vs,
    const Shader& ps,
    VkExtent2D extent,
    VkFormat image_format,
    VkFormat depth_format)
    : device(device)
    , depth_format(depth_format)
    , scene(nullptr)
    , in_render(false)
{
    set_extent(extent.width, extent.height);
    create_render_pass(image_format, depth_format);
    create_descriptor_set_layout();
    create_pipeline(vs, ps);

    device.create_buffer(uniform_buffer, uniform_buffer_memory, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkMapMemory(device.logical, uniform_buffer_memory, 0, sizeof(UniformBufferObject), 0, (void**)&uniform_buffer_map);

    create_descriptor_set(dispatcher);
    create_command_buffer(dispatcher);
    create_sync_objects();
}

Rasteriser::~Rasteriser()
{
    if (scene != nullptr)
        free_scene_buffers();

    vkDestroyBuffer(device.logical, uniform_buffer, nullptr);
    vkUnmapMemory(device.logical, uniform_buffer_memory);
    vkFreeMemory(device.logical, uniform_buffer_memory, nullptr);

    vkDestroySemaphore(device.logical, render_semaphore, nullptr);
    vkDestroyFence(device.logical, render_fence, nullptr);

    vkDestroyPipeline(device.logical, pipeline, nullptr);
    vkDestroyPipelineLayout(device.logical, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device.logical, descriptor_set_layout, nullptr);
    vkDestroyRenderPass(device.logical, render_pass, nullptr);
}

void Rasteriser::create_render_pass(VkFormat image_format, VkFormat depth_format)
{
    VkAttachmentDescription colour_attachment{};
    colour_attachment.format = image_format;
    colour_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colour_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colour_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colour_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colour_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colour_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colour_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // TODO

    VkAttachmentReference colour_attachment_ref{};
    colour_attachment_ref.attachment = 0;
    colour_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_format;
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

    std::array<VkAttachmentDescription, 2> attachments = { colour_attachment, depth_attachment };
    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_create_info.pAttachments = attachments.data();
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &dependency;

    if (vkCreateRenderPass(device.logical, &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS)
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

    if (vkCreateDescriptorSetLayout(device.logical, &layout_create_info, nullptr, &descriptor_set_layout) != VK_SUCCESS)
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

    std::vector<VkVertexInputBindingDescription> binding_descriptions = {
        Vertex::binding_description(0),
        InstanceData::binding_description(1)
    };

    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
    auto vertex_attributes = Vertex::attribute_descriptions(0, 0);
    attribute_descriptions.insert(attribute_descriptions.end(), vertex_attributes.begin(), vertex_attributes.end());
    auto instance_attributes = InstanceData::attribute_descriptions(1, vertex_attributes.size());
    attribute_descriptions.insert(attribute_descriptions.end(), instance_attributes.begin(), instance_attributes.end());
    assert(attribute_descriptions.size() == 10);

    VkPipelineVertexInputStateCreateInfo vertex_input_create_info{};
    vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_create_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_descriptions.size());
    vertex_input_create_info.pVertexBindingDescriptions = binding_descriptions.data();
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
    rasteriser_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

    if (vkCreatePipelineLayout(device.logical, &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
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

    if (vkCreateGraphicsPipelines(device.logical, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline.");
}

void Rasteriser::create_descriptor_set(Dispatcher& dispatch)
{
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = dispatch.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout;

    if (vkAllocateDescriptorSets(device.logical, &alloc_info, &descriptor_set) != VK_SUCCESS)
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

    vkUpdateDescriptorSets(device.logical, 1, &descriptor_write, 0, nullptr);
}

void Rasteriser::create_command_buffer(Dispatcher& dispatch)
{
    VkCommandBufferAllocateInfo cmd_buffer_alloc_info{};
    cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_alloc_info.commandPool = dispatch.command_pool;
    cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device.logical, &cmd_buffer_alloc_info, &command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffer.");
}

// TODO do not write each frame
void Rasteriser::write_command_buffer(VkFramebuffer framebuffer)
{
    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass;
    render_pass_begin_info.framebuffer = framebuffer;
    render_pass_begin_info.renderArea.offset = { 0, 0 };
    render_pass_begin_info.renderArea.extent = extent;

    std::array<VkClearValue, 2> clear_values = {};
    clear_values[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clear_values[1].depthStencil = { 1.0f, 0 };
    render_pass_begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    render_pass_begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, offsets);

    vkCmdBindVertexBuffers(command_buffer, 1, 1, &instance_buffer, offsets);

    vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

    assert(vertex_end_indices.size() == index_end_indices.size());
    size_t variant_count = vertex_end_indices.size();
    for (size_t i = 0; i < variant_count; i++) {
        uint32_t index_count = index_end_indices[i] - (i == 0 ? 0 : index_end_indices[i - 1]);
        uint32_t instance_count = instance_end_indices[i] - (i == 0 ? 0 : instance_end_indices[i - 1]);
        uint32_t index_index = i == 0 ? 0 : index_end_indices[i - 1];
        uint32_t vertex_index = i == 0 ? 0 : vertex_end_indices[i - 1];
        vkCmdDrawIndexed(command_buffer, index_count, instance_count, index_index, vertex_index, 0);
    }
}

void Rasteriser::create_sync_objects()
{
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(device.logical, &semaphore_create_info, nullptr, &render_semaphore) != VK_SUCCESS)
        throw std::runtime_error("Failed to create semaphore.");

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(device.logical, &fence_create_info, nullptr, &render_fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to create fence.");
}

void Rasteriser::set_extent(uint32_t width, uint32_t height)
{
    extent.width = width;
    extent.height = height;
}

void Rasteriser::free_scene_buffers()
{
    assert(this->scene);
    vkDestroyBuffer(device.logical, vertex_buffer, nullptr);
    vkFreeMemory(device.logical, vertex_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical, index_buffer, nullptr);
    vkFreeMemory(device.logical, index_buffer_memory, nullptr);
    vkDestroyBuffer(device.logical, instance_buffer, nullptr);
    vkFreeMemory(device.logical, instance_buffer_memory, nullptr);
}

void Rasteriser::set_scene(Dispatcher& dispatcher, const Scene& scene)
{
    if (this->scene != nullptr)
        free_scene_buffers();

    this->scene = &scene;

    size_t vertex_count = 0, index_count = 0, instance_count = 0;
    const std::vector<ObjectVariant>& object_variants = scene.get_object_variants();

    for (const ObjectVariant& variant : object_variants) {
        vertex_count += variant.mesh.get_vertices().size();
        index_count += variant.mesh.get_indexed_triangles().size();
        instance_count += variant.instances.size();
    }

    std::vector<Vertex> all_vertices;
    all_vertices.reserve(vertex_count);
    std::vector<IndexedTriangle> all_indices;
    all_indices.reserve(index_count);
    std::vector<InstanceData> all_instance_data;
    all_indices.reserve(instance_count);

    vertex_end_indices.clear();
    vertex_end_indices.reserve(object_variants.size());
    index_end_indices.clear();
    index_end_indices.reserve(object_variants.size());
    instance_end_indices.clear();
    instance_end_indices.reserve(object_variants.size());

    for (const ObjectVariant& variant : object_variants) {

        const auto& vertices = variant.mesh.get_vertices();
        all_vertices.insert(all_vertices.end(), vertices.begin(), vertices.end());
        vertex_end_indices.push_back(static_cast<uint32_t>(all_vertices.size()));

        const auto& indices = variant.mesh.get_indexed_triangles();
        all_indices.insert(all_indices.end(), indices.begin(), indices.end());
        index_end_indices.push_back(static_cast<uint32_t>(all_indices.size()) * 3);

        for (const Instance inst : variant.instances) {
            InstanceData data;
            data.transform = inst.transform.matrix;
            data.normal = glm::transpose(glm::inverse(inst.transform.matrix));
            all_instance_data.push_back(data);
        }
        instance_end_indices.push_back(static_cast<uint32_t>(all_instance_data.size()));
    }

    device.create_buffer(vertex_buffer,
                         vertex_buffer_memory,
                         all_vertices.size() * sizeof(Vertex),
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    dispatcher.transfer_to_buffer(vertex_buffer,
                                  all_vertices.data(),
                                  all_vertices.size() * sizeof(Vertex));

    device.create_buffer(index_buffer,
                         index_buffer_memory,
                         all_indices.size() * sizeof(IndexedTriangle),
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    dispatcher.transfer_to_buffer(index_buffer,
                                  all_indices.data(),
                                  all_indices.size() * sizeof(IndexedTriangle));

    device.create_buffer(instance_buffer,
                         instance_buffer_memory,
                         all_instance_data.size() * sizeof(InstanceData),
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    dispatcher.transfer_to_buffer(instance_buffer,
                                  all_instance_data.data(),
                                  all_instance_data.size() * sizeof(InstanceData));
}

void Rasteriser::set_camera(Dispatcher& dispatcher, const Camera& camera)
{
    if (scene == nullptr)
        throw std::runtime_error("No scene has been set.");

    glm::vec3 light_dir(0.0f, -10.0f, 0.0f);

    Mat4 model = scene->global_transform().matrix;
    Mat4 view = camera.view_matrix();
    Mat4 proj = camera.projection_matrix();

    uniform_buffer_map->mvp = proj * view * model;
    uniform_buffer_map->normal = glm::transpose(glm::inverse(model));
    uniform_buffer_map->view_pos = camera.position;
    uniform_buffer_map->inv_light_dir_norm = glm::normalize(-light_dir);
}

void Rasteriser::wait_for_render()
{
    vkWaitForFences(device.logical, 1, &render_fence, VK_TRUE, UINT64_MAX);
}

void Rasteriser::begin_render(VkFramebuffer framebuffer)
{
    if (scene == nullptr)
        throw std::runtime_error("No scene has been set.");
    if (in_render)
        throw std::runtime_error("Rasteriser is already rendering.");

    vkResetCommandBuffer(command_buffer, 0);

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin command buffer.");

    write_command_buffer(framebuffer);

    in_render = true;
}

VkSemaphore Rasteriser::end_render(VkSemaphore* wait_for, uint32_t semaphore_count)
{
    if (!in_render)
        throw std::runtime_error("Render ended before having begun.");

    vkCmdEndRenderPass(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to write command buffer.");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_for;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = semaphore_count;
    submit_info.pCommandBuffers = &command_buffer;

    VkSemaphore signal_semaphores[] = { render_semaphore };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(device.logical, 1, &render_fence);

    if (vkQueueSubmit(device.graphics_queue, 1, &submit_info, render_fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit to queue.");

    in_render = false;
    return render_semaphore;
}