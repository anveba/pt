#include "constants.h"
#include "queue.hlsli"
#include "pathtrace.hlsli"

RWTexture2D<float4> image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };
RWStructuredBuffer<uint> queues : register(u10);

[numthreads(WFRESET_GROUP_SIZE, WFRESET_GROUP_SIZE, 1)] 
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    int2 pixel = dispatch_id.xy;

    if (pixel.x >= ubo.width || pixel.y >= ubo.height)
        return;

    if (ubo.sample_index == 0) {
        image[pixel] = float4(0.0, 0.0, 0.0, 1.0);
    }

    if (dispatch_id.x == 0 && dispatch_id.y == 0 && dispatch_id.z == 0) {
        init_queues(queues, ubo.queue_capacity, ubo.width * ubo.height);
    }

    const uint src_queue = 0;
    uint item_index = pixel.x + pixel.y * ubo.width;

    queues[QUEUE_FIELD(ubo.queue_capacity, src_queue, item_index, QUEUE_ITEM_SEGMENT_AND_SAMPLE_INDEX_OFFSET)] = 0;

    uint pixel_compressed = (pixel.x << 16) | pixel.y;
    queues[QUEUE_FIELD(ubo.queue_capacity, src_queue, item_index, QUEUE_ITEM_PIXEL_OFFSET)] = pixel_compressed;

    queue_set_float3(queues, ubo.queue_capacity, src_queue, item_index, QUEUE_ITEM_RADIANCE_OFFSET, float3(0.0, 0.0, 0.0));
}