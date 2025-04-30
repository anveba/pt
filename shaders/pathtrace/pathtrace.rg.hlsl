#include "pathtrace.hlsli"
#include "util.hlsli"
#include "sample.hlsli"

RaytracingAccelerationStructure bvh : register(b0);
RWTexture2D<float4> dest_image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };

[shader("raygeneration")]
void main()
{
    const uint3 id = DispatchRaysIndex();

    RayPayload payload = create_ray_payload();

    for (uint i = 0; i < ubo.samples; i++) {

        sampler_start_new(payload.sampler, id.xy, ubo.sample_index + i);

        RayDesc ray_desc;
        ray_desc.TMin = ubo.near;
        ray_desc.TMax = ubo.far;

        sample_camera_ray(float4(sample_2d(payload.sampler), sample_2d(payload.sampler)), 
            id.xy, uint2(ubo.width, ubo.height), 
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
                float u = sample_1d(payload.sampler);
                if (max_throughput < 1.0) {
                    float q = max(0.0, 1.0 - max_throughput);
                    if (u < q)
                        break;
                    accumulate_throughput(payload, 1.0 / (1.0 - q));
                }
            }
        }
    }

    float3 radiance = get_radiance(payload) / ubo.samples;
    float3 prev = dest_image[id.xy].rgb;
    dest_image[id.xy].rgb = prev + (radiance - prev) / (ubo.sample_index + 1);
}