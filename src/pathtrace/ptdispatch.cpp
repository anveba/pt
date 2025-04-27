#include "ptdispatch.h"

#include <chrono>

static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes()
{
    std::vector<VkDescriptorPoolSize> pool_sizes;

    std::vector<VkDescriptorPoolSize> extra = PathTracer::get_descriptor_pool_sizes();
    pool_sizes.insert(pool_sizes.end(), extra.begin(), extra.end());

    extra = PostProcessor<PathTracer::IN_FLIGHT>::get_descriptor_pool_sizes();
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
    , accumulation_image(device, command_pool, { width, height }, VK_FORMAT_R32G32B32A32_SFLOAT, VkImageUsageFlagBits(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT), VK_IMAGE_LAYOUT_GENERAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    , result_image(device, command_pool, { width, height }, VK_FORMAT_R32G32B32A32_SFLOAT, VkImageUsageFlagBits(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT), VK_IMAGE_LAYOUT_GENERAL, VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), VK_IMAGE_TILING_LINEAR)
    , path_tracer(device, descriptor_pool, command_pool, scene, accumulation_image)
    , tone_mapper(device, descriptor_pool, command_pool, { width, height }, accumulation_image.get_view(), accumulation_image.get_view(), false, true)
    , current_dispatch_index(0)
    , semaphores(device)
    , fences(device, true)
{
    command_pool.create_command_buffers(command_buffers.data(), command_buffers.size());
}

PathTraceDispatcher::~PathTraceDispatcher()
{
    command_pool.destroy_command_buffers(command_buffers.data(), command_buffers.size());
}

void PathTraceDispatcher::start(const PathTraceParameters& parameters)
{
    if (parameters.output_format == OutputImageFormat::NONE)
        throw std::runtime_error("No output format was given.");
    if (parameters.samples == 0)
        throw std::runtime_error("Sample count is zero.");

    path_tracer.set_camera(command_pool, parameters.camera);
    path_tracer.set_max_bounces(parameters.max_bounces);

    write_command_buffers();

    auto start_time = std::chrono::high_resolution_clock::now();

    uint32_t samples_taken;
    render(parameters, samples_taken);

    size_t last_dispatch_index = current_dispatch_index;

    put_result(parameters);

    fences[last_dispatch_index].wait();

    float time_taken = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start_time).count();

    std::cout << "Rendering took " << time_taken << " seconds with " << samples_taken << " samples taken per pixel.\n";

    float samples_per_second = samples_taken / time_taken;
    std::cout << samples_per_second << " samples per second.\n";

    for (size_t i = 0; i < fences.size(); i++)
        fences[i].wait();

    std::cout << "Writing image to " << parameters.out_path << std::endl;

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

void PathTraceDispatcher::render(const PathTraceParameters& parameters, uint32_t& samples_taken)
{
    samples_taken = 0;

    uint32_t samples_to_dispatch = 1; // TODO find a good value
    bool is_first_dispatch = true;

    do {
        if (samples_taken + samples_to_dispatch > parameters.samples)
            samples_to_dispatch = parameters.samples - samples_taken;

        size_t previous_dispatch_index = current_dispatch_index;
        current_dispatch_index = (current_dispatch_index + 1) % PathTracer::IN_FLIGHT;

        fences[current_dispatch_index].wait();
        fences[current_dispatch_index].reset();

        path_tracer.set_samples(samples_to_dispatch);
        path_tracer.update_uniforms(current_dispatch_index);

        VkSubmitInfo submit_info{};
        VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR };
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers[current_dispatch_index];
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.waitSemaphoreCount = is_first_dispatch ? 0 : 1;
        submit_info.pWaitSemaphores = &semaphores[previous_dispatch_index].handle();
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &semaphores[current_dispatch_index].handle();
        if (vkQueueSubmit(device.get_graphics_queue(), 1, &submit_info, fences[current_dispatch_index].handle()) != VK_SUCCESS)
            throw std::runtime_error("Failed to submit to queue.");

        is_first_dispatch = false;
        samples_taken += samples_to_dispatch;

    } while (samples_taken < parameters.samples);
}

void PathTraceDispatcher::write_command_buffers()
{
    for (size_t i = 0; i < command_buffers.size(); i++) {

        VkCommandBufferBeginInfo cmd_buffer_begin_info{};
        cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buffer_begin_info.flags = 0;
        cmd_buffer_begin_info.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(command_buffers[i], &cmd_buffer_begin_info) != VK_SUCCESS)
            throw std::runtime_error("Failed to begin command buffer.");

        path_tracer.write_command_buffer(command_buffers[i], i);

        if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to write command buffer.");
    }
}

void PathTraceDispatcher::put_result(const PathTraceParameters& parameters)
{
    size_t previous_dispatch_index = current_dispatch_index;
    current_dispatch_index = (current_dispatch_index + 1) % PathTracer::IN_FLIGHT;

    fences[current_dispatch_index].wait();
    fences[current_dispatch_index].reset();

    tone_mapper.update_uniforms(0);

    vkResetCommandBuffer(command_buffers[current_dispatch_index], 0);

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffers[current_dispatch_index], &cmd_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin command buffer.");

    if (parameters.output_format != OutputImageFormat::HDR)
        tone_mapper.write_command_buffer(command_buffers[current_dispatch_index], 0);

    copy_image(command_buffers[current_dispatch_index],
               accumulation_image.handle(),
               accumulation_image.get_layout(),
               result_image.handle(),
               result_image.get_layout(),
               result_image.get_extent().width,
               result_image.get_extent().height);

    if (vkEndCommandBuffer(command_buffers[current_dispatch_index]) != VK_SUCCESS)
        throw std::runtime_error("Failed to write command buffer.");

    VkSubmitInfo submit_info{};
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[current_dispatch_index];
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &semaphores[previous_dispatch_index].handle();
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &semaphores[current_dispatch_index].handle();
    if (vkQueueSubmit(device.get_graphics_queue(), 1, &submit_info, fences[current_dispatch_index].handle()) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit to queue.");
}
