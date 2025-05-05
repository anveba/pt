#include "colour.h"

float to_srgb(float x) {
    return x <= 0.00031308 ? 12.92 * x : 1.055 * pow(x, (1.0 / 2.4) ) - 0.055;
}

Vec3 to_srgb(Vec3 x) {
    return Vec3(to_srgb(x.x), to_srgb(x.y), to_srgb(x.z));
}

float from_srgb(float x) {
    return (x <= 0.04045) ? (x / 12.92) : pow((x + 0.055) / 1.055, 2.4);
}

Vec3 from_srgb(Vec3 x) {
    return Vec3(from_srgb(x.x), from_srgb(x.y), from_srgb(x.z));
}