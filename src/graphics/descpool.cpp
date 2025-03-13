#include "descpool.h"

DescriptorPool::DescriptorPool(Device& device, const VkDescriptorPoolSize* pool_sizes, uint32_t sizes_count)
    : device(device)
{
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.poolSizeCount = sizes_count;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 0;
    for (uint32_t i = 0; i < sizes_count; i++)
        pool_info.maxSets += pool_sizes[i].descriptorCount;

    if (vkCreateDescriptorPool(device.logical_handle(), &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool.");
}

DescriptorPool::DescriptorPool(Device& device, const std::vector<VkDescriptorPoolSize>& pool_sizes)
    : DescriptorPool(device, pool_sizes.data(), static_cast<uint32_t>(pool_sizes.size()))
{
}

DescriptorPool::~DescriptorPool()
{
    vkDestroyDescriptorPool(device.logical_handle(), descriptor_pool, nullptr);
}

VkDescriptorSet DescriptorPool::allocate_set(const DescriptorSetLayout& layout)
{
    VkDescriptorSet descriptor_set;

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout.layout;

    if (vkAllocateDescriptorSets(device.logical_handle(), &alloc_info, &descriptor_set) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set.");

    return descriptor_set;
}

void DescriptorPool::free_set(VkDescriptorSet descriptor_set)
{
    vkFreeDescriptorSets(device.logical_handle(), descriptor_pool, 1, &descriptor_set);
}
