#include "display/windowapp.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scenebuild.h"
#include <SDL3/SDL.h>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Expected path to scene." << std::endl;
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Init error : " << SDL_GetError() << std::endl;
        return 1;
    }

    Camera camera(Vec3(0.0f, 0.0f, -10.0f),
                  Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
                  glm::radians(45.0f),
                  1.0f,
                  0.1f,
                  1000.0f);
    Scene scene;
    SceneBuilder builder;
    scene.from_file(std::string(argv[1]), camera);
    // builder.set_material_scene(scene, camera);

    std::vector<const char*> validation_layers;
#ifndef NDEBUG
    validation_layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    bool headless = false;

    if (headless) {

    } else {
        WindowedApplication application(400, 400, validation_layers);
        application.begin(scene, camera);
    }
}