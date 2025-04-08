#include "pathtrace.hlsli"
#include "util.hlsli"

RaytracingAccelerationStructure bvh : register(t0);
RWTexture2D<float4> dest_image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };

[shader("raygeneration")]
void main()
{
    const uint3 id = DispatchRaysIndex();
    const uint3 size = DispatchRaysDimensions();

    uint id_hash = (id.x << 16) | (id.y & 0x0000FFFF);
    //TODO set better seeds
    uint4 dispatch_seed = uint4(id_hash + 0, id_hash + 1, id_hash + 2, id_hash + 3);

    RayPayload payload = create_ray_payload(create_rng(dispatch_seed, ubo.seed));

    for (uint i = 0; i < ubo.samples; i++) {

        float offset_x = next_float(payload.rng);
        float offset_y = next_float(payload.rng);

        const float2 film_position = float2(id.xy) + float2(offset_x, offset_y);
        const float2 norm_coords = (film_position / float2(size.xy)) * 2.0 - 1.0;
        const float4 target = mul(ubo.inv_proj, float4(norm_coords.xy, 1.0, 1.0));

        RayDesc ray_desc;
        ray_desc.Origin = mul(ubo.inv_view, float4(0.0, 0.0, 0.0, 1.0)).xyz;
        ray_desc.Direction = normalize(mul(ubo.inv_view, float4(target.xyz, 0.0)).xyz);
        ray_desc.TMin = ubo.near;
        ray_desc.TMax = ubo.far;
   
        set_new_path(payload);
        set_new_segment(payload, ubo.max_bounces == 0);

        TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);

        // Check if initial ray hit. If so, we continue tracing rays.
        if (!is_final_segment(payload)) {

            ray_desc.TMin = 0;
            ray_desc.TMax = INFINITY;

            uint i;
            for (i = 0; i < ubo.max_bounces; i++) {

                get_next_ray(payload, ray_desc.Origin, ray_desc.Direction);

                set_new_segment(payload, i + 1 >= ubo.max_bounces);

                TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);

                if (is_final_segment(payload))
                    break;

                // Apply Russian roulette
                const float3 throughput = get_throughput(payload);
                const float max_throughput = max(max(throughput.r, throughput.g), throughput.b);
                if (max_throughput < 1.0) {
                    float q = max(0.0, 1.0 - max_throughput);
                    if (next_float(payload.rng) < q)
                        break;
                    accumulate_throughput(payload, 1.0 / (1.0 - q));
                }
            }
        }
    }
    // This condition is used in order to zero out NaN values that may be present in the image.
    // TODO: do this in a compute shader right after image creation (or with vkCmdClearColorImage?)
    float4 old_value = (ubo.old_samples_mult == 0.0) ? float4(0.0, 0.0, 0.0, 1.0) : ubo.old_samples_mult * dest_image[int2(id.xy)];
    dest_image[int2(id.xy)] = old_value + ubo.new_samples_mult * float4(get_radiance(payload) / ubo.samples, 1.0);
}