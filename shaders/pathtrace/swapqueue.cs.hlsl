#include "queue.hlsli"
#include "pathtrace.hlsli"

cbuffer ubo : register(b2) { UniformBufferObject ubo; };
RWStructuredBuffer<uint> queues : register(u10);

[numthreads(1, 1, 1)] 
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    if (dispatch_id.x == 0 && dispatch_id.y == 0 && dispatch_id.z == 0) {
        increment_dispatch(queues);
        swap_queues(queues);
        reset_queue(queues, ubo.queue_capacity, get_dst_queue(queues));
    }
}