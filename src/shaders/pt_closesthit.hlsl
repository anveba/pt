struct UniformBufferObject
{
    float4x4 inv_view;
    float4x4 inv_proj;
    float near;
    float far;
    float old_samples_mult;
    float new_samples_mult;
    uint seed;
};

cbuffer ubo : register(b2) { UniformBufferObject ubo; };

struct Attributes
{
    float2 barycentric;
};

struct RayPayload
{
    [[vk::location(0)]] float3 incoming_colour;
};

RaytracingAccelerationStructure bvh : register(t0);

[shader("closesthit")]
void main(inout RayPayload payload, in Attributes attributes)
{
    payload.incoming_colour = float3(1.0, 0.0, 1.0);
}
