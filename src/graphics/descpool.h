#ifndef GRAPHICS_DESCPOOL_H_INCLUDED
#define GRAPHICS_DESCPOOL_H_INCLUDED

#include "desclayout.h"
#include "device.h"
#include "util.h"

class DescriptorPool
{
  public:
    DescriptorPool(Device& device, const VkDescriptorPoolSize* pool_sizes, uint32_t sizes_count);
    DescriptorPool(Device& device, const std::vector<VkDescriptorPoolSize>& pool_sizes);
    ~DescriptorPool();

    inline const VkDescriptorPool& handle() const { return descriptor_pool; }
    inline Device& get_device() { return device; }

  private:
    VkDescriptorSet allocate_set(const DescriptorSetLayout& layout);

    void free_set(VkDescriptorSet descriptor_set);

    Device& device;
    VkDescriptorPool descriptor_pool;

    friend class DescriptorSet;

    NO_COPY(DescriptorPool);
};

#endif