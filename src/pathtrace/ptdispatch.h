#ifndef PATHTRACE_PTDISPATCH_H_INCLUDED
#define PATHTRACE_PTDISPATCH_H_INCLUDED

#include "pathtrace.h"
#include "scene/scene.h"
#include "util.h"

struct PathTraceParameters
{
    uint32_t samples;
    uint32_t max_bounces;

    std::string out_path;
};

class PathTraceDispatch
{
    PathTraceDispatch(const Scene& scene,
                      VkExtent2D extent);
    ~PathTraceDispatch();

    void start(const PathTraceParameters& parameters);

  private:
    Device& device;
    CommandPool& command_pool;
    PathTracer path_tracer;

    NO_COPY(PathTraceDispatch);
};

#endif