#ifndef POSTPROCESS_TONEMAP_H_INCLUDED
#define POSTPROCESS_TONEMAP_H_INCLUDED

#include "postprocess.h"

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
               bool dst_is_srgb = false);

    void set_parameters(uint32_t width, uint32_t height, bool src_is_srgb = false, bool dst_is_srgb = false);
    void set_source_image(VkImageView image_view);
    void set_result_image(VkImageView image_view);
    void write_command_buffer(VkCommandBuffer command_buffer);

  private:
    Shader shader;
    UniformBuffer uniform_buffer;
    PostProcessor post_processor;

    NO_COPY(ToneMapper);
};

#endif