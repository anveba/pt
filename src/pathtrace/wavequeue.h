#ifndef PATHTRACE_WAVEQUEUE_H_INCLUDED
#define PATHTRACE_WAVEQUEUE_H_INCLUDED

#include "constants.h"
#include "graphics/device.h"

class WavefrontQueue
{
  public:
    WavefrontQueue(Device& device, VkDeviceSize queue_max_count)
        : device(device)
        , queue_max_count(queue_max_count)
    {
        device.create_buffer(buffer, memory, get_total_size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    ~WavefrontQueue()
    {
        vkDestroyBuffer(device.logical_handle(), buffer, nullptr);
        vkFreeMemory(device.logical_handle(), memory, nullptr);
    }

    inline const VkBuffer& handle() const { return buffer; }
    inline size_t get_queue_max_count() const { return queue_max_count; }
    inline size_t get_total_size() const { return QUEUE_OFFSET(queue_max_count, 2) * 4; }

  private:
    Device& device;
    VkDeviceSize queue_max_count;

    VkBuffer buffer;
    VkDeviceMemory memory;

    NO_COPY(WavefrontQueue);
};

#endif