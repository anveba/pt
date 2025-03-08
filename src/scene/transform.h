#ifndef SCENE_TRANSFORM_H_INCLUDED
#define SCENE_TRANSFORM_H_INCLUDED

#include "lalgebra.h"

class Transform
{
  public:
    Transform()
    {
    }

    Transform(Mat4 transform)
        : matrix(transform)
    {
    }

    Mat4 matrix;
};

#endif