#ifndef ACCSTRUCT_H_INCLUDED
#define ACCSTRUCT_H_INCLUDED

#include "scene/scenebuffer.h"

class AccelerationStructure
{
  public:
    AccelerationStructure(Device& device, CommandPool& command_pool, const SceneBuffer<PathTraceInstanceData>& scene_buffer);
    ~AccelerationStructure();

    void rebuild(CommandPool& command_pool, const SceneBuffer<PathTraceInstanceData>& scene_buffer);

    inline const VkAccelerationStructureKHR& get_top_level() const { return tlas; }

  private:
    Device& device;
    const SceneBuffer<PathTraceInstanceData>* scene_buffer;

    VkBuffer blas_buffer;
    VkDeviceMemory blas_memory;
    std::vector<VkAccelerationStructureKHR> blas;

    VkBuffer tlas_buffer;
    VkDeviceMemory tlas_memory;
    VkAccelerationStructureKHR tlas;

    void create_blas(CommandPool& command_pool);
    void create_tlas(CommandPool& command_pool);
    void free();

    NO_COPY(AccelerationStructure);
};

#endif