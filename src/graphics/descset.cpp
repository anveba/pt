#include "descset.h"

DescriptorSet::DescriptorSet(
    DescriptorPool& pool,
    const DescriptorSetLayout& layout)
    : pool(pool)
{
    descriptor_set = pool.allocate_set(layout);
}

DescriptorSet::~DescriptorSet()
{
    pool.free_set(descriptor_set);
}

void DescriptorSet::update_write_descriptors(Device& device, const VkWriteDescriptorSet* write_descriptors, uint32_t count)
{
    vkUpdateDescriptorSets(device.logical_handle(), count, write_descriptors, 0, VK_NULL_HANDLE);
}

VkDescriptorBufferInfo DescriptorSet::create_descriptor(VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset)
{
    VkDescriptorBufferInfo buffer_descriptor{};
    buffer_descriptor.buffer = buffer;
    buffer_descriptor.range = size;
    buffer_descriptor.offset = offset;
    return buffer_descriptor;
}

VkDescriptorImageInfo DescriptorSet::create_descriptor(VkImageView image_view, VkImageLayout layout)
{
    VkDescriptorImageInfo image_descriptor{};
    image_descriptor.imageView = image_view;
    image_descriptor.imageLayout = layout;
    return image_descriptor;
}

VkWriteDescriptorSet DescriptorSet::write_descriptor_set(
    const VkDescriptorBufferInfo& buffer_descriptor,
    uint32_t binding,
    VkDescriptorType type)
{
    VkWriteDescriptorSet buffer_write{};
    buffer_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    buffer_write.dstSet = descriptor_set;
    buffer_write.descriptorType = type;
    buffer_write.dstBinding = binding;
    buffer_write.pBufferInfo = &buffer_descriptor;
    buffer_write.descriptorCount = 1;
    return buffer_write;
}

VkWriteDescriptorSet DescriptorSet::write_descriptor_set(const VkDescriptorImageInfo& image_descriptor, uint32_t binding, VkDescriptorType type)
{
    VkWriteDescriptorSet image_write{};
    image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    image_write.dstSet = descriptor_set;
    image_write.descriptorType = type;
    image_write.dstBinding = binding;
    image_write.pImageInfo = &image_descriptor;
    image_write.descriptorCount = 1;
    return image_write;
}

VkWriteDescriptorSet DescriptorSet::write_descriptor_set(
    const VkAccelerationStructureKHR& acc_struct,
    VkWriteDescriptorSetAccelerationStructureKHR& write_descriptor_set,
    uint32_t binding)
{
    write_descriptor_set = {};
    write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    write_descriptor_set.accelerationStructureCount = 1;
    write_descriptor_set.pAccelerationStructures = &acc_struct;

    VkWriteDescriptorSet acceleration_structure_write{};
    acceleration_structure_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    acceleration_structure_write.dstSet = descriptor_set;
    acceleration_structure_write.dstBinding = binding;
    acceleration_structure_write.descriptorCount = 1;
    acceleration_structure_write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    acceleration_structure_write.pNext = &write_descriptor_set;
    return acceleration_structure_write;
}
