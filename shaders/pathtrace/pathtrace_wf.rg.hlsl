#include "pathtrace.hlsli"
#include "util.hlsli"
#include "sample.hlsli"
#include "queue.hlsli"

RaytracingAccelerationStructure bvh : register(b0);
RWTexture2D<float4> dest_image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };
RWStructuredBuffer<uint> queues : register(u10);

[shader("raygeneration")]
void main()
{
    RayDesc ray_desc;
    RayPayload payload = create_ray_payload();

    uint2 pixel;
    uint segment, sample_index;
    float3 radiance;

    {
        const uint src_queue = get_src_queue(queues);
        const uint id = DispatchRaysIndex().x;

        // TODO: This is incorrect, but for some reason, the queue swapper does not reset the queue...
        if (id == 0)
            reset_queue(queues, ubo.queue_capacity, get_dst_queue(queues));
        
        if (id < queue_item_count(queues, ubo.queue_capacity, src_queue)) {

            uint segment_and_sample_index = queues[QUEUE_FIELD(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_SEGMENT_AND_SAMPLE_INDEX_OFFSET)];
            segment = segment_and_sample_index >> 16;
            sample_index = ubo.sample_index + (0x0000FFFF & segment_and_sample_index);

            uint pixel_compressed = queues[QUEUE_FIELD(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_PIXEL_OFFSET)];
            pixel = uint2(pixel_compressed >> 16, pixel_compressed & 0x0000FFFF);

            sampler_start_new(payload.sampler, pixel, sample_index, segment);
            radiance = queue_get_float3(queues, ubo.queue_capacity, src_queue, id, QUEUE_ITEM_RADIANCE_OFFSET);

            if (segment == 0) {

                sample_camera_ray(sample_2d(payload.sampler), sample_2d(payload.sampler), 
                    pixel, uint2(ubo.width, ubo.height), 
                    ubo.inv_view, ubo.inv_proj,
                    ubo.lens_radius, ubo.focus_dist,
                    ray_desc.Origin, ray_desc.Direction);

                ray_desc.TMin = ubo.near;
                ray_desc.TMax = ubo.far;

                set_new_path(payload);

            } else {
                set_throughput(payload, queue_get_float3(queues, ubo.queue_capacity, src_queue, id, QUEUE_ITEM_THROUGHPUT_OFFSET));
                set_brdf_pdf(payload, asfloat(queues[QUEUE_FIELD(ubo.queue_capacity, src_queue, id, QUEUE_ITEM_BRDF_PDF_OFFSET)]));

                ray_desc.Origin = queue_get_float3(queues, ubo.queue_capacity, src_queue, id, QUEUE_ITEM_ORIGIN_OFFSET);
                ray_desc.Direction = queue_get_float3(queues, ubo.queue_capacity, src_queue, id, QUEUE_ITEM_DIRECTION_OFFSET);
                ray_desc.TMin = 0;
                ray_desc.TMax = INFINITY;
            }
        } else {
            return;
        }
    }
   
    set_new_segment(payload, ubo.max_bounces <= segment);

    TraceRay(bvh, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, ray_desc, payload);

    radiance += get_radiance(payload);

    bool path_is_terminated = is_final_segment(payload) || (segment > 0 && russian_roulette(payload));

    if (path_is_terminated && ((sample_index + 1) >= (ubo.sample_index + ubo.samples))) {

        radiance = correct_radiance(radiance);
        float3 prev = dest_image[pixel].rgb;
        dest_image[pixel].rgb = prev + (radiance - ubo.samples * prev) / (ubo.sample_index + ubo.samples);

    } else {
        const uint dst_queue = get_dst_queue(queues);
        uint item_index = queue_push(queues, ubo.queue_capacity, dst_queue);

        uint pixel_compressed = (pixel.x << 16) | pixel.y;
        queues[QUEUE_FIELD(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_PIXEL_OFFSET)] = pixel_compressed;

        uint next_segment = path_is_terminated ? 0 : (segment + 1);
        uint next_index = path_is_terminated ? (sample_index + 1) : sample_index;
        uint segment_and_sample_index = (next_segment << 16) | (next_index - ubo.sample_index);
        queues[QUEUE_FIELD(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_SEGMENT_AND_SAMPLE_INDEX_OFFSET)] = segment_and_sample_index;

        queue_set_float3(queues, ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_RADIANCE_OFFSET, radiance);

        if (!path_is_terminated) {
            queue_set_float3(queues, ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_THROUGHPUT_OFFSET, get_throughput(payload));
            queues[QUEUE_FIELD(ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_BRDF_PDF_OFFSET)] = asuint(get_brdf_pdf(payload));

            float3 new_origin, new_direction;
            get_next_ray(payload, new_origin, new_direction);
            queue_set_float3(queues, ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_ORIGIN_OFFSET, new_origin);
            queue_set_float3(queues, ubo.queue_capacity, dst_queue, item_index, QUEUE_ITEM_DIRECTION_OFFSET, new_direction);
        }
    }
}