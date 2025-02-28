#include "raytrace.h"

#include "dispatch.h"
#include "rtarget.h"
#include "scene.h"

#include <cassert>

std::vector<VkDescriptorPoolSize> RayTracer::get_descriptor_pool_sizes()
{
    return { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 } };
}

RayTracer::RayTracer(
    Device& device,
    Dispatcher& dispatcher,
    const Shader& vs,
    const Shader& ps,
    VkExtent2D extent,
    VkFormat image_format)
    : device(&device)
    , extent(extent)
    , scene(nullptr)
    , in_render(false)
{
    create_descriptor_set_layout();
    create_pipeline();

    create_descriptor_set(dispatcher);
    create_command_buffer(dispatcher);
    create_sync_objects();
}

RayTracer::~RayTracer()
{
    if (scene != nullptr)
        free_scene_buffers();

    vkDestroySemaphore(device->logical, image_semaphore, nullptr);
    vkDestroySemaphore(device->logical, render_semaphore, nullptr);
    vkDestroyFence(device->logical, render_fence, nullptr);

    vkDestroyPipeline(device->logical, pipeline, nullptr);
    vkDestroyPipelineLayout(device->logical, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device->logical, descriptor_set_layout, nullptr);
}

void RayTracer::create_descriptor_set_layout()
{
}

void RayTracer::create_pipeline()
{
}

void RayTracer::create_descriptor_set(Dispatcher& dispatch)
{
}

void RayTracer::create_command_buffer(Dispatcher& dispatch)
{
    VkCommandBufferAllocateInfo cmd_buffer_alloc_info{};
    cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_alloc_info.commandPool = dispatch.command_pool;
    cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device->logical, &cmd_buffer_alloc_info, &command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers.");
}

void RayTracer::write_command_buffer(VkFramebuffer framebuffer)
{
}

void RayTracer::create_sync_objects()
{
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(device->logical, &semaphore_create_info, nullptr, &image_semaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device->logical, &semaphore_create_info, nullptr, &render_semaphore) != VK_SUCCESS)
        throw std::runtime_error("Failed to create semaphore.");

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(device->logical, &fence_create_info, nullptr, &render_fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to create fence.");
}

void RayTracer::free_scene_buffers()
{
    assert(this->scene);
}

void RayTracer::set_scene(Dispatcher& dispatcher, const Scene& scene)
{
    if (this->scene != nullptr)
        free_scene_buffers();

    this->scene = &scene;
}

void RayTracer::set_camera(Dispatcher& dispatcher, const Camera& camera)
{
    if (scene == nullptr)
        throw std::runtime_error("No scene has been set.");
}

void RayTracer::begin_render(IRenderTarget* render_target)
{
    if (scene == nullptr)
        throw std::runtime_error("No scene has been set.");
    if (render_target == nullptr)
        throw std::runtime_error("The render target was null");
    if (in_render)
        throw std::runtime_error("Ray tracer is already rendering.");

    vkWaitForFences(device->logical, 1, &render_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device->logical, 1, &render_fence);
    VkFramebuffer framebuffer = render_target->acquire(image_semaphore);

    vkResetCommandBuffer(command_buffer, 0);

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin command buffer.");

    write_command_buffer(framebuffer);

    in_render = true;
}

VkSemaphore RayTracer::end_render()
{
    if (!in_render)
        throw std::runtime_error("Render ended before having begun.");

    vkCmdEndRenderPass(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to write command buffer.");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_semaphore;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkSemaphore signal_semaphores[] = { render_semaphore };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(device->graphics_queue, 1, &submit_info, render_fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit to queue.");

    in_render = false;
    return render_semaphore;
}