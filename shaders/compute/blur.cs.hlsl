#include "constants.h"

RWTexture2D<float4> source_image : register(u0);
RWTexture2D<float4> result_image : register(u1);

struct UniformBufferObject {
    uint width;
    uint height;
    uint blur_size;
    uint blur_intensity;
    uint vertical_pass;
};

#define MAX_BLUR_SIZE (128)

cbuffer ubo : register(b2) { UniformBufferObject ubo; };

[numthreads(BLUR_GROUP_SIZE, BLUR_GROUP_SIZE, 1)] 
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    int2 pixel = dispatch_id.xy;

    if (pixel.x >= ubo.width || pixel.y >= ubo.height)
        return;

    float weights[(MAX_BLUR_SIZE * 2 + 1) * (MAX_BLUR_SIZE * 2 + 1)];
    float total_weight = 0.0;

    for (uint i = -ubo.blur_size; i <= ubo.blur_size; i++)
    {
        weights[i + ubo.blur_size] = exp(-0.5 * (i * i)) / sqrt(2.0 * 3.14159);
        total_weight += weights[i + ubo.blur_size];
    }

    for (uint i = 0; i < ubo.blur_size * 2 + 1; i++)
    {
        weights[i] /= total_weight;
    }

    for (uint i = -ubo.blur_size; i <= ubo.blur_size; i++)
    {
    }
}