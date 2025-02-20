#ifndef RENDERER_H_INCLUDED
#define RENDERER_H_INCLUDED

class Rasteriser;
class Scene;

class IRenderer
{
    virtual void set_scene(const Scene& scene) = 0;
    virtual void new_frame() = 0;
    virtual void end_frame() = 0;
};

#endif