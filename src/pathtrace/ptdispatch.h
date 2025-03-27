#ifndef PATHTRACE_PTDISPATCH_H_INCLUDED
#define PATHTRACE_PTDISPATCH_H_INCLUDED

#include "graphics/fence.h"
#include "io/imagewrite.h"
#include "pathtrace.h"
#include "postprocess/tonemap.h"
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

    void render(VkCommandBuffer& command_buffer,
                const PathTraceParameters& parameters,
                float& time_taken,
                uint32_t& samples_taken);
    void put_result(VkCommandBuffer& command_buffer, const PathTraceParameters& parameters);

  private:
    VulkanContext context;
    Device device;
    DescriptorPool descriptor_pool;
    CommandPool command_pool;
    StorageImage result_image;
    PathTracer path_tracer;
    ToneMapper tone_mapper;

    Fence fence;

    NO_COPY(PathTraceDispatcher);
};

#endif