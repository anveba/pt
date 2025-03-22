#include "windowapp.h"

#include "fps.h"
#include "pathtrace/ptdisplayer.h"
#include "postprocess/postprocess.h"
#include "rasterise/rasterisedisplayer.h"

static std::vector<VkDescriptorPoolSize> get_descriptor_pool_sizes()
{
    std::vector<VkDescriptorPoolSize> pool_sizes;

    std::vector<VkDescriptorPoolSize> extra = Rasteriser::get_descriptor_pool_sizes();
    pool_sizes.insert(pool_sizes.end(), extra.begin(), extra.end());

    extra = UserInterface::get_descriptor_pool_sizes();
    pool_sizes.insert(pool_sizes.end(), extra.begin(), extra.end());

    extra = PathTracer::get_descriptor_pool_sizes();
    pool_sizes.insert(pool_sizes.end(), extra.begin(), extra.end());

    extra = PostProcessor::get_descriptor_pool_sizes();
    pool_sizes.insert(pool_sizes.end(), extra.begin(), extra.end());

    return pool_sizes;
}

WindowedApplication::WindowedApplication(uint32_t width, uint32_t height, const std::vector<const char*>& validation_layers)
    : context(ContextUsage(CONTEXT_USAGE_WINDOW_BIT), validation_layers)
    , window(context, width, height)
    , device(context, DeviceUsage(DEVICE_USAGE_WINDOW_BIT | DEVICE_USAGE_RAY_TRACE_BIT), &window)
    , descriptor_pool(device, get_descriptor_pool_sizes())
    , command_pool(device)

{
    UserInterface::init(window);

    control_panel = { .render_type = RENDER_TYPE_RASTERISE,
                      .max_bounces = 2,
                      .samples_per_frame = 1 };
}

WindowedApplication::~WindowedApplication()
{
    UserInterface::destroy();
}

static void render_loop(IDisplayable** displayers, Camera& camera, Display& display, DescriptorPool& descriptor_pool, UiControlPanel& control_panel)
{
    Window& window = display.get_window();
    IDisplayable* current_displayer = displayers[control_panel.render_type];

    UserInterface::init_vulkan(descriptor_pool, *current_displayer);
    UserInterface ui(control_panel);

    FpsCounter fps_counter(0.2f);
    fps_counter.restart();
    CameraInput camera_input;
    UiInfo ui_info;
    WindowEventInfo window_event_info = {};

    auto last_frame = std::chrono::high_resolution_clock::now();
    uint32_t width = display.get_extent().width, height = display.get_extent().height;

    while (true) {
        auto this_frame = std::chrono::high_resolution_clock::now();
        float delta_time = std::chrono::duration<float>(this_frame - last_frame).count();
        last_frame = this_frame;

        window.process_events(window_event_info, &camera_input);

        if (window_event_info.exit)
            break;

        bool update_camera = false;

        if (control_panel.render_type != current_displayer->render_type()) {
            current_displayer->wait_idle();
            current_displayer = displayers[control_panel.render_type];
            UserInterface::destroy_vulkan();
            UserInterface::init_vulkan(descriptor_pool, *current_displayer);
            current_displayer->set_extent(display.get_extent().width, display.get_extent().height);
            update_camera = true;
        }

        if (width != display.get_extent().width || height != display.get_extent().height) {
            width = display.get_extent().width; 
            height = display.get_extent().height;
            camera.aspect_ratio = float(width) / height;
            update_camera = true;
        }

        bool camera_rotated = camera_input.rotate(camera, 1.0f * delta_time);
        bool camera_moved = camera_input.move(camera, 4.0f * delta_time);
        if (camera_rotated || camera_moved || update_camera)
            current_displayer->set_camera(camera);

        current_displayer->set_settings(control_panel);

        current_displayer->begin_render();

        fps_counter.add_frame();

        ui_info.fps = fps_counter.frames_per_second();
        ui_info.cam_position = camera.position;
        ui_info.look_dir = glm::normalize(camera.rotation * Vec3(0.0f, 0.0f, 1.0f));
        ui_info.near = camera.near;
        ui_info.far = camera.far;
        current_displayer->get_debug_info(ui_info.render_info);
        ui.new_frame(ui_info);
        ui.render();

        current_displayer->end_render();
    }
    display.get_device().wait_idle();
    UserInterface::destroy_vulkan();
}

void WindowedApplication::begin(const Scene& scene, Camera& camera)
{
    SwapChainSupport swap_chain_support;
    device.query_swap_chain_support(swap_chain_support, window);
    VkSurfaceFormatKHR surface_format = choose_surface_format(swap_chain_support.formats);
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // choose_present_mode(swap_chain_support.present_modes);
    Display display(device, window, surface_format, present_mode);
    camera.aspect_ratio = float(display.get_extent().width) / display.get_extent().height;

    RasteriseDisplayer rasterise_displayer(display, descriptor_pool, command_pool, scene, display.get_extent(), surface_format.format, choose_depth_format(device));
    rasterise_displayer.set_camera(camera);

    PathTraceDisplayer path_trace_displayer(display, descriptor_pool, command_pool, scene, display.get_extent());
    path_trace_displayer.set_camera(camera);

    IDisplayable* displayers[MAX_RENDER_TYPE]{};
    displayers[RENDER_TYPE_PATH_TRACE] = &path_trace_displayer;
    displayers[RENDER_TYPE_RASTERISE] = &rasterise_displayer;

    render_loop(displayers, camera, display, descriptor_pool, control_panel);
}