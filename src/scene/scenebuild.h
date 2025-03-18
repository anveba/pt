#ifndef SCENE_SCENEBUILD_H_INCLUDED
#define SCENE_SCENEBUILD_H_INCLUDED

#include "scene.h"

class SceneBuilder
{
  public:
    SceneBuilder();
    ~SceneBuilder();

    void set_material_scene(Scene& scene, Camera& camera);

  private:
    NO_COPY(SceneBuilder);
};

#endif