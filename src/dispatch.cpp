#include "dispatch.h"

#include "rasterise.h"
#include "raytrace.h"
#include "ui.h"

Dispatcher::Dispatcher(Device& device, DispatchUsage usage)
    : device(device)
    , usage(usage)
{
    create_descriptor_pool();
    create_command_pool();
}

Dispatcher::~Dispatcher()
{
    vkDestroyCommandPool(device.logical, command_pool, nullptr);
    vkDestroyDescriptorPool(device.logical, descriptor_pool, nullptr);
}

void Dispatcher::create_descriptor_pool()
{
    std::vector<VkDescriptorPoolSize> pool_sizes;
    if (usage & DISPATCH_USAGE_RASTERISER_BIT) {
        std::vector<VkDescriptorPoolSize> extra = Rasteriser::get_descriptor_pool_sizes();
        pool_sizes.insert(pool_sizes.end(), extra.begin(), extra.end());
    }
    if (usage & DISPATCH_USAGE_UI_BIT) {
        std::vector<VkDescriptorPoolSize> extra = UserInterface::get_descriptor_pool_sizes();
        pool_sizes.insert(pool_sizes.end(), extra.begin(), extra.end());
    }
    if (usage & DISPATCH_USAGE_RAY_TRACE_BIT) {
        std::vector<VkDescriptorPoolSize> extra = RayTracer::get_descriptor_pool_sizes();
        pool_sizes.insert(pool_sizes.end(), extra.begin(), extra.end());
    }

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = 0;
    for (VkDescriptorPoolSize& pool_size : pool_sizes)
        pool_info.maxSets += pool_size.descriptorCount;

    if (vkCreateDescriptorPool(device.logical, &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool.");
}

void Dispatcher::create_command_pool()
{
    VkCommandPoolCreateInfo cmd_pool_create_info{};
    cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_create_info.queueFamilyIndex = device.physical_device_info.graphics_family_idx.value();

    if (vkCreateCommandPool(device.logical, &cmd_pool_create_info, nullptr, &command_pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool.");
}

VkCommandBuffer Dispatcher::begin_one_time_use_command_buffer()
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device.logical, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);
    return command_buffer;
}

void Dispatcher::end_one_time_use_command_buffer(VkCommandBuffer command_buffer, VkQueue queue)
{
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device.logical, command_pool, 1, &command_buffer);
}

void Dispatcher::transfer_to_buffer(VkBuffer& buffer, const void* src_data, size_t size)
{
    VkDeviceSize buffer_size = size;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    device.create_buffer(staging_buffer,
                         staging_buffer_memory,
                         buffer_size,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* mapped;
    vkMapMemory(device.logical, staging_buffer_memory, 0, buffer_size, 0, &mapped);
    memcpy(mapped, src_data, size);
    vkUnmapMemory(device.logical, staging_buffer_memory);

    copy_buffer(staging_buffer, buffer, buffer_size);

    vkDestroyBuffer(device.logical, staging_buffer, nullptr);
    vkFreeMemory(device.logical, staging_buffer_memory, nullptr);
}

void Dispatcher::copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device.logical, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = size;
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(device.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(device.graphics_queue);

    vkFreeCommandBuffers(device.logical, command_pool, 1, &command_buffer);
}