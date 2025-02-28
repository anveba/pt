#ifndef TRANSFORM_H_INCLUDED
#define TRANSFORM_H_INCLUDED

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