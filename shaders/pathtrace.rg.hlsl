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