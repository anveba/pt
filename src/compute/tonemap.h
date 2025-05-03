#ifndef COMPUTE_TONEMAP_H_INCLUDED
#define COMPUTE_TONEMAP_H_INCLUDED

#include "constants.h"
#include "postprocess.h"

struct ToneMapUbo
{
    uint32_t width;
    uint32_t height;
    uint32_t src_is_srgb;
    uint32_t dst_is_srgb;
};

template<size_t InFlight>
class ToneMapper
{
  public:
    ToneMapper(Device& device,
               DescriptorPool& descriptor_pool,
               CommandPool& command_pool,
               VkExtent2D extent,
               VkImageView source_image_view,
               VkImageView result_image_view,
               bool src_is_srgb = false,
               bool dst_is_srgb = false)
        : shader(device, "bin/shaders/compute/tonemap.cs.spv")
        , aligned_uniform_size(round_up_to<VkDeviceSize>(sizeof(ToneMapUbo), device.get_physical_device_info().properties.limits.minUniformBufferOffsetAlignment))
        , uniform_buffer(device, InFlight * aligned_uniform_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        , post_processor(device, descriptor_pool, command_pool, shader, uniform_buffer, sizeof(ToneMapUbo), aligned_uniform_size, Uint3(1, 1, 1), source_image_view, result_image_view)
    {
        set_parameters(extent.width, extent.height, src_is_srgb, dst_is_srgb);
    }

    void set_parameters(uint32_t width,
                        uint32_t height,
                        bool src_is_srgb = false,
                        bool dst_is_srgb = false)
    {
        uniform_data.width = width;
        uniform_data.height = height;
        uniform_data.src_is_srgb = (uint32_t)src_is_srgb;
        uniform_data.dst_is_srgb = (uint32_t)dst_is_srgb;
        post_processor.set_group_counts(Compute<InFlight>::compute_group_counts_2d(width, height, TONE_MAP_GROUP_SIZE));
    }

    void update_uniforms(size_t flight_index)
    {
        ToneMapUbo* map = (ToneMapUbo*)uniform_buffer.get_map();
        memcpy(map + flight_index, &uniform_data, sizeof(ToneMapUbo));
    }

    void set_source_image(VkImageView image_view)
    {
        post_processor.set_source_image(image_view);
    }

    void set_result_image(VkImageView image_view)
    {
        post_processor.set_result_image(image_view);
    }

    void write_command_buffer(VkCommandBuffer command_buffer, size_t flight_index)
    {
        post_processor.write_command_buffer(command_buffer, flight_index);
    }

  private:
    Shader shader;

    VkDeviceSize aligned_uniform_size;
    SharedMemory uniform_buffer;
    ToneMapUbo uniform_data;

    PostProcessor<InFlight> post_processor;

    NO_COPY(ToneMapper);
};

#endif