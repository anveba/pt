#include "rasterise.h"

#include <cassert>

std::vector<VkDescriptorPoolSize> Rasteriser::get_descriptor_pool_sizes()
{
    return { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
}

static std::vector<VkDescriptorSetLayoutBinding> get_descriptor_set_layout_bindings()
{
    VkDescriptorSetLayoutBinding ubo_layout_binding = DescriptorSetLayout::create_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    return { ubo_layout_binding };
}

Rasteriser::Rasteriser(
    Device& device,
    DescriptorPool& descriptor_pool,
    CommandPool& command_pool,
    const Scene& scene,
    VkExtent2D extent,
    VkFormat image_format,
    VkFormat depth_format)
    : device(device)
    , scene_buffer(device, command_pool, scene, VkBufferUsageFlagBits(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
    , uniform_buffer(device, sizeof(RasteriseUniformData))
    , descriptor_set_layout(device, get_descriptor_set_layout_bindings())
    , descriptor_set(descriptor_pool, descriptor_set_layout)
    , depth_format(depth_format)
{
    set_extent(extent.width, extent.height);
    create_render_pass(image_format, depth_format);
    create_pipeline();

    set_scene(command_pool, scene);
    update_descriptor_set();
}

Rasteriser::~Rasteriser()
{
    vkDestroyPipeline(device.logical_handle(), pipeline, nullptr);
    vkDestroyPipelineLayout(device.logical_handle(), pipeline_layout, nullptr);
    vkDestroyRenderPass(device.logical_handle(), render_pass, nullptr);
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

    if (vkCreateRenderPass(device.logical_handle(), &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass.");
}

void Rasteriser::create_pipeline()
{
    Shader vs(device, "bin/shaders/rasterise/rasterise.vs.spv");
    Shader ps(device, "bin/shaders/rasterise/rasterise.ps.spv");

    VkPipelineShaderStageCreateInfo shader_stage_create_infos[2] = { {}, {} };

    shader_stage_create_infos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stage_create_infos[0].module = vs.handle();
    shader_stage_create_infos[0].pName = "main";

    shader_stage_create_infos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_create_infos[1].module = ps.handle();
    shader_stage_create_infos[1].pName = "main";

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
        RasteriseInstanceData::binding_description(1)
    };

    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
    auto vertex_attributes = Vertex::attribute_descriptions(0, 0);
    attribute_descriptions.insert(attribute_descriptions.end(), vertex_attributes.begin(), vertex_attributes.end());
    auto instance_attributes = RasteriseInstanceData::attribute_descriptions(1, vertex_attributes.size());
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
    colour_blend_attachment.blendEnable = VK_TRUE;
    colour_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colour_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colour_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    colour_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colour_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colour_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

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
    pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout.handle();
    pipeline_layout_create_info.pushConstantRangeCount = 0;
    pipeline_layout_create_info.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(device.logical_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
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

    if (vkCreateGraphicsPipelines(device.logical_handle(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline.");
}

void Rasteriser::update_descriptor_set()
{
    VkDescriptorBufferInfo buffer_info = DescriptorSet::create_descriptor(uniform_buffer.handle(), sizeof(RasteriseUniformData));
    VkWriteDescriptorSet descriptor_write = descriptor_set.write_descriptor_set(&buffer_info, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    DescriptorSet::update_write_descriptors(device, &descriptor_write, 1);
}

void Rasteriser::update_uniforms()
{
    memcpy(uniform_buffer.get_map(), &uniform_data, sizeof(RasteriseUniformData));
}

void Rasteriser::write_command_buffer(VkCommandBuffer command_buffer, VkFramebuffer framebuffer)
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

    VkDeviceSize vertex_offsets[] = { scene_buffer.get_vertex_offset() };
    VkDeviceSize instance_offsets[] = { scene_buffer.get_instance_offset() };
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &scene_buffer.handle(), vertex_offsets);

    vkCmdBindVertexBuffers(command_buffer, 1, 1, &scene_buffer.handle(), instance_offsets);

    vkCmdBindIndexBuffer(command_buffer, scene_buffer.handle(), scene_buffer.get_index_offset(), VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set.handle(), 0, nullptr);

    for (size_t i = 0; i < scene_buffer.object_variant_count(); i++) {
        const BufferIndices start_indices = scene_buffer.get_start_indices(i);
        const BufferIndices end_indices = scene_buffer.get_start_indices(i + 1);
        uint32_t index_count = end_indices.index - start_indices.index;
        uint32_t instance_count = end_indices.instance - start_indices.instance;
        vkCmdDrawIndexed(command_buffer, index_count, instance_count, start_indices.index, start_indices.vertex, 0);
    }
}

void Rasteriser::set_extent(uint32_t width, uint32_t height)
{
    extent.width = width;
    extent.height = height;
}

void Rasteriser::set_scene(CommandPool& command_pool, const Scene& scene)
{
    scene_buffer.rebuild(command_pool, scene);
}

void Rasteriser::set_camera(CommandPool& command_pool, const Camera& camera)
{
    glm::vec3 light_dir(0.0f, -10.0f, 0.0f);

    Mat4 view = camera.view_matrix();
    Mat4 proj = camera.projection_matrix();

    uniform_data.mvp = proj * view;
    uniform_data.view_pos = camera.position;
    uniform_data.inv_light_dir_norm = glm::normalize(-light_dir);
}