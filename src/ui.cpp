#include "ui.h"
#include "dispatch.h"
#include "rasteriser.h"
#include "window.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    fprintf(stderr, "User interface error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

UserInterface::UserInterface(Window& window, Rasteriser& rasteriser)
    : rasteriser(&rasteriser)
{
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForVulkan(window.handle);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = rasteriser.dispatcher->instance;
    init_info.PhysicalDevice = rasteriser.dispatcher->physical_device.handle;
    init_info.Device = rasteriser.dispatcher->device;
    init_info.QueueFamily = rasteriser.dispatcher->physical_device.present_family_idx.value();
    init_info.Queue = rasteriser.dispatcher->present_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = rasteriser.descriptor_pool;
    init_info.RenderPass = rasteriser.render_pass;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);
}
UserInterface::~UserInterface()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void UserInterface::new_frame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();
}

void UserInterface::render()
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), rasteriser->command_buffer);
}