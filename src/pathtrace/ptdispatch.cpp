#include "ptdispatch.h"

// PathTraceDispatcher::PathTraceDispatcher(const std::vector<const char*>& validation_layers)
//     : context(ContextUsage(CONTEXT_USAGE_MINIMUM), validation_layers)
//     , window(context, width, height)
//     , device(context, DeviceUsage(DEVICE_USAGE_RAY_TRACE_BIT), &window)
//     , descriptor_pool(device, get_descriptor_pool_sizes())
//     , command_pool(device)

//           void PathTraceDispatcher::start(const PathTraceParameters& parameters)
// {
//     VkCommandBuffer command_buffer = command_pool.create_command_buffer();

//     command_pool.destroy_command_buffer(command_buffer);
// }