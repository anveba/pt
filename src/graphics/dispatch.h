#ifndef GRAPHICS_DISPATCH_H_INCLUDED
#define GRAPHICS_DISPATCH_H_INCLUDED

#include "device.h"

enum DispatchUsage
{
    DISPATCH_USAGE_MINIMUM = 0,
    DISPATCH_USAGE_RASTERISER_BIT = 1,
    DISPATCH_USAGE_RAY_TRACE_BIT = 2,
    DISPATCH_USAGE_UI_BIT = 4,
};

class Dispatcher
{
  public:
    Dispatcher(Device& device, DispatchUsage usage);
    ~Dispatcher();

  private:
    Device& device;
    const DispatchUsage usage;

    VkDescriptorPool descriptor_pool;
    VkCommandPool command_pool;

    void transfer_to_buffer(VkBuffer& buffer, const void* src_data, size_t size);
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

    void create_descriptor_pool();
    void create_command_pool();

    VkCommandBuffer begin_one_time_use_command_buffer();
    void end_one_time_use_command_buffer(VkCommandBuffer command_buffer, VkQueue queue);

    friend class Rasteriser;
    friend class UserInterface;
    friend class PathTracer;

    NO_COPY(Dispatcher);
};

#endif