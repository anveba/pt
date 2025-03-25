#include "ptdisplayer.h"

#include "display/ui.h"
#include "pathtrace/pathtrace.h"

PathTraceDisplayer::PathTraceDisplayer(
    Display& display,
    DescriptorPool& descriptor_pool,
    CommandPool& command_pool,
    const Scene& scene,
    VkExtent2D extent)
    : image_semaphore(display.get_device(), false)
    , render_semaphore(display.get_device(), false)
    , render_fence(display.get_device(), true)
    , intermediate_image(display.get_device(), command_pool, extent, VK_FORMAT_R32G32B32A32_SFLOAT, VkImageUsageFlagBits(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT), VK_IMAGE_LAYOUT_GENERAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    , display(display)
    , command_pool(command_pool)
    , path_tracer(display.get_device(), descriptor_pool, command_pool, scene, extent)
    , tone_mapper(display.get_device(), descriptor_pool, command_pool, extent, path_tracer.get_accumulation_image().get_view(), intermediate_image.get_view())
    , in_render(false)
{
    create_render_pass();
    create_framebuffers();
    command_buffer = command_pool.create_command_buffer();
}

PathTraceDisplayer::~PathTraceDisplayer()
{
    command_pool.destroy_command_buffer(command_buffer);
    destroy_framebuffers();
    vkDestroyRenderPass(display.get_device().logical_handle(), render_pass, nullptr);
}

void PathTraceDisplayer::set_extent(uint32_t width, uint32_t height)
{
    path_tracer.set_extent(command_pool, width, height);
    VkExtent2D image_extent = path_tracer.get_accumulation_image().get_extent();
    intermediate_image.rebuild(command_pool, image_extent);
    tone_mapper.set_source_image(path_tracer.get_accumulation_image().get_view());
    tone_mapper.set_result_image(intermediate_image.get_view());
    tone_mapper.set_extent(image_extent);

    destroy_framebuffers();
    create_framebuffers();
}

void PathTraceDisplayer::set_scene(const Scene& scene)
{
    path_tracer.set_scene(command_pool, scene);
}

void PathTraceDisplayer::set_camera(const Camera& camera)
{
    path_tracer.set_camera(command_pool, camera);
}

void PathTraceDisplayer::wait_idle()
{
    render_fence.wait();
}

void PathTraceDisplayer::begin_render()
{
    if (in_render)
        throw std::runtime_error("Ray tracer is already rendering.");

    render_fence.wait();

    path_tracer.update_uniforms();

    bool swap_chain_recreated;
    uint32_t index = display.acquire_next_index(image_semaphore, swap_chain_recreated);
    if (swap_chain_recreated)
        set_extent(display.get_extent().width, display.get_extent().height);

    vkResetCommandBuffer(command_buffer, 0);

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.flags = 0;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(command_buffer, &cmd_buffer_begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin command buffer.");

    // TODO avoid writing each frame (implement together with frames in flight)
    path_tracer.write_command_buffer(command_buffer);

    tone_mapper.write_command_buffer(command_buffer);

    blit_result(display.get_swap_chain().images[index], display.get_swap_chain().extent.width, display.get_swap_chain().extent.height);

    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass;
    render_pass_begin_info.framebuffer = framebuffers[index];
    render_pass_begin_info.renderArea.offset = { 0, 0 };
    render_pass_begin_info.renderArea.extent = intermediate_image.get_extent();

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    in_render = true;
}

void PathTraceDisplayer::end_render()
{
    vkCmdEndRenderPass(command_buffer);

    if (!in_render)
        throw std::runtime_error("Render ended before having begun.");

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to write command buffer.");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_semaphore.handle();
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkSemaphore signal_semaphores[] = { render_semaphore.handle() };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    render_fence.reset();

    if (vkQueueSubmit(display.get_device().get_graphics_queue(), 1, &submit_info, render_fence.handle()) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit to queue.");

    in_render = false;

    display.present(render_semaphore);
}

void PathTraceDisplayer::get_debug_info(RenderDebugInfo& info)
{
    info = {};
    info.samples = path_tracer.accumulated_samples();
}

void PathTraceDisplayer::set_settings(const UiControlPanel& control_panel)
{
    if (path_tracer.get_max_bounces() != control_panel.max_bounces)
        path_tracer.set_max_bounces(control_panel.max_bounces);
    if (path_tracer.get_samples_per_render() != control_panel.samples_per_frame)
        path_tracer.set_samples(control_panel.samples_per_frame);
}

VkRenderPass PathTraceDisplayer::get_render_pass()
{
    return render_pass;
}

VkCommandBuffer PathTraceDisplayer::get_command_buffer()
{
    return command_buffer;
}

void PathTraceDisplayer::create_render_pass()
{
    VkAttachmentDescription colour_attachment{};
    colour_attachment.format = display.get_surface_format().format;
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

    if (vkCreateRenderPass(display.get_device().logical_handle(), &render_pass_create_info, nullptr, &render_pass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass.");
}

void PathTraceDisplayer::create_framebuffers()
{
    framebuffers.resize(display.get_swap_chain().images.size());
    for (size_t i = 0; i < display.get_swap_chain().images.size(); i++) {

        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = render_pass;
        framebuffer_create_info.attachmentCount = 1;
        framebuffer_create_info.pAttachments = &display.get_swap_chain().image_views[i];
        framebuffer_create_info.width = display.get_swap_chain().extent.width;
        framebuffer_create_info.height = display.get_swap_chain().extent.height;
        framebuffer_create_info.layers = 1;

        if (vkCreateFramebuffer(display.get_device().logical_handle(), &framebuffer_create_info, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer.");
    }
}

void PathTraceDisplayer::destroy_framebuffers()
{
    for (auto framebuffer : framebuffers)
        vkDestroyFramebuffer(display.get_device().logical_handle(), framebuffer, nullptr);
}

void PathTraceDisplayer::blit_result(VkImage image, uint32_t width, uint32_t height)
{
    VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    transition_image_layout(command_buffer,
                            image,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            0,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            subresource_range);

    transition_image_layout(command_buffer,
                            intermediate_image.handle(),
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            0,
                            VK_ACCESS_TRANSFER_READ_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            subresource_range);

    VkImageBlit blit{};
    blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.srcOffsets[0] = { 0, 0, 0 };
    blit.srcOffsets[1] = {
        static_cast<int32_t>(intermediate_image.get_extent().width),
        static_cast<int32_t>(intermediate_image.get_extent().height),
        1
    };
    blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.dstOffsets[0] = { 0, 0, 0 };
    blit.dstOffsets[1] = { static_cast<int32_t>(width), static_cast<int32_t>(height), 1 };
    vkCmdBlitImage(command_buffer, intermediate_image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

    transition_image_layout(command_buffer,
                            image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            0,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            subresource_range);

    transition_image_layout(command_buffer,
                            intermediate_image.handle(),
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_ACCESS_TRANSFER_READ_BIT,
                            0,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            subresource_range);
}