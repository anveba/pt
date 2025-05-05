#ifndef COLOUR_H_INCLUDED
#define COLOUR_H_INCLUDED

#include "lalgebra.h"

float to_srgb(float x);
Vec3 to_srgb(Vec3 x);

float from_srgb(float x);
Vec3 from_srgb(Vec3 x);

#endif