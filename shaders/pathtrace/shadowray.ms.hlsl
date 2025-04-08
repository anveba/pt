#include "pathtrace.hlsli"

[shader("miss")]
void main(inout ShadowRayPayload payload)
{
    payload.is_occluded = false;
}
