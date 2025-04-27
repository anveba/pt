#ifndef PATHTRACE_PTDISPATCH_H_INCLUDED
#define PATHTRACE_PTDISPATCH_H_INCLUDED

#include "compute/tonemap.h"
#include "graphics/fence.h"
#include "graphics/semaphore.h"
#include "initablearray.h"
#include "io/imagewrite.h"
#include "pathtrace.h"
#include "scene/scene.h"
#include "util.h"

struct PathTraceParameters
{
    const Camera& camera;
    std::string out_path;
    OutputImageFormat output_format;

    uint32_t samples;
    uint32_t max_bounces;
    float render_time;
};

class PathTraceDispatcher
{
  public:
    PathTraceDispatcher(const Scene& scene,
                        uint32_t width,
                        uint32_t height,
                        const std::vector<const char*>& validation_layers);
    ~PathTraceDispatcher();

    void start(const PathTraceParameters& parameters);

  private:
    VulkanContext context;
    Device device;
    DescriptorPool descriptor_pool;
    CommandPool command_pool;
    StorageImage accumulation_image;
    StorageImage result_image;
    PathTracer path_tracer;
    ToneMapper<1> tone_mapper;

    size_t current_dispatch_index;
    InitableArray<Semaphore, PathTracer::IN_FLIGHT> semaphores;
    InitableArray<Fence, PathTracer::IN_FLIGHT> fences;
    std::array<VkCommandBuffer, PathTracer::IN_FLIGHT> command_buffers;

    void render(const PathTraceParameters& parameters, uint32_t& samples_taken);
    void write_command_buffers();
    void put_result(const PathTraceParameters& parameters);

    NO_COPY(PathTraceDispatcher);
};

#endif