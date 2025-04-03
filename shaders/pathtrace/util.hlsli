#ifndef UTIL_HLSLI_INCLUDED
#define UTIL_HLSLI_INCLUDED

#define PI (3.141592654)
#define INFINITY (1.0 / 0.0)

float sq(float x) {
    return x * x;
}

float cos_theta(float3 v) {
    return v.z;
}

float cos2_theta(float3 v) {
    return sq(cos_theta(v));
}

float sin2_theta(float3 v) {
    return max(0.0, 1.0 - cos2_theta(v));
}

float sin_theta(float3 v) {
    return sqrt(sin2_theta(v));
}

float tan_theta(float3 v) {
    return sin_theta(v) / cos_theta(v);
}

float tan2_theta(float3 v) {
    return sin2_theta(v) / cos2_theta(v);
}

// https://graphics.pixar.com/library/OrthonormalB/paper.pdf
void onb(const in float3 n, out float3 x, out float3 y) {
    const float s = n.z < 0 ? -1 : 1;
    const float a = -1.0f / (s + n.z);
    const float b = n.x * n.y * a;
    x = float3(1.0f + s * n.x * n.x * a, s * b, -s * n.x);
    y = float3(b, s + n.y * n.y * a, -n.y);
}

#endif