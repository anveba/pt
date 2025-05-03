#include "pathtrace.hlsli"
#include "util.hlsli"
#include "sample.hlsli"

RaytracingAccelerationStructure bvh : register(b0);
RWTexture2D<float4> dest_image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };

[shader("raygeneration")]
void main()
{
    const uint2 pixel = DispatchRaysIndex().xy;

    RayPayload payload = create_ray_payload();

    for (uint sample_index = ubo.sample_index; sample_index < ubo.sample_index + ubo.samples; sample_index++) {

        sampler_start_new(payload.sampler, pixel, sample_index, 0);

        RayDesc ray_desc;
        ray_desc.TMin = ubo.near;
        ray_desc.TMax = ubo.far;

        sample_camera_ray(sample_2d(payload.sampler), sample_2d(payload.sampler), 
            pixel, uint2(ubo.width, ubo.height), 
            ubo.inv_view, ubo.inv_proj,
            ubo.lens_radius, ubo.focus_dist,
            ray_desc.Origin, ray_desc.Direction);

        set_new_path(payload);
        set_new_segment(payload, ubo.max_bounces == 0);

        TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);

        // Check if initial ray hit. If so, we continue tracing rays.
        if (!is_final_segment(payload)) {

            ray_desc.TMin = 0;
            ray_desc.TMax = INFINITY;

            for (uint segment = 1; segment <= ubo.max_bounces; segment++) {

                sampler_start_new(payload.sampler, pixel, sample_index, segment);

                get_next_ray(payload, ray_desc.Origin, ray_desc.Direction);

                set_new_segment(payload, segment >= ubo.max_bounces);

                TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);

                if (is_final_segment(payload) || russian_roulette(payload))
                    break;
            }
        }
    }

    float3 radiance = correct_radiance(get_radiance(payload) / ubo.samples);
    float3 prev = dest_image[pixel].rgb;
    dest_image[pixel].rgb = prev + (radiance - prev) / (ubo.sample_index + 1);
}