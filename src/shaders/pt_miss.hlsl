struct RayPayload
{
    [[vk::location(0)]] float3 incoming_colour;
};

[shader("miss")]
void main(inout RayPayload p)
{
    float t = (dot(normalize(WorldRayDirection()), float3(0.0, 1.0, 0.0)) + 1.0) * 0.5;
    p.incoming_colour = t * float3(0.5, 0.8, 0.95) + (1 - t) * float3(0.1, 0.05, 0.0);
}