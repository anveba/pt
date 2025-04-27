#include "ui.h"
#include "window.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

static bool ui_initialied = false;
static IDisplayable* ui_displayer = nullptr;

static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    fprintf(stderr, "User interface error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void UserInterface::init(Window& window)
{
    if (ui_initialied)
        throw std::runtime_error("User interface is already initialised.");

    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForVulkan(window.handle);

    ui_initialied = true;
}

void UserInterface::destroy()
{
    if (!ui_initialied)
        throw std::runtime_error("User interface is not initialised.");

    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    ui_initialied = false;
}

void UserInterface::init_vulkan(DescriptorPool& descriptor_pool, IDisplayable& displayable)
{
    if (ui_displayer != nullptr)
        throw std::runtime_error("Vulkan usage for the user interface is already initialised.");

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = descriptor_pool.get_device().get_context().handle();
    init_info.PhysicalDevice = descriptor_pool.get_device().physical_handle();
    init_info.Device = descriptor_pool.get_device().logical_handle();
    init_info.QueueFamily = descriptor_pool.get_device().get_physical_device_info().graphics_family_idx.value();
    init_info.Queue = descriptor_pool.get_device().get_graphics_queue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool.handle();
    init_info.RenderPass = displayable.get_render_pass();
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = std::max(static_cast<uint32_t>(displayable.max_in_flight()), init_info.MinImageCount);
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = VK_NULL_HANDLE;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    ui_displayer = &displayable;
}

void UserInterface::destroy_vulkan()
{
    if (ui_displayer == nullptr)
        throw std::runtime_error("Vulkan usage for the user interface is not initialised.");

    ui_displayer = nullptr;

    ImGui_ImplVulkan_Shutdown();
}

std::vector<VkDescriptorPoolSize> UserInterface::get_descriptor_pool_sizes()
{
    return { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE } };
}

UserInterface::UserInterface(UiControlPanel& control_panel)
    : control_panel(&control_panel)
{
    if (!ui_initialied || ui_displayer == nullptr)
        throw std::runtime_error("User interface is not initialised.");
}

void UserInterface::new_frame(const UiInfo& info)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Debug Info");
    ImGui::Text("FPS: %.1f", info.fps);
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", info.cam_position.x, info.cam_position.y, info.cam_position.z);
    ImGui::Text("Look direction: (%.2f, %.2f, %.2f)", info.look_dir.x, info.look_dir.y, info.look_dir.z);
    ImGui::Text("Clipping planes: %.2f, %.2f", info.near, info.far);
    if (control_panel->render_type == RENDER_TYPE_PATH_TRACE)
        ImGui::Text("Samples: %d", info.render_info.samples);
    ImGui::End();

    ImGui::Begin("Control Panel");
    ImGui::Combo("Rendering", (int*)&control_panel->render_type, "Path tracing\0Rasterisation\0");
    if (control_panel->render_type == RENDER_TYPE_PATH_TRACE) {
        ImGui::SliderInt("Samples per frame", (int*)&control_panel->samples_per_frame, 1, 64);
        ImGui::SliderInt("Max bounces", (int*)&control_panel->max_bounces, 1, 16);
    }
    ImGui::End();
}

void UserInterface::render()
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ui_displayer->get_command_buffer());
}