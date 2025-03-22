#include "cmdpool.h"

CommandPool::CommandPool(Device& device)
    : device(device)
{
    VkCommandPoolCreateInfo cmd_pool_create_info{};
    cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_create_info.queueFamilyIndex = device.get_physical_device_info().graphics_family_idx.value();

    if (vkCreateCommandPool(device.logical_handle(), &cmd_pool_create_info, nullptr, &command_pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool.");
}

CommandPool::~CommandPool()
{
    vkDestroyCommandPool(device.logical_handle(), command_pool, nullptr);
}

VkCommandBuffer CommandPool::begin_one_time_use_command_buffer()
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device.logical_handle(), &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);
    return command_buffer;
}

void CommandPool::end_one_time_use_command_buffer(VkCommandBuffer command_buffer, VkQueue queue)
{
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device.logical_handle(), command_pool, 1, &command_buffer);
}

VkCommandBuffer CommandPool::create_command_buffer()
{
    VkCommandBufferAllocateInfo cmd_buffer_alloc_info{};
    cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_alloc_info.commandPool = command_pool;
    cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    if (vkAllocateCommandBuffers(device.logical_handle(), &cmd_buffer_alloc_info, &command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffer.");
    return command_buffer;
}

void CommandPool::destroy_command_buffer(VkCommandBuffer command_buffer)
{
    vkFreeCommandBuffers(device.logical_handle(), command_pool, 1, &command_buffer);
}

void CommandPool::transfer_to_buffer(VkBuffer& buffer, const void* src_data, size_t size)
{
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    device.create_buffer(staging_buffer,
                         staging_buffer_memory,
                         size,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* mapped;
    vkMapMemory(device.logical_handle(), staging_buffer_memory, 0, size, 0, &mapped);
    memcpy(mapped, src_data, size);
    vkUnmapMemory(device.logical_handle(), staging_buffer_memory);

    copy_buffer(staging_buffer, buffer, size);

    vkDestroyBuffer(device.logical_handle(), staging_buffer, nullptr);
    vkFreeMemory(device.logical_handle(), staging_buffer_memory, nullptr);
}

void CommandPool::copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkCommandBuffer command_buffer = begin_one_time_use_command_buffer();

    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = size;
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy);

    end_one_time_use_command_buffer(command_buffer, device.get_graphics_queue());
}