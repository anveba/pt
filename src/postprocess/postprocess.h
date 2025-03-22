#ifndef POSTPROCESS_POSTPROCESS_H_INCLUDED
#define POSTPROCESS_POSTPROCESS_H_INCLUDED

#include "graphics/cmdpool.h"
#include "graphics/descset.h"
#include "graphics/shader.h"
#include "graphics/ubo.h"
#include "lalgebra.h"

class PostProcessor
{
  public:
    PostProcessor(Device& device,
                  DescriptorPool& descriptor_pool,
                  CommandPool& command_pool,
                  const Shader& shader,
                  const UniformBuffer& ubo,
                  Uint3 group_counts,
                  VkImageView source_image_view,
                  VkImageView result_image_view);
    ~PostProcessor();

    static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes();

    void write_command_buffer(VkCommandBuffer command_buffer);

    void set_source_image(VkImageView source_image_view);
    void set_result_image(VkImageView result_image_view);
    const Uint3& get_group_counts() const { return group_counts; };
    void set_group_counts(Uint3 group_counts) { this->group_counts = group_counts; };

  private:
    Device& device;
    const Shader& shader;
    const UniformBuffer& ubo;

    Uint3 group_counts;

    DescriptorSetLayout descriptor_set_layout;
    DescriptorSet descriptor_set;

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    void set_uniform_descriptor();

    void create_pipeline();

    NO_COPY(PostProcessor);
};

#endif