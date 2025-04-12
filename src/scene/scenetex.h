#ifndef SCENE_SCENETEX_H_INCLUDED
#define SCENE_SCENETEX_H_INCLUDED

#include "graphics/cmdpool.h"
#include "scene.h"

struct Texture
{
    VkImage image;
    VkFormat format;
    VkImageView view;
    VkMemoryRequirements memory_requirements;
    VkDeviceSize image_buffer_offset;
};

class SceneTextures
{
  public:
    SceneTextures(Device& device, CommandPool& command_pool, const Scene& scene, VkMemoryAllocateFlags allocate_flags);
    ~SceneTextures();

    inline const std::vector<Texture>& get_textures() const { return textures; }

    void rebuild(CommandPool& command_pool, const Scene& scene);

  private:
    Device& device;

    VkDeviceMemory memory;
    VkMemoryAllocateFlags allocate_flags;

    std::vector<Texture> textures;

    void build(CommandPool& command_pool, const Scene& scene);
    void destroy();

    NO_COPY(SceneTextures);
};

#endif