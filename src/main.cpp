#include "dispatch.h"
#include "io/ioutil.h"
#include "io/obj_format.h"
#include "rasteriser.h"
#include "ui.h"
#include "window.h"
#include <iostream>

int main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Init error : " << SDL_GetError() << std::endl;
        return 1;
    }

    Scene scene;
    scene.get_meshes().push_back(read_obj(str_from_file("data/bunny.obj")));

    Window window(800, 800);
    Dispatcher dispatcher(&window);
    Shader vs(dispatcher, "bin/hello_vertex.spirv");
    Shader ps(dispatcher, "bin/hello_pixel.spirv");
    Rasteriser rasteriser(vs, ps, dispatcher);
    UserInterface ui(window, rasteriser);

    rasteriser.set_scene(scene);

    while (true) {
        window.process_events();
        ui.new_frame();
        rasteriser.new_frame();
        ui.render();
        rasteriser.end_frame();
    }
}