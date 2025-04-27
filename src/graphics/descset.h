#ifndef GRAPHICS_DESCSET_H_INCLUDED
#define GRAPHICS_DESCSET_H_INCLUDED

#include "descpool.h"
#include "util.h"

class DescriptorSet
{
  public:
    DescriptorSet(DescriptorPool& pool, const DescriptorSetLayout& layout);
    ~DescriptorSet();

    static void update_write_descriptors(Device& device, const VkWriteDescriptorSet* write_descriptors, uint32_t count);

    static VkDescriptorBufferInfo create_descriptor(VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0);
    static VkDescriptorImageInfo create_descriptor(VkImageView image_view, VkImageLayout layout, VkSampler sampler = VK_NULL_HANDLE);

    VkWriteDescriptorSet write_descriptor_set(const VkDescriptorBufferInfo* buffer_descriptor,
                                              uint32_t binding,
                                              VkDescriptorType type,
                                              uint32_t count = 1);
    VkWriteDescriptorSet write_descriptor_set(const VkDescriptorImageInfo* image_descriptor,
                                              uint32_t binding,
                                              VkDescriptorType type,
                                              uint32_t count = 1);
    VkWriteDescriptorSet write_descriptor_set(const VkAccelerationStructureKHR& acc_struct,
                                              VkWriteDescriptorSetAccelerationStructureKHR& write_descriptor_set,
                                              uint32_t binding);

    inline const VkDescriptorSet& handle() const { return descriptor_set; }

  private:
    VkDescriptorSet descriptor_set;

    DescriptorPool& pool;

    NO_COPY(DescriptorSet);
};

#endif