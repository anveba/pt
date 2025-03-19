#ifndef PATHTRACE_PTDISPATCH_H_INCLUDED
#define PATHTRACE_PTDISPATCH_H_INCLUDED

#include "pathtrace.h"
#include "scene/scene.h"
#include "util.h"

struct PathTraceParameters
{
    uint32_t width, height;
    uint32_t samples;
    uint32_t max_bounces;

    std::string out_path;
};

class PathTraceDispatcher
{
    PathTraceDispatcher(const std::vector<const char*>& validation_layers);
    ~PathTraceDispatcher();

    void start(const PathTraceParameters& parameters);

  private:
    VulkanContext context;
    Device device;
    CommandPool command_pool;
    DescriptorPool descriptor_pool;
    PathTracer path_tracer;

    NO_COPY(PathTraceDispatcher);
};

#endif