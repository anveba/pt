#include "pathtrace.hlsli"
#include "util.hlsli"

cbuffer ubo : register(b2) { UniformBufferObject ubo; };

[shader("miss")]
void main(inout RayPayload payload)
{
    add_radiance(payload, ubo.environment_colour.rgb);
    no_scatter(payload);
}
