#include "pathtrace.hlsli"

[shader("miss")]
void main(inout RayPayload payload)
{
    float t = (dot(normalize(WorldRayDirection()), float3(0.0, 1.0, 0.0)) + 1.0) * 0.5;
    float3 sky_colour = t * float3(0.5, 0.7, 1.0) + (1 - t) * float3(0.15, 0.05, 0.1);
    payload.scatter = float4(0.0, 0.0, 0.0, INFINITY);
    payload.emission = float3(sky_colour);
}
