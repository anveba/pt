#ifndef PATHTRACE_WAVEQUEUE_H_INCLUDED
#define PATHTRACE_WAVEQUEUE_H_INCLUDED

#include "constants.h"
#include "graphics/device.h"

class WavefrontQueue
{
  public:
    WavefrontQueue(Device& device, VkDeviceSize queue_capacity)
        : device(device)
    {
        create(queue_capacity);
    }

    ~WavefrontQueue()
    {
        destroy();
    }

    void rebuild(VkDeviceSize queue_capacity) {
        destroy();
        create(queue_capacity);
    }

    inline const VkBuffer& handle() const { return buffer; }
    inline size_t get_queue_max_count() const { return queue_capacity; }
    inline size_t get_total_size() const { return QUEUE_OFFSET(queue_capacity, 2) * 4; }

  private:
    Device& device;
    VkDeviceSize queue_capacity;

    VkBuffer buffer;
    VkDeviceMemory memory;

    void create(VkDeviceSize queue_capacity) {
        this->queue_capacity = queue_capacity;
        device.create_buffer(buffer, memory, get_total_size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    void destroy() {
        vkDestroyBuffer(device.logical_handle(), buffer, nullptr);
        vkFreeMemory(device.logical_handle(), memory, nullptr);
    }

    NO_COPY(WavefrontQueue);
};

#endif