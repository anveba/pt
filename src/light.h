#ifndef LIGHT_H_INCLUDED
#define LIGHT_H_INCLUDED

#include "colour.h"
#include "lalgebra.h"

class PointLight
{
  public:
    PointLight(const Vec3& position, const Colour& colour)
        : pos(position)
        , col(colour)
    {
    }

    Vec3& position() { return pos; }
    const Vec3& position() const { return pos; }
    Colour& colour() { return col; }
    const Colour& colour() const { return col; }

  private:
    Vec3 pos;
    Colour col;
};

#endif