#ifndef GRAPHICS_SHAREDMEM_H_INCLUDED
#define GRAPHICS_SHAREDMEM_H_INCLUDED

#include "device.h"

class SharedMemory
{
  public:
    SharedMemory(Device& device, VkDeviceSize size, VkBufferUsageFlagBits usage)
        : device(device)
        , size(size)
    {
        device.create_buffer(buffer, memory, size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkMapMemory(device.logical_handle(), memory, 0, size, 0, (void**)&map);
    }

    ~SharedMemory()
    {
        vkDestroyBuffer(device.logical_handle(), buffer, nullptr);
        vkUnmapMemory(device.logical_handle(), memory);
        vkFreeMemory(device.logical_handle(), memory, nullptr);
    }

    inline const VkBuffer& handle() const { return buffer; }

    inline void* get_map() const { return map; }
    inline size_t get_size() const { return size; }

  private:
    Device& device;
    VkDeviceSize size;

    VkBuffer buffer;
    VkDeviceMemory memory;
    void* map;

    NO_COPY(SharedMemory);
};

#endif
