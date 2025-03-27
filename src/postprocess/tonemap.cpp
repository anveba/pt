#include "tonemap.h"

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

ToneMapper::ToneMapper(
    Device& device,
    DescriptorPool& descriptor_pool,
    CommandPool& command_pool,
    VkExtent2D extent,
    VkImageView source_image_view,
    VkImageView result_image_view,
    bool src_is_srgb,
    bool dst_is_srgb)
    : shader(device, "bin/shaders/postprocess/tonemap.cs.spv")
    , uniform_buffer(device, sizeof(ToneMapUbo))
    , post_processor(device, descriptor_pool, command_pool, shader, uniform_buffer, get_tone_map_group_counts(extent.width, extent.height), source_image_view, result_image_view)
{
    ToneMapUbo* map = (ToneMapUbo*)uniform_buffer.get_map();
    map->width = extent.width;
    map->height = extent.height;
    map->src_is_srgb = (uint32_t)src_is_srgb;
    map->dst_is_srgb = (uint32_t)dst_is_srgb;
}

void ToneMapper::set_parameters(uint32_t width, uint32_t height, bool src_is_srgb, bool dst_is_srgb)
{
    ToneMapUbo* map = (ToneMapUbo*)uniform_buffer.get_map();
    map->width = width;
    map->height = height;
    map->src_is_srgb = (uint32_t)src_is_srgb;
    map->dst_is_srgb = (uint32_t)dst_is_srgb;
    post_processor.set_group_counts(get_tone_map_group_counts(width, height));
}

void ToneMapper::set_source_image(VkImageView image_view)
{
    post_processor.set_source_image(image_view);
}

void ToneMapper::set_result_image(VkImageView image_view)
{
    post_processor.set_result_image(image_view);
}

void ToneMapper::write_command_buffer(VkCommandBuffer command_buffer)
{
    post_processor.write_command_buffer(command_buffer);
}
