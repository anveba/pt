#ifndef POSTPROCESS_TONEMAP_H_INCLUDED
#define POSTPROCESS_TONEMAP_H_INCLUDED

#include "postprocess.h"

class ToneMapper {
  public:
    ToneMapper(Device& device,
               DescriptorPool& descriptor_pool,
               CommandPool& command_pool,
               VkExtent2D extent,
               VkImageView source_image_view,
               VkImageView result_image_view);

    void set_extent(VkExtent2D extent);
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