struct UniformBufferObject
{
    float4x4 inv_view;
    float4x4 inv_proj;
    float near;
    float far;
    float old_samples_mult;
    float new_samples_mult;
    uint4 seed;
    uint samples;
    uint max_bounces;
};

struct ObjectData {
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
    [[vk::location(0)]] float4 incoming_colour; // w component is distance
    [[vk::location(1)]] uint4 rng_state;
    [[vk::location(2)]] float3 next_direction;
};

RaytracingAccelerationStructure bvh : register(t0);
RWTexture2D<float4> dest_image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };
StructuredBuffer<float4> vertex_buffer : register(t3);
StructuredBuffer<uint> index_buffer : register(t4);
StructuredBuffer<ObjectData> object_buffer : register(t5);

ObjectData get_object_data(uint i) {
    return object_buffer[i];
}

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

uint taus_step(inout uint z, int s1, int s2, int s3, uint m)
{
    uint b = (((z << s1) ^ z) >> s2);
    z = ((z & m) << s3) ^ b;
    return z;
}

uint lcg_step(inout uint z, uint a, uint c)
{
    z = a * z + c;
    return z;
}

float hybrid_taus(inout uint4 state)
{
    return 2.3283064365387e-10 * (            
        taus_step(state.x, 13, 19, 12, 4294967294UL) ^  
        taus_step(state.y, 2, 25, 4, 4294967288UL) ^   
        taus_step(state.z, 3, 11, 17, 4294967280UL) ^  
        lcg_step(state.w, 1664525, 1013904223UL)      
    );
}

//TODO look at different RNGs
// uint rand_xorshift(inout uint rng_state)
// {
//     rng_state ^= (rng_state << 13);
//     rng_state ^= (rng_state >> 17);
//     rng_state ^= (rng_state << 5);
//     return rng_state;
// }

// float rand_float(inout uint4 rng_state) {
//     return (float)rand_xorshift(rng_state) / (float)0xFFFFFFFF;
// }

// float rand_float(inout uint rng_state) {
//     return (float)((1977654935 * rng_state) & 0x7FFFFFFF) / (float)0x80000000;
// }

float3 rand_dir(inout uint4 rng_state) {
    //TODO try Box-Muller transform
    const float pi = 3.141592654;
    float theta = hybrid_taus(rng_state) * pi;
    float phi = hybrid_taus(rng_state) * 2.0 * pi;
    return float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
}

uint4 get_seed(uint4 v0, uint4 v1) {
    const uint n = 16;
    uint s0 = 0;
    for  (uint i = 0; i < n; i++) { 
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4); 
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e); 
    } 
    return v0;
}

[shader("closesthit")]
void closesthit_main(inout RayPayload payload, in Attributes attributes)
{
    ObjectData object_data = get_object_data(InstanceID());
    uint3 indices = get_index(object_data.index_index + PrimitiveIndex() * 3);

    Vertex v_a = get_vertex(object_data.vertex_index + indices.x);
    Vertex v_b = get_vertex(object_data.vertex_index + indices.y);
    Vertex v_c = get_vertex(object_data.vertex_index + indices.z);

    float alpha = 1.0f - attributes.barycentric.x - attributes.barycentric.y;
    float beta = attributes.barycentric.x;
    float gamma = attributes.barycentric.y;

    float dist = RayTCurrent();
    float3 hit_point = WorldRayOrigin() + dist * WorldRayDirection();
    float3 obj_space_normal = normalize(alpha * v_a.normal + beta * v_b.normal + gamma * v_c.normal);
    float3 normal = mul((float3x3)WorldToObject4x3(), obj_space_normal); // TODO investigate performance of WorldToObject4x3()

    payload.next_direction = normalize(rand_dir(payload.rng_state));
    if (dot(normal, payload.next_direction) < 0.0) {
        payload.next_direction = -payload.next_direction;
    }

    payload.incoming_colour = float4(float3(1.0, 1.0, 1.0) * dot(normal, payload.next_direction), dist);
}

[shader("miss")]
void miss_main(inout RayPayload p)
{
    const float infinity = 1.0 / 0.0;
    float t = (dot(normalize(WorldRayDirection()), float3(0.0, 1.0, 0.0)) + 1.0) * 0.5;
    p.incoming_colour = float4(t * float3(0.5, 0.8, 0.95) + (1 - t) * float3(0.2, 0.05, 0.35), infinity);
}

[shader("raygeneration")]
void raygeneration_main()
{
    const uint3 id = DispatchRaysIndex();
    const uint3 size = DispatchRaysDimensions();

    uint dispatch_seed = (id.x << 16) | (id.y & 0x0000FFFF);
    RayPayload payload;
    payload.rng_state = get_seed(uint4(dispatch_seed + 0, dispatch_seed + 1, dispatch_seed + 2, dispatch_seed + 3), ubo.seed);
    
    float3 colour_sum = float3(0.0, 0.0, 0.0);

    for (uint i = 0; i < ubo.samples; i++) {

        float offset_x = hybrid_taus(payload.rng_state);
        float offset_y = hybrid_taus(payload.rng_state);

        const float2 film_position = float2(id.xy) + float2(offset_x, offset_y);
        const float2 norm_coords = (film_position / float2(size.xy)) * 2.0 - 1.0;
        const float4 target = mul(ubo.inv_proj, float4(norm_coords.xy, 1.0, 1.0));

        RayDesc ray_desc;
        ray_desc.Origin = mul(ubo.inv_view, float4(0.0, 0.0, 0.0, 1.0)).xyz;
        ray_desc.Direction = mul(ubo.inv_view, float4(normalize(target.xyz), 0.0)).xyz;
        ray_desc.TMin = ubo.near;
        ray_desc.TMax = ubo.far;
   
        TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);

        float3 ray_colour = payload.incoming_colour.xyz;

        // Check if initial ray hit. If so, we continue tracing rays.
        if (!isinf(payload.incoming_colour.w)) {

            ray_desc.TMin = 0.001;
            ray_desc.TMax = 10000.0;

            uint i;
            for (i = 0; i <= ubo.max_bounces; i++) {

                ray_desc.Origin = ray_desc.Origin + payload.incoming_colour.w * ray_desc.Direction;
                ray_desc.Direction = payload.next_direction;

                TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);

                ray_colour *= payload.incoming_colour.xyz;

                if (isinf(payload.incoming_colour.w))
                    break;
            }

            // TODO Check if ray reaches depth limit
            // if (i > ubo.max_bounces)
            //     ray_colour = float3(0.0, 0.0, 0.0);
        }

        colour_sum += ray_colour;
    }

    dest_image[int2(id.xy)] = ubo.old_samples_mult * dest_image[int2(id.xy)] + ubo.new_samples_mult * float4(colour_sum / ubo.samples, 0.0);
}