#include "pathtrace.hlsli"
#include "util.hlsli"
#include "sample.hlsli"
#include "queue.hlsli"

RaytracingAccelerationStructure bvh : register(b0);
RWTexture2D<float4> dest_image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };
RWStructuredBuffer<uint> queues : register(u10);

inline float3 queue_get_float3(uint queue_capacity, uint queue, uint item, uint field) {
    return float3(asfloat(queues[QUEUE_FIELD(queue_capacity, queue, item, field + 0)]),
        asfloat(queues[QUEUE_FIELD(queue_capacity, queue, item, field + 1)]),
        asfloat(queues[QUEUE_FIELD(queue_capacity, queue, item, field + 2)]));
}

inline void queue_set_float3(uint queue_capacity, uint queue, uint item, uint field, float3 value) {
    queues[QUEUE_FIELD(queue_capacity, queue, item, field + 0)] = asuint(value.x);
    queues[QUEUE_FIELD(queue_capacity, queue, item, field + 1)] = asuint(value.y);
    queues[QUEUE_FIELD(queue_capacity, queue, item, field + 2)] = asuint(value.z);
}

inline void add_sample(uint2 pixel, float3 radiance) {
    float3 prev = dest_image[pixel].rgb;
    dest_image[pixel].rgb = prev + (radiance - prev) / (ubo.sample_index + 1);
}

[shader("raygeneration")]
void main()
{
    const uint id = DispatchRaysIndex().x;

    RayDesc ray_desc;
    RayPayload payload = create_ray_payload();

    uint2 pixel;
    uint segment;
    float3 radiance;

    {
        const uint src_queue = get_src_queue(queues);
        
        if (id >= queue_item_count(queues, ubo.queue_capacity, src_queue)) {

            if (get_pixel_index(queues) >= ubo.width * ubo.height)
                return;
            uint pixel_index = next_pixel_index(queues);
            if (pixel_index >= ubo.width * ubo.height)
                return;

            pixel = uint2(pixel_index % ubo.width, pixel_index / ubo.width);
            segment = 0;
            radiance = 0.0;

            sampler_start_new(payload.sampler, pixel, ubo.sample_index, segment);

            sample_camera_ray(sample_2d(payload.sampler), sample_2d(payload.sampler), 
                pixel, uint2(ubo.width, ubo.height), 
                ubo.inv_view, ubo.inv_proj,
                ubo.lens_radius, ubo.focus_dist,
                ray_desc.Origin, ray_desc.Direction);

            ray_desc.TMin = ubo.near;
            ray_desc.TMax = ubo.far;

            set_new_path(payload);

        } else {
            pixel = uint2(queues[QUEUE_FIELD(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_X_OFFSET)], 
                queues[QUEUE_FIELD(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_Y_OFFSET)]);
            segment = queues[QUEUE_FIELD(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_PATH_SEGMENT_OFFSET)];
            radiance = queue_get_float3(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_RADIANCE_OFFSET);

            sampler_start_new(payload.sampler, pixel, ubo.sample_index, segment);

            set_throughput(payload, queue_get_float3(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_THROUGHPUT_OFFSET));
            set_brdf_pdf(payload, asfloat(queues[QUEUE_FIELD(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_BRDF_PDF_OFFSET)]));

            ray_desc.Origin = queue_get_float3(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_ORIGIN_OFFSET);
            ray_desc.Direction = queue_get_float3(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_DIRECTION_OFFSET);
            ray_desc.TMin = 0;
            ray_desc.TMax = INFINITY;
        }
    }
   
    set_new_segment(payload, ubo.max_bounces <= segment);

    TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);

    float3 throughput = get_throughput(payload);
    radiance += get_radiance(payload);

    // Check if ray hit. If so, we continue the path by pushing to the queue.
    if (segment < ubo.max_bounces && !is_final_segment(payload) && !russian_roulette(payload)) {

        const uint dst_queue = get_dst_queue(queues);
        uint item_index = queue_push(queues, ubo.queue_capacity, dst_queue);

        queues[QUEUE_FIELD(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_X_OFFSET)] = pixel.x;
        queues[QUEUE_FIELD(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_Y_OFFSET)] = pixel.y;
        queue_set_float3(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_RADIANCE_OFFSET, radiance);
        queue_set_float3(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_THROUGHPUT_OFFSET, throughput);
        queues[QUEUE_FIELD(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_BRDF_PDF_OFFSET)] = asuint(get_brdf_pdf(payload));
        queues[QUEUE_FIELD(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_PATH_SEGMENT_OFFSET)] = segment + 1;

        float3 new_origin, new_direction;
        get_next_ray(payload, new_origin, new_direction);
        queue_set_float3(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_ORIGIN_OFFSET, new_origin);
        queue_set_float3(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_DIRECTION_OFFSET, new_direction);

    } else {
        add_sample(pixel, radiance);
    }
}