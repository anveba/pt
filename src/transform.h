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
        : mat(transform)
    {
    }

    Mat4& matrix() { return mat; }
    const Mat4& matrix() const { return mat; }

  private:
    Mat4 mat;
};

#endif