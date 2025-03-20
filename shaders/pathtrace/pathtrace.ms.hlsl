#include "pathtrace.hlsli"

[shader("miss")]
void main(inout RayPayload payload)
{
    // float t = (dot(normalize(WorldRayDirection()), float3(0.0, 1.0, 0.0)) + 1.0) * 0.5;
    // float3 sky_colour = t * float3(1.0, 1.4, 2.0) + (1 - t) * float3(0.5, 0.3, 0.3);
    float3 sky_colour = float3(1.0, 1.0, 1.0) * 5.0;
    payload.incoming_direction = float4(0.0, 0.0, 0.0, INFINITY);
    payload.emission = float3(sky_colour);
}
