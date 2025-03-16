#ifndef POSTPROCESS_POSTPROCESS_H_INCLUDED
#define POSTPROCESS_POSTPROCESS_H_INCLUDED

#include "graphics/cmdpool.h"
#include "graphics/descset.h"
#include "graphics/shader.h"

class PostProcessing
{
  public:
    PostProcessing(Device& device,
                   DescriptorPool& descriptor_pool,
                   CommandPool& command_pool,
                   VkExtent2D extent,
                   VkImageView source_image_view,
                   VkImageView result_image_view);
    ~PostProcessing();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

    void write_command_buffer(VkCommandBuffer command_buffer);

    void set_source_image(VkImageView source_image_view);
    void set_result_image(VkImageView result_image_view);
    const VkExtent2D& get_extent() const { return extent; };
    void set_extent(VkExtent2D extent) { this->extent = extent; };

  private:
    Device& device;

    VkExtent2D extent;

    DescriptorSetLayout descriptor_set_layout;
    DescriptorSet descriptor_set;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    void create_pipeline();

    NO_COPY(PostProcessing);
};

#endif