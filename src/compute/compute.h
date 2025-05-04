#ifndef COMPUTE_COMPUTE_H_INCLUDED
#define COMPUTE_COMPUTE_H_INCLUDED

#include "graphics/cmdpool.h"
#include "graphics/descset.h"
#include "graphics/shader.h"
#include "graphics/sharedmem.h"
#include "initablearray.h"
#include "lalgebra.h"

template<size_t InFlight>
class Compute
{
  public:
    Compute(Device& device,
            DescriptorPool& descriptor_pool,
            CommandPool& command_pool,
            const Shader& shader,
            const DescriptorSetLayout& descriptor_set_layout,
            const InitableArray<DescriptorSet, InFlight>& descriptor_sets,
            Uint3 group_counts)
        : device(device)
        , shader(shader)
        , group_counts(group_counts)
        , descriptor_set_layout(descriptor_set_layout)
        , descriptor_sets(descriptor_sets)
    {
        create_pipeline();
    }

    ~Compute()
    {
        vkDestroyPipeline(device.logical_handle(), pipeline, nullptr);
        vkDestroyPipelineLayout(device.logical_handle(), pipeline_layout, nullptr);
    }

    void dispatch(VkCommandBuffer command_buffer)
    {
        vkCmdDispatch(command_buffer, group_counts.x, group_counts.y, group_counts.z);
    }

    void bind(VkCommandBuffer command_buffer, size_t flight_index) {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_sets[flight_index].handle(), 0, 0);
    }

    const Uint3& get_group_counts() const { return group_counts; };
    void set_group_counts(Uint3 group_counts) { this->group_counts = group_counts; };

    inline Device& get_device() { return device; }

    static Uint3 compute_group_counts_2d(uint32_t width, uint32_t height, uint32_t group_size)
    {
        uint32_t group_width = width / group_size + ((width % group_size) > 0 ? 1 : 0);
        uint32_t group_height = height / group_size + ((height % group_size) > 0 ? 1 : 0);
        return Uint3(group_width, group_height, 1);
    }

  private:
    Device& device;
    const Shader& shader;

    Uint3 group_counts;

    const DescriptorSetLayout& descriptor_set_layout;
    const InitableArray<DescriptorSet, InFlight>& descriptor_sets;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    void create_pipeline()
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

    NO_COPY(Compute);
};

#endif