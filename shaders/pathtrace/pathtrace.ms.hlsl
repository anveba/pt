#include "pathtrace.hlsli"
#include "util.hlsli"

cbuffer ubo : register(b2) { UniformBufferObject ubo; };

[shader("miss")]
void main(inout RayPayload payload)
{
    payload.throughput = float4(0.0, 0.0, 0.0, INFINITY);
    payload.emission = ubo.environment_colour.rgb;
}
