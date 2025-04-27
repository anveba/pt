#ifndef GRAPHICS_CMDPOOL_H_INCLUDED
#define GRAPHICS_CMDPOOL_H_INCLUDED

#include "device.h"
#include "util.h"

class CommandPool
{
  public:
    CommandPool(Device& device);
    ~CommandPool();

    // TODO move and change so a caller can use their own command buffer
    void transfer_to_buffer(VkBuffer& buffer, const void* src_data, size_t size);
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

    VkCommandBuffer begin_one_time_use_command_buffer();
    void end_one_time_use_command_buffer(VkCommandBuffer command_buffer, VkQueue queue);

    // TODO move to its own class
    void create_command_buffers(VkCommandBuffer* handles, uint32_t count);
    void destroy_command_buffers(const VkCommandBuffer* handles, uint32_t count);

  private:
    Device& device;
    VkCommandPool command_pool;

    NO_COPY(CommandPool);
};

#endif