#include "context.h"
#include <SDL3/SDL_vulkan.h>
#include <cassert>
#include <cstring>

VulkanContext::VulkanContext(ContextUsage usage, const std::vector<const char*>& validation_layers)
    : validation_layers(validation_layers)
    , usage(usage)
{
    init_instance(usage);
}

VulkanContext::~VulkanContext()
{
    vkDestroyInstance(instance, nullptr);
}

static bool validation_layers_are_supported(const std::vector<const char*>& validation_layers)
{
    uint32_t count;
    VK_ASSERT(vkEnumerateInstanceLayerProperties(&count, nullptr));

    std::vector<VkLayerProperties> available(count);
    VK_ASSERT(vkEnumerateInstanceLayerProperties(&count, available.data()));

    for (const char* layer : validation_layers) {
        bool found = false;
        for (const auto& properties : available) {
            if (!strcmp(layer, properties.layerName)) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    return true;
}

static std::vector<const char*> get_required_instance_extensions(bool enable_validation_layers, ContextUsage usage)
{
    std::vector<const char*> extensions;

    if (usage & CONTEXT_USAGE_WINDOW_BIT) {
        unsigned int sdl_extension_count;
        const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
        if (sdl_extensions == NULL)
            throw std::runtime_error("Failed to get Vulkan extensions required by SDL.");

        for (const char* const* ext = sdl_extensions; ext < sdl_extensions + sdl_extension_count; ext += 1)
            extensions.push_back(*ext);
    }

    if (usage & CONTEXT_USAGE_RAY_TRACE_BIT) {
    }

    if (enable_validation_layers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

void VulkanContext::init_instance(ContextUsage usage)
{
    if (!validation_layers_are_supported(validation_layers))
        throw std::runtime_error("Not all validation layers requested are available.");

    if (!validation_layers.empty())
        std::cout << "Validation layers enabled." << std::endl;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
    create_info.ppEnabledLayerNames = validation_layers.data();

    std::vector<const char*> extensions = get_required_instance_extensions(!validation_layers.empty(), usage);
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    VK_ASSERT(vkCreateInstance(&create_info, nullptr, &instance));
}