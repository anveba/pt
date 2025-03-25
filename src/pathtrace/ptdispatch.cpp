#include "ptdispatch.h"

#include <chrono>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes()
{
    std::vector<VkDescriptorPoolSize> pool_sizes;

    std::vector<VkDescriptorPoolSize> extra = PathTracer::get_descriptor_pool_sizes();
    pool_sizes.insert(pool_sizes.end(), extra.begin(), extra.end());

    extra = PostProcessor::get_descriptor_pool_sizes();
    pool_sizes.insert(pool_sizes.end(), extra.begin(), extra.end());

    return pool_sizes;
}

PathTraceDispatcher::PathTraceDispatcher(const Scene& scene,
                                         uint32_t width,
                                         uint32_t height,
                                         const std::vector<const char*>& validation_layers)
    : context(ContextUsage(CONTEXT_USAGE_MINIMUM), validation_layers)
    , device(context, DeviceUsage(DEVICE_USAGE_RAY_TRACE_BIT), nullptr)
    , descriptor_pool(device, get_descriptor_pool_sizes())
    , command_pool(device)
    , result_image(device, command_pool, { width, height }, VK_FORMAT_R8G8B8A8_UNORM, VkImageUsageFlagBits(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT), VK_IMAGE_LAYOUT_GENERAL, VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), VK_IMAGE_TILING_LINEAR)
    , path_tracer(device, descriptor_pool, command_pool, scene, { width, height })
    , tone_mapper(device, descriptor_pool, command_pool, { width, height }, path_tracer.get_accumulation_image().get_view(), result_image.get_view())
    , render_fence(device, false)
{
}

PathTraceDispatcher::~PathTraceDispatcher()
{
}

void PathTraceDispatcher::start(const PathTraceParameters& parameters)
{
    path_tracer.set_camera(command_pool, parameters.camera);
    path_tracer.set_max_bounces(parameters.max_bounces);
    path_tracer.set_samples(parameters.samples);
    path_tracer.update_uniforms();

    VkCommandBuffer command_buffer = command_pool.create_command_buffer();

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin command buffer.");

    path_tracer.write_command_buffer(command_buffer);

    tone_mapper.write_command_buffer(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to write command buffer.");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    render_fence.reset();

    auto start_time = std::chrono::high_resolution_clock::now();

    if (vkQueueSubmit(device.get_graphics_queue(), 1, &submit_info, render_fence.handle()) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit to queue.");

    render_fence.wait();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> time_taken = end_time - start_time;

    std::cout << "Rendering took " << time_taken.count() << " seconds.\n";
    std::cout << "Writing image to " << parameters.out_path << std::endl;

    command_pool.destroy_command_buffer(command_buffer);

    VkImageSubresource subresource{};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    VkSubresourceLayout subresource_layout;

    vkGetImageSubresourceLayout(device.logical_handle(), result_image.handle(), &subresource, &subresource_layout);

    const uint8_t* map;
    vkMapMemory(device.logical_handle(), result_image.get_memory(), 0, VK_WHOLE_SIZE, 0, (void**)&map);
    map += subresource_layout.offset;

    uint32_t width = result_image.get_extent().width, height = result_image.get_extent().height;
    uint32_t* data = new uint32_t[width * height];

    for (uint32_t y = 0; y < height; y++) {
        uint32_t* row = (uint32_t*)map;
        for (uint32_t x = 0; x < width; x++)
            data[y * width + x] = row[x];

        map += subresource_layout.rowPitch;
    }

    vkUnmapMemory(device.logical_handle(), result_image.get_memory());

    stbi_write_png(parameters.out_path.c_str(), width, height, 4, data, 4 * width);

    delete[] data;
}