#ifndef ACCSTRUCT_H_INCLUDED
#define ACCSTRUCT_H_INCLUDED

#include "scene/scenebuffer.h"

class AccelerationStructure
{
  public:
    // The scene and scene buffer given must correspond to one another.
    AccelerationStructure(Device& device,
                          CommandPool& command_pool,
                          const Scene& scene,
                          const SceneBuffer& scene_buffer);
    ~AccelerationStructure();

    void rebuild(CommandPool& command_pool, const Scene& scene, const SceneBuffer& scene_buffer);

    inline const VkAccelerationStructureKHR& get_top_level() const { return tlas; }

  private:
    Device& device;

    VkBuffer blas_buffer;
    VkDeviceMemory blas_memory;
    std::vector<VkAccelerationStructureKHR> blas;

    VkBuffer tlas_buffer;
    VkDeviceMemory tlas_memory;
    VkAccelerationStructureKHR tlas;

    void create_blas(CommandPool& command_pool, const Scene& scene, const SceneBuffer& scene_buffer);
    void create_tlas(CommandPool& command_pool, const Scene& scene);
    void free();

    NO_COPY(AccelerationStructure);
};

#endif