#include "pathtrace.hlsli"

[shader("miss")]
void main(inout RayPayload p)
{
    const float infinity = 1.0 / 0.0;
    float t = (dot(normalize(WorldRayDirection()), float3(0.0, 1.0, 0.0)) + 1.0) * 0.5;
    p.incoming_colour = float4(t * float3(0.5, 0.8, 0.95) + (1 - t) * float3(0.2, 0.05, 0.35), infinity);
}
