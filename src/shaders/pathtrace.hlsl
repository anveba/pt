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

struct InstanceData {
    uint vertex_index;
    uint index_index;
    uint material_index;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
};

struct Attributes
{
    float2 barycentric;
};

struct RayPayload
{
    [[vk::location(0)]] float3 incoming_colour;
    [[vk::location(1)]] uint recursion;
};

RaytracingAccelerationStructure bvh : register(t0);
RWTexture2D<float4> dest_image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };
StructuredBuffer<float4> vertex_buffer : register(t3);
StructuredBuffer<uint> index_buffer : register(t4);
StructuredBuffer<InstanceData> instance_buffer : register(t5);

uint3 get_index(uint i) {
    return uint3(index_buffer[i + 0], index_buffer[i + 1], index_buffer[i + 2]);
}

Vertex get_vertex(uint i) {
    float4 v0 = vertex_buffer[i * 2 + 0];
    float4 v1 = vertex_buffer[i * 2 + 1];

    Vertex v;
    v.position = v0.xyz;
    v.normal = float3(v0.w, v1.xy);
    v.uv = v1.zw;
    return v;
}

//TODO look at different RNGs
uint rand_xorshift(inout uint rng_state)
{
    rng_state ^= (rng_state << 13);
    rng_state ^= (rng_state >> 17);
    rng_state ^= (rng_state << 5);
    return rng_state;
}

float rand_float(inout uint rng_state) {
    return (float)rand_xorshift(rng_state) / (float)0xFFFFFFFF;
}

float3 rand_dir(inout uint rng_state) {
    //TODO try Box-Muller transform
    const float pi = 3.1415;
    float theta = rand_float(rng_state) * pi;
    float phi = rand_float(rng_state) * 2.0 * pi;
    return float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
}

uint wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}


[shader("closesthit")]
void closesthit_main(inout RayPayload payload, in Attributes attributes)
{
    if (payload.recursion >= 4) {
        payload.incoming_colour = float3(0.0, 0.0, 0.0);
        return;
    }

    const uint3 id = DispatchRaysIndex();
    uint rng_state = wang_hash(((id.x << 16) | (id.y & 0x0000FFFF)) ^ ubo.seed + payload.recursion);

    InstanceData instance_data = instance_buffer[InstanceID()];
    uint3 indices = get_index(instance_data.index_index + PrimitiveIndex() * 3);

    Vertex v_a = get_vertex(instance_data.vertex_index + indices.x);
    Vertex v_b = get_vertex(instance_data.vertex_index + indices.y);
    Vertex v_c = get_vertex(instance_data.vertex_index + indices.z);

    float alpha = 1.0f - attributes.barycentric.x - attributes.barycentric.y;
    float beta = attributes.barycentric.x;
    float gamma = attributes.barycentric.y;

    float3 hit_point = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 obj_space_normal = normalize(alpha * v_a.normal + beta * v_b.normal + gamma * v_c.normal);
    float3 normal = mul((float3x3)WorldToObject4x3(), obj_space_normal); // TODO investigate performance of WorldToObject4x3()

    float3 next_dir = normalize(rand_dir(rng_state));
    if (dot(normal, next_dir) < 0.0) {
        next_dir = -next_dir;
    }

    RayDesc ray_desc;
    ray_desc.Origin = hit_point + next_dir * 0.001;
    ray_desc.Direction = next_dir;
    ray_desc.TMin = ubo.near;
    ray_desc.TMax = ubo.far;

    RayPayload next_payload;
    next_payload.recursion = payload.recursion + 1;
    TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, next_payload);

    payload.incoming_colour = next_payload.incoming_colour * dot(normal, next_dir);
}

[shader("miss")]
void miss_main(inout RayPayload p)
{
    float t = (dot(normalize(WorldRayDirection()), float3(0.0, 1.0, 0.0)) + 1.0) * 0.5;
    p.incoming_colour = t * float3(0.5, 0.8, 0.95) + (1 - t) * float3(0.2, 0.05, 0.35);
}

[shader("raygeneration")]
void raygeneration_main()
{
    const uint3 id = DispatchRaysIndex();
    const uint3 size = DispatchRaysDimensions();

    uint rng_state = ((id.x << 16) | (id.y & 0x0000FFFF)) ^ ubo.seed;
    float offset_x = rand_float(rng_state);
    float offset_y = rand_float(rng_state);

    const float2 film_position = float2(id.xy) + float2(offset_x, offset_y);
    const float2 norm_coords = (film_position / float2(size.xy)) * 2.0 - 1.0;
    const float4 target = mul(ubo.inv_proj, float4(norm_coords.xy, 1.0, 1.0));

    RayDesc ray_desc;
    ray_desc.Origin = mul(ubo.inv_view, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    ray_desc.Direction = mul(ubo.inv_view, float4(normalize(target.xyz), 0.0)).xyz;
    ray_desc.TMin = ubo.near;
    ray_desc.TMax = ubo.far;

    RayPayload payload;
    payload.recursion = 1;
    TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);

    dest_image[int2(id.xy)] = ubo.old_samples_mult * dest_image[int2(id.xy)] + ubo.new_samples_mult * float4(payload.incoming_colour, 0.0);
}