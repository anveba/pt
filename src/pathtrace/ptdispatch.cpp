#include "ptdispatch.h"

void PathTraceDispatch::start(const PathTraceParameters& parameters)
{
    VkCommandBuffer command_buffer = command_pool.create_command_buffer();

    command_pool.destroy_command_buffer(command_buffer);
}