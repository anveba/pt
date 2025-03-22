#include "postprocess.h"
#include <array>

std::vector<VkDescriptorPoolSize> PostProcessor::get_descriptor_pool_sizes()
{
    return { { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 } };
}

static std::vector<VkDescriptorSetLayoutBinding> get_descriptor_set_layout_bindings()
{
    VkDescriptorSetLayoutBinding source_image_layout_binding = DescriptorSetLayout::create_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    VkDescriptorSetLayoutBinding result_image_layout_binding = DescriptorSetLayout::create_layout_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    VkDescriptorSetLayoutBinding uniform_buffer_binding = DescriptorSetLayout::create_layout_binding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

    return {
        source_image_layout_binding,
        result_image_layout_binding,
        uniform_buffer_binding,
    };
}

PostProcessor::PostProcessor(
    Device& device,
    DescriptorPool& descriptor_pool,
    CommandPool& command_pool,
    const Shader& shader,
    const UniformBuffer& ubo,
    Uint3 group_counts,
    VkImageView source_image_view,
    VkImageView result_image_view)
    : device(device)
    , shader(shader)
    , ubo(ubo)
    , group_counts(group_counts)
    , descriptor_set_layout(device, get_descriptor_set_layout_bindings())
    , descriptor_set(descriptor_pool, descriptor_set_layout)
{
    create_pipeline();
    set_source_image(source_image_view);
    set_result_image(result_image_view);
    set_uniform_descriptor();
}

PostProcessor::~PostProcessor()
{
    vkDestroyPipeline(device.logical_handle(), pipeline, nullptr);
    vkDestroyPipelineLayout(device.logical_handle(), pipeline_layout, nullptr);
}

void PostProcessor::set_source_image(VkImageView source_image_view)
{
    VkDescriptorImageInfo source_image_descriptor = DescriptorSet::create_descriptor(source_image_view, VK_IMAGE_LAYOUT_GENERAL);
    VkWriteDescriptorSet source_image_write = descriptor_set.write_descriptor_set(&source_image_descriptor, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    DescriptorSet::update_write_descriptors(device, &source_image_write, 1);
}

void PostProcessor::set_result_image(VkImageView result_image_view)
{
    VkDescriptorImageInfo result_image_descriptor = DescriptorSet::create_descriptor(result_image_view, VK_IMAGE_LAYOUT_GENERAL);
    VkWriteDescriptorSet result_image_write = descriptor_set.write_descriptor_set(&result_image_descriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    DescriptorSet::update_write_descriptors(device, &result_image_write, 1);
}

void PostProcessor::set_uniform_descriptor()
{
    VkDescriptorBufferInfo uniform_descriptor = DescriptorSet::create_descriptor(ubo.handle(), ubo.get_size(), 0);
    VkWriteDescriptorSet uniform_descriptor_write = descriptor_set.write_descriptor_set(&uniform_descriptor, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    DescriptorSet::update_write_descriptors(device, &uniform_descriptor_write, 1);
}

void PostProcessor::create_pipeline()
{
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout.handle();

    if (vkCreatePipelineLayout(device.logical_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create compute pipeline layout.");

    VkPipelineShaderStageCreateInfo shader_info{};
    shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_info.module = shader.handle();
    shader_info.pName = "main";

    VkComputePipelineCreateInfo pipeline_create_info{};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_create_info.layout = pipeline_layout;
    pipeline_create_info.stage = shader_info;

    if (vkCreateComputePipelines(device.logical_handle(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create compute pipeline.");
}

void PostProcessor::write_command_buffer(VkCommandBuffer command_buffer)
{
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set.handle(), 0, 0);
    vkCmdDispatch(command_buffer, group_counts.x, group_counts.y, group_counts.z);
}