#include "pathtrace.hlsli"
#include "util.hlsli"
#include "sample.hlsli"

RaytracingAccelerationStructure bvh : register(t0);
RWTexture2D<float4> dest_image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };

[shader("raygeneration")]
void main()
{
    const uint3 id = DispatchRaysIndex();
    const uint3 size = DispatchRaysDimensions();

    RayPayload payload = create_ray_payload();

    for (uint i = 0; i < ubo.samples; i++) {

        sampler_start_new(payload.sampler, id.xy, ubo.sample_index + i);

        float2 pixel_offset = sample_2d(payload.sampler);

        const float2 film_position = id.xy + pixel_offset;
        const float2 norm_coords = (film_position / size.xy) * 2.0 - 1.0;
        const float4 target = mul(ubo.inv_proj, float4(norm_coords, 1.0, 1.0));

        float3 ray_origin = 0.0, ray_dir = target.xyz;
        if (ubo.lens_radius > 0.0) {
            float2 lens_point = ubo.lens_radius * sample_concentric_disk(sample_2d(payload.sampler));
            float t_to_focus = ubo.focus_dist / -ray_dir.z;
            float3 focus_point = ray_origin + t_to_focus * ray_dir;
            ray_origin = float3(lens_point, 0.0);
            ray_dir = focus_point - ray_origin;
        }

        RayDesc ray_desc;
        ray_desc.Origin = mul(ubo.inv_view, float4(ray_origin, 1.0)).xyz;
        ray_desc.Direction = normalize(mul(ubo.inv_view, float4(ray_dir, 0.0)).xyz);
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

    // This condition is used in order to zero out NaN values that may be present in the image.
    // TODO: do this in a compute shader right after image creation (or with vkCmdClearColorImage?)
    float3 radiance = get_radiance(payload) / ubo.samples;
    if (ubo.sample_index == 0) {
        dest_image[int2(id.xy)] = float4(radiance, 1.0);
    } else {
        float3 prev = dest_image[int2(id.xy)].rgb;
        dest_image[int2(id.xy)].rgb = prev + (radiance - prev) / (ubo.sample_index + 1);
    }
}