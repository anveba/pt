#ifndef SCENE_SCENEBUILD_H_INCLUDED
#define SCENE_SCENEBUILD_H_INCLUDED

#include "scene.h"

class SceneBuilder
{
  public:
    SceneBuilder();
    ~SceneBuilder();

    void set_material_scene(Scene& scene, Camera& camera);
    void read_scene_description(Scene& scene, Camera& camera, const std::string& path);

  private:
    NO_COPY(SceneBuilder);
};

#endif