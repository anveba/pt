#ifndef PATHTRACE_PTDISPATCH_H_INCLUDED
#define PATHTRACE_PTDISPATCH_H_INCLUDED

#include "scene/scene.h"
#include "util.h"

struct PathTraceParameters
{
    uint32_t samples;
    uint32_t max_bounces;
};

class PathTraceDispatch
{
    PathTraceDispatch(const Scene& scene,
                      VkExtent2D extent);
    ~PathTraceDispatch();

    void render(const PathTraceParameters& parameters);

  private:
    NO_COPY(PathTraceDispatch);
};

#endif