#ifndef COMPUTE_TONEMAP_H_INCLUDED
#define COMPUTE_TONEMAP_H_INCLUDED

#include "postprocess.h"

struct ToneMapUbo
{
    uint32_t width;
    uint32_t height;
    uint32_t src_is_srgb;
    uint32_t dst_is_srgb;
};

static Uint3 get_tone_map_group_counts(uint32_t width, uint32_t height)
{
    constexpr uint32_t TONE_MAP_GROUP_SIZE = 16;
    width = width / TONE_MAP_GROUP_SIZE + ((width % TONE_MAP_GROUP_SIZE) > 0 ? 1 : 0);
    height = height / TONE_MAP_GROUP_SIZE + ((height % TONE_MAP_GROUP_SIZE) > 0 ? 1 : 0);
    return Uint3(width, height, 1);
}

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
        : shader(device, "bin/shaders/postprocess/tonemap.cs.spv")
        , uniform_buffer(device, InFlight * sizeof(ToneMapUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        , post_processor(device, descriptor_pool, command_pool, shader, uniform_buffer, sizeof(ToneMapUbo), get_tone_map_group_counts(extent.width, extent.height), source_image_view, result_image_view)
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
        post_processor.set_group_counts(get_tone_map_group_counts(width, height));
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
    SharedMemory uniform_buffer;
    ToneMapUbo uniform_data;
    PostProcessor<InFlight> post_processor;

    NO_COPY(ToneMapper);
};

#endif