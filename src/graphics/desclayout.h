#ifndef GRAPHICS_DESCLAYOUT_H_INCLUDED
#define GRAPHICS_DESCLAYOUT_H_INCLUDED

#include "device.h"
#include "util.h"

class DescriptorSetLayout
{
  public:
    DescriptorSetLayout(Device& device,
                        const VkDescriptorSetLayoutBinding* layout_bindings,
                        uint32_t binding_count);
    DescriptorSetLayout(Device& device,
                        const std::vector<VkDescriptorSetLayoutBinding> layout_bindings);
    ~DescriptorSetLayout();

    inline const VkDescriptorSetLayout& handle() const { return layout; }

    static VkDescriptorSetLayoutBinding create_layout_binding(uint32_t binding,
                                                              VkDescriptorType type,
                                                              VkShaderStageFlags stage,
                                                              uint32_t count = 1,
                                                              const VkSampler* samplers = nullptr);

  private:
    Device& device;
    VkDescriptorSetLayout layout;

    friend class DescriptorPool;

    NO_COPY(DescriptorSetLayout);
};

#endif