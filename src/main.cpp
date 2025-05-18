#include "argparse.h"
#include "display/windowapp.h"
#include "pathtrace/ptdispatch.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scenebuild.h"
#include <SDL3/SDL.h>
#include <iostream>

static void print_help(const std::string& program_name, const CommandLineParser& parser)
{
    std::cout << "Usage: " << program_name << " [options] path-to-scene\n"
              << parser.get_help() << std::endl;
}

int main(int argc, char** argv)
{
    CommandLineParser parser;
    parser.add_option("--width", "-w", OptionType::INTEGER, "The width of the rendered image.");
    parser.add_option("--height", "-h", OptionType::INTEGER, "The height of the rendered image.");
    parser.add_option("--headless", "", OptionType::SWITCH, "Run without window and user interface.");
    parser.add_option("--samples", "-s", OptionType::INTEGER, "Number of samples to take.");
    parser.add_option("--bounces", "-b", OptionType::INTEGER, "Maximum number of ray bounces.");
    parser.add_option("--out", "-o", OptionType::STRING, "Specifies in which file to place the output.");
    parser.add_option("--format", "-f", OptionType::STRING, "The image format of the output file.");
    parser.add_option("--time", "-t", OptionType::DECIMAL, "The maximum rendering time.");

    std::vector<std::string> arguments;
    try {
        parser.parse(argc, argv, arguments);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        print_help(argv[0], parser);
        return 1;
    }

    if (parser.help_requested()) {
        print_help(argv[0], parser);
        return 0;
    }

    if (arguments.size() == 0) {
        std::cerr << "Error: Expected path to scene." << std::endl;
        print_help(argv[0], parser);
        return 1;
    }

    if (arguments.size() > 1) {
        std::cerr << "Error: Unexpected number of arguments." << std::endl;
        print_help(argv[0], parser);
        return 1;
    }

    bool headless = parser.get_arg_as_switch("--headless");
    int samples = parser.get_arg_as_integer("--samples").value_or(32);
    if (samples < 1) {
        std::cerr << "Error: Invalid sample count: " << samples << "." << std::endl;
        return 1;
    }

    int bounces = parser.get_arg_as_integer("--bounces").value_or(2);
    if (bounces < 0) {
        std::cerr << "Error: Invalid number of ray bounces: " << bounces << "." << std::endl;
        return 1;
    }

    std::optional<std::string> out = parser.get_arg_as_string("--out");
    if (headless && !out.has_value()) {
        std::cerr << "Error: No output file was given (required for headless execution)." << std::endl;
        return 1;
    }

    int width = parser.get_arg_as_integer("--width").value_or(400);
    int height = parser.get_arg_as_integer("--height").value_or(400);
    if (width < 1 || height < 1) {
        std::cerr << "Error: Invalid dimensions: (" << std::to_string(width) << ", "
                  << std::to_string(height) << ")." << std::endl;
        return 1;
    }

    OutputImageFormat output_format = image_format_from_string(parser.get_arg_as_string("--format").value_or("png"));
    if (output_format == OutputImageFormat::NONE) {
        std::cerr << "Error: Output format is invalid: " << parser.get_arg_as_string("--format").value() << std::endl;
        return 1;
    }

    float render_time = parser.get_arg_as_decimal("--time").value_or(INFINITY);

    Camera camera(Vec3(0.0f, 0.0f, 0.0f),
                  Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
                  glm::radians(45.0f),
                  1.0f,
                  0.1f,
                  10000.0f);
    Scene scene;
    SceneBuilder builder;
    builder.read_scene_description(scene, camera, arguments[0]);
    camera.aspect_ratio = float(width) / height;

    std::cout << scene.scene_details() << std::endl;

    std::vector<const char*> validation_layers;
#ifndef NDEBUG
    validation_layers.push_back("VK_LAYER_KHRONOS_validation");
    std::cout << "Validation layers enabled." << std::endl;
#endif

    if (headless) {
        PathTraceDispatcher dispatcher(scene, width, height, validation_layers);
        PathTraceParameters params{
            .camera = camera,
            .out_path = out.value(),
            .output_format = output_format,
            .samples = static_cast<uint32_t>(samples),
            .max_bounces = static_cast<uint32_t>(bounces),
            .render_time = render_time
        };
        dispatcher.start(params);
    } else {

        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::cerr << "Init error : " << SDL_GetError() << std::endl;
            return 1;
        }

        WindowedApplication application(width, height, validation_layers);
        application.begin(scene, camera);
    }
}