#ifndef SCENE_LIGHT_H_INCLUDED
#define SCENE_LIGHT_H_INCLUDED

#include "colour.h"
#include "lalgebra.h"

class PointLight
{
  public:
    PointLight(const Vec3& position, const Vec3& colour)
        : pos(position)
        , col(colour)
    {
    }

    Vec3& position() { return pos; }
    const Vec3& position() const { return pos; }
    Vec3& colour() { return col; }
    const Vec3& colour() const { return col; }
    float power() const { return col.r + col.g + col.b; }

  private:
    Vec3 pos;
    Vec3 col;
};

class DirectionalLight
{
  public:
    DirectionalLight(const Vec3& direction, const Vec3& colour)
        : dir(direction)
        , col(colour)
    {
    }

    Vec3& direction() { return dir; }
    const Vec3& direction() const { return dir; }
    Vec3& colour() { return col; }
    const Vec3& colour() const { return col; }
    float power() const { return col.r + col.g + col.b; }

  private:
    Vec3 dir;
    Vec3 col;
};

#endif