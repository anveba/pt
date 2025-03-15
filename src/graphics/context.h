#ifndef GRAPHICS_CONTEXT_H_INCLUDED
#define GRAPHICS_CONTEXT_H_INCLUDED

#include "util.h"
#include <vector>

enum ContextUsage
{
    CONTEXT_USAGE_MINIMUM = 0,
    CONTEXT_USAGE_WINDOW_BIT = 1,
    CONTEXT_USAGE_RAY_TRACE_BIT = 2,
};

class VulkanContext
{
  public:
    VulkanContext(ContextUsage usage, const std::vector<const char*>& validation_layers);
    ~VulkanContext();

    inline const VkInstance& handle() const { return instance; }

    inline const std::vector<const char*>& get_validation_layers() const { return validation_layers; }
    inline const ContextUsage& get_usage() const { return usage; }

  private:
    const std::vector<const char*> validation_layers;

    VkInstance instance;
    const ContextUsage usage;

    void init_instance(ContextUsage usage);

    NO_COPY(VulkanContext);
};

#endif