#include "rtdisplayer.h"

#include "display.h"
#include "raytrace.h"
#include "ui.h"

RayTraceDisplayer::RayTraceDisplayer(Display& display, RayTracer& ray_tracer)
    : display(display)
    , ray_tracer(ray_tracer)
{
    create_render_pass();
    create_image_semaphore();
    create_framebuffers();
}

RayTraceDisplayer::~RayTraceDisplayer()
{
    vkDestroySemaphore(display.device.logical, image_semaphore, nullptr);
    destroy_framebuffers();
    vkDestroyRenderPass(display.device.logical, render_pass, nullptr);
}

void RayTraceDisplayer::set_extent(uint32_t width, uint32_t height)
{
    ray_tracer.set_extent(width, height);

    destroy_framebuffers();
    create_framebuffers();
}

void RayTraceDisplayer::set_scene(Dispatcher& dispatcher, const Scene& scene)
{
    ray_tracer.set_scene(dispatcher, scene);
}

void RayTraceDisplayer::set_camera(Dispatcher& dispatcher, const Camera& camera)
{
    ray_tracer.set_camera(dispatcher, camera);
}

void RayTraceDisplayer::wait_idle()
{
    ray_tracer.wait_for_render();
}

void RayTraceDisplayer::begin_render()
{
    ray_tracer.wait_for_render();

    uint32_t index = display.acquire_next_index(image_semaphore);
    ray_tracer.begin_render();
    ray_tracer.copy_result(display.swap_chain.images[index]);

    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass;
    render_pass_begin_info.framebuffer = framebuffers[index];
    render_pass_begin_info.renderArea.offset = { 0, 0 };
    render_pass_begin_info.renderArea.extent = ray_tracer.extent;

    vkCmdBeginRenderPass(ray_tracer.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void RayTraceDisplayer::end_render()
{
    vkCmdEndRenderPass(ray_tracer.command_buffer);
    VkSemaphore render_semaphore = ray_tracer.end_render(&image_semaphore, 1);
    display.present(render_semaphore);
}

void RayTraceDisplayer::get_debug_info(RenderDebugInfo& info)
{
    info = {};
    info.samples = ray_tracer.samples_taken + ray_tracer.get_samples_per_render();
}

void RayTraceDisplayer::set_settings(const UiControlPanel& control_panel)
{
    if (ray_tracer.get_max_bounces() != control_panel.max_bounces)
        ray_tracer.set_max_bounces(control_panel.max_bounces);
    if (ray_tracer.get_samples_per_render() != control_panel.samples_per_frame)
        ray_tracer.set_samples(control_panel.samples_per_frame);
}

VkRenderPass RayTraceDisplayer::get_render_pass()
{
    return render_pass;
}

VkCommandBuffer RayTraceDisplayer::get_command_buffer()
{
    return ray_tracer.command_buffer;
}

void RayTraceDisplayer::create_render_pass()
{
    VkAttachmentDescription colour_attachment{};
    colour_attachment.format = display.surface_format.format;
    colour_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colour_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colour_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colour_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colour_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colour_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colour_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_reference{};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_reference;
    subpass_description.pDepthStencilAttachment = nullptr;
    subpass_description.inputAttachmentCount = 0;
    subpass_description.pInputAttachments = nullptr;
    subpass_description.preserveAttachmentCount = 0;
    subpass_description.pPreserveAttachments = nullptr;
    subpass_description.pResolveAttachments = nullptr;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_NONE_KHR;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments = &colour_attachment;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass_description;
    render_pass_create_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
    render_pass_create_info.pDependencies = dependencies.data();

    if (vkCreateRenderPass(display.device.logical, &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass.");
}

void RayTraceDisplayer::create_framebuffers()
{
    framebuffers.resize(display.swap_chain.images.size());
    for (size_t i = 0; i < display.swap_chain.images.size(); i++) {

        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = render_pass;
        framebuffer_create_info.attachmentCount = 1;
        framebuffer_create_info.pAttachments = &display.swap_chain.image_views[i];
        framebuffer_create_info.width = display.swap_chain.extent.width;
        framebuffer_create_info.height = display.swap_chain.extent.height;
        framebuffer_create_info.layers = 1;

        if (vkCreateFramebuffer(display.device.logical, &framebuffer_create_info, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer.");
    }
}

void RayTraceDisplayer::destroy_framebuffers()
{
    for (auto framebuffer : framebuffers)
        vkDestroyFramebuffer(display.device.logical, framebuffer, nullptr);
}

void RayTraceDisplayer::create_image_semaphore()
{
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(display.device.logical, &semaphore_create_info, nullptr, &image_semaphore) != VK_SUCCESS)
        throw std::runtime_error("Failed to create semaphore.");
}