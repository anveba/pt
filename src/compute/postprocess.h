#ifndef COMPUTE_POSTPROCESS_H_INCLUDED
#define COMPUTE_POSTPROCESS_H_INCLUDED

#include "compute.h"
#include "graphics/cmdpool.h"
#include "graphics/descset.h"
#include "graphics/shader.h"
#include "graphics/sharedmem.h"
#include "lalgebra.h"

template<size_t InFlight>
class PostProcessor
{
  public:
    PostProcessor(Device& device,
                  DescriptorPool& descriptor_pool,
                  CommandPool& command_pool,
                  const Shader& shader,
                  const SharedMemory& ubo,
                  uint32_t uniform_data_size,
                  uint32_t aligned_uniform_data_size,
                  Uint3 group_counts,
                  VkImageView source_image_view,
                  VkImageView result_image_view)
        : desc_set_layout(device, descriptor_set_layout_bindings())
        , descriptor_sets(descriptor_pool, desc_set_layout)
        , compute(device, descriptor_pool, command_pool, shader, desc_set_layout, descriptor_sets, group_counts)
    {
        set_source_image(source_image_view);
        set_result_image(result_image_view);
        set_uniform_descriptors(ubo, uniform_data_size, aligned_uniform_data_size);
    }

    ~PostProcessor()
    {
    }

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes()
    {
        return { { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * InFlight },
                 { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, InFlight } };
    }

    void write_command_buffer(VkCommandBuffer command_buffer, size_t flight_index)
    {
        compute.write_command_buffer(command_buffer, flight_index);
    }

    void set_source_image(VkImageView source_image_view)
    {
        VkDescriptorImageInfo source_image_descriptor = DescriptorSet::create_descriptor(source_image_view, VK_IMAGE_LAYOUT_GENERAL);
        VkWriteDescriptorSet source_image_writes[InFlight];
        for (size_t i = 0; i < InFlight; i++)
            source_image_writes[i] = descriptor_sets[i].write_descriptor_set(&source_image_descriptor, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        DescriptorSet::update_write_descriptors(compute.get_device(), source_image_writes, InFlight);
    }

    void set_result_image(VkImageView result_image_view)
    {
        VkDescriptorImageInfo result_image_descriptor = DescriptorSet::create_descriptor(result_image_view, VK_IMAGE_LAYOUT_GENERAL);
        VkWriteDescriptorSet result_image_writes[InFlight];
        for (size_t i = 0; i < InFlight; i++)
            result_image_writes[i] = descriptor_sets[i].write_descriptor_set(&result_image_descriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        DescriptorSet::update_write_descriptors(compute.get_device(), result_image_writes, InFlight);
    }

    const Uint3& get_group_counts() const { return compute.get_group_counts(); };
    void set_group_counts(Uint3 group_counts) { compute.set_group_counts(group_counts); };

  private:
    DescriptorSetLayout desc_set_layout;
    InitableArray<DescriptorSet, InFlight> descriptor_sets;
    Compute<InFlight> compute;

    void set_uniform_descriptors(const SharedMemory& ubo, uint32_t uniform_data_size, uint32_t aligned_uniform_data_size)
    {
        VkWriteDescriptorSet uniform_descriptor_writes[InFlight];
        VkDescriptorBufferInfo uniform_descriptors[InFlight];
        for (size_t i = 0; i < InFlight; i++) {
            uniform_descriptors[i] = DescriptorSet::create_descriptor(ubo.handle(), uniform_data_size, i * aligned_uniform_data_size);
            uniform_descriptor_writes[i] = descriptor_sets[i].write_descriptor_set(&uniform_descriptors[i], 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        }
        DescriptorSet::update_write_descriptors(compute.get_device(), uniform_descriptor_writes, InFlight);
    }

    static std::vector<VkDescriptorSetLayoutBinding> descriptor_set_layout_bindings()
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

    NO_COPY(PostProcessor);
};

#endif