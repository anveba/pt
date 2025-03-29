#include "pathtrace.hlsli"

RaytracingAccelerationStructure bvh : register(t0);
RWTexture2D<float4> dest_image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };

[shader("raygeneration")]
void main()
{
    const uint3 id = DispatchRaysIndex();
    const uint3 size = DispatchRaysDimensions();

    uint dispatch_seed = (id.x << 16) | (id.y & 0x0000FFFF);
    RayPayload payload;
    //TODO set better seeds
    seed(payload.rng, uint4(dispatch_seed + 0, dispatch_seed + 1, dispatch_seed + 2, dispatch_seed + 3), ubo.seed);
    
    float3 radiance_sum = float3(0.0, 0.0, 0.0);

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

        float3 throughput = float3(1.0, 1.0, 1.0);
   
        TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);
        radiance_sum += payload.emission * throughput;
        throughput *= payload.brdf.rgb / payload.brdf.w;

        // Check if initial ray hit. If so, we continue tracing rays.
        if (!isinf(payload.incoming_direction.w)) {

            ray_desc.TMin = 0.001; // TODO set ray origin as an offset of the geometry normal
            ray_desc.TMax = 10000.0;

            uint i;
            for (i = 0; i < ubo.max_bounces; i++) {

                // const float termination_probability = 1.0 / dot(throughput, throughput);
                // if (hybrid_taus(payload.rng_state) < termination_probability)
                //     break;

                ray_desc.Origin = ray_desc.Origin + payload.incoming_direction.w * ray_desc.Direction;
                ray_desc.Direction = payload.incoming_direction.xyz;

                TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);
                radiance_sum += payload.emission * throughput;
                throughput *= payload.brdf.rgb / payload.brdf.w;

                if (isinf(payload.incoming_direction.w))
                    break;
            }
        }
    }
    // This condition is used in order to zero out NaN values that may be present in the image.
    // TODO: do this in a compute shader right after image creation (or with vkCmdClearColorImage?)
    float4 old_value = (ubo.old_samples_mult == 0.0) ? float4(0.0, 0.0, 0.0, 1.0) : ubo.old_samples_mult * dest_image[int2(id.xy)];
    dest_image[int2(id.xy)] = old_value + ubo.new_samples_mult * float4(radiance_sum / ubo.samples, 1.0);
}