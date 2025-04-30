#include "constants.h"
#include "queue.hlsli"
#include "pathtrace.hlsli"

RWTexture2D<float4> image : register(u1);
cbuffer ubo : register(b2) { UniformBufferObject ubo; };
RWStructuredBuffer<uint> queues : register(u10);

[numthreads(PTRESET_GROUP_SIZE, PTRESET_GROUP_SIZE, 1)] 
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    int2 coord = dispatch_id.xy;

    if (coord.x >= ubo.width || coord.y >= ubo.height)
        return;

    if (ubo.sample_index == 0) {
        image[coord] = float4(0.0, 0.0, 0.0, 1.0);
    }

    if (dispatch_id.x == 0 && dispatch_id.y == 0 && dispatch_id.z == 0) {
        init_queues(queues, ubo.queue_capacity);
    }
}