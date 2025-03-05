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

struct RayPayload
{
    [[vk::location(0)]] float3 incoming_colour;
};

RaytracingAccelerationStructure bvh : register(t0);
RWTexture2D<float4> dest_image : register(u1);

uint mcg31(uint prev) {
    return (1977654935 * prev) & 0x7FFFFFFF;
}

float rand(uint r) {
    return (float)r / (float)0xFFFFFFFF;
}

[shader("raygeneration")]
void main()
{
    const uint3 id = DispatchRaysIndex();
    const uint3 size = DispatchRaysDimensions();

    uint prev = mcg31(((id.x << 16) | (id.y & 0x0000FFFF)) ^ ubo.seed);
    float offset_x = rand(prev);
    prev = mcg31(prev);
    float offset_y = rand(prev);

    const float2 film_position = float2(id.xy) + float2(offset_x, offset_y);
    const float2 norm_coords = (film_position / float2(size.xy)) * 2.0 - 1.0;
    const float4 target = mul(ubo.inv_proj, float4(norm_coords.xy, 1.0, 1.0));

    RayDesc ray_desc;
    ray_desc.Origin = mul(ubo.inv_view, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    ray_desc.Direction = mul(ubo.inv_view, float4(normalize(target.xyz), 0.0)).xyz;
    ray_desc.TMin = ubo.near;
    ray_desc.TMax = ubo.far;

    RayPayload payload;
    TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);

    dest_image[int2(id.xy)] = ubo.old_samples_mult * dest_image[int2(id.xy)] + ubo.new_samples_mult * float4(payload.incoming_colour, 0.0);
}