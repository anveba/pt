#include "ptdispatch.h"

#include <chrono>

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
    , result_image(device, command_pool, { width, height }, VK_FORMAT_R32G32B32A32_SFLOAT, VkImageUsageFlagBits(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT), VK_IMAGE_LAYOUT_GENERAL, VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), VK_IMAGE_TILING_LINEAR)
    , path_tracer(device, descriptor_pool, command_pool, scene, { width, height })
    , tone_mapper(device, descriptor_pool, command_pool, { width, height }, path_tracer.get_accumulation_image().get_view(), path_tracer.get_accumulation_image().get_view(), false, true)
    , fence(device, false)
{
}

PathTraceDispatcher::~PathTraceDispatcher()
{
}

void PathTraceDispatcher::start(const PathTraceParameters& parameters)
{
    if (parameters.output_format == OutputImageFormat::NONE)
        throw std::runtime_error("No output format was given.");

    path_tracer.set_camera(command_pool, parameters.camera);
    path_tracer.set_max_bounces(parameters.max_bounces);

    VkCommandBuffer command_buffer = command_pool.create_command_buffer();

    float time_taken;
    uint32_t samples_taken;
    render(command_buffer, parameters, time_taken, samples_taken);

    std::cout << "Rendering took " << time_taken << " seconds with " << samples_taken << " samples taken per pixel.\n";

    float samples_per_second = samples_taken / time_taken;
    std::cout << samples_per_second << " samples per second.\n";

    std::cout << "Writing image to " << parameters.out_path << std::endl;

    put_result(command_buffer, parameters);

    command_pool.destroy_command_buffer(command_buffer);

    VkImageSubresource subresource{};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    VkSubresourceLayout subresource_layout;

    vkGetImageSubresourceLayout(device.logical_handle(), result_image.handle(), &subresource, &subresource_layout);

    const uint8_t* map;
    vkMapMemory(device.logical_handle(), result_image.get_memory(), 0, VK_WHOLE_SIZE, 0, (void**)&map);
    map += subresource_layout.offset;

    if (parameters.output_format == OutputImageFormat::HDR)
        write_hdr(map, parameters.out_path, result_image.get_extent().width, result_image.get_extent().height, subresource_layout.rowPitch);
    else if (parameters.output_format == OutputImageFormat::PNG)
        write_png(map, parameters.out_path, result_image.get_extent().width, result_image.get_extent().height, subresource_layout.rowPitch);
    else
        throw std::runtime_error("Unknown image format.");

    vkUnmapMemory(device.logical_handle(), result_image.get_memory());
}

void PathTraceDispatcher::render(
    VkCommandBuffer& command_buffer,
    const PathTraceParameters& parameters,
    float& time_taken,
    uint32_t& samples_taken)
{
    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin command buffer.");

    path_tracer.write_command_buffer(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to write command buffer.");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    time_taken = 0.0f;
    samples_taken = 0;

    auto start_time = std::chrono::high_resolution_clock::now();
    uint32_t samples_to_dispatch = 8; // TODO find a good value

    do {
        if (samples_taken + samples_to_dispatch > parameters.samples)
            samples_to_dispatch = parameters.samples - samples_taken;

        path_tracer.set_samples(samples_to_dispatch);
        path_tracer.update_uniforms();

        fence.reset();

        if (vkQueueSubmit(device.get_graphics_queue(), 1, &submit_info, fence.handle()) != VK_SUCCESS)
            throw std::runtime_error("Failed to submit to queue.");

        fence.wait();

        time_taken = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start_time).count();
        samples_taken += samples_to_dispatch;

    } while (time_taken < parameters.render_time && samples_taken < parameters.samples);
}

void PathTraceDispatcher::put_result(VkCommandBuffer& command_buffer, const PathTraceParameters& parameters)
{
    vkResetCommandBuffer(command_buffer, 0);

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin command buffer.");

    if (parameters.output_format != OutputImageFormat::HDR)
        tone_mapper.write_command_buffer(command_buffer);

    copy_image(command_buffer,
               path_tracer.get_accumulation_image().handle(),
               path_tracer.get_accumulation_image().get_layout(),
               result_image.handle(),
               result_image.get_layout(),
               result_image.get_extent().width,
               result_image.get_extent().height);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to write command buffer.");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    fence.reset();

    if (vkQueueSubmit(device.get_graphics_queue(), 1, &submit_info, fence.handle()) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit to queue.");

    fence.wait();
}
