#ifndef QUEUE_HLSLI_INCLUDED
#define QUEUE_HLSLI_INCLUDED

#include "constants.h"

#define QUEUE_CURRENT_SOURCE_OFFSET (0)
#define QUEUE_CURRENT_DISPATCH (1)

#define QUEUE_COUNT_OFFSET (0)
#define QUEUE_START_OFFSET (1)

#define QUEUE_ITEM_THROUGHPUT_OFFSET (0)
#define QUEUE_ITEM_BRDF_PDF_OFFSET (3)
#define QUEUE_ITEM_RADIANCE_OFFSET (4)
#define QUEUE_ITEM_PIXEL_OFFSET (7)
#define QUEUE_ITEM_ORIGIN_OFFSET (8)
#define QUEUE_ITEM_DIRECTION_OFFSET (12)

#define USE_SOA

#ifdef USE_SOA
#define QUEUE_FIELD(capacity, queue, item, field) (QUEUE_OFFSET(capacity, queue) + QUEUE_START_OFFSET + (field) * (capacity) + (item))
#else
#define QUEUE_FIELD(capacity, queue, item, field) (QUEUE_OFFSET(capacity, queue) + QUEUE_START_OFFSET + (item) * QUEUE_ITEM_SIZE + (field))
#endif

inline void init_queues(inout RWStructuredBuffer<uint> queues, uint queue_capacity) {
    queues[QUEUE_CURRENT_SOURCE_OFFSET] = 0;
    queues[QUEUE_CURRENT_DISPATCH] = 0;
    queues[QUEUE_OFFSET(queue_capacity, 0) + QUEUE_COUNT_OFFSET] = 0;
    queues[QUEUE_OFFSET(queue_capacity, 1) + QUEUE_COUNT_OFFSET] = 0;
}

inline void swap_queues(inout RWStructuredBuffer<uint> queues) {
    queues[QUEUE_CURRENT_SOURCE_OFFSET] = ((queues[QUEUE_CURRENT_SOURCE_OFFSET] + 1) & 1);
}

inline void reset_queue(inout RWStructuredBuffer<uint> queues, uint queue_capacity, uint queue) {
    queues[QUEUE_OFFSET(queue_capacity, queue) + QUEUE_COUNT_OFFSET] = 0;
}

inline uint get_src_queue(in RWStructuredBuffer<uint> queues) {
    return queues[QUEUE_CURRENT_SOURCE_OFFSET];
}

inline uint get_dst_queue(in RWStructuredBuffer<uint> queues) {
    return ((get_src_queue(queues) + 1) & 1);
}

inline uint get_current_dispatch(in RWStructuredBuffer<uint> queues) {
    return queues[QUEUE_CURRENT_DISPATCH];
}

inline void increment_dispatch(inout RWStructuredBuffer<uint> queues) {
    queues[QUEUE_CURRENT_DISPATCH] += 1;
}

inline uint queue_item_count(in RWStructuredBuffer<uint> queues, uint queue_capacity, uint queue) {
    return queues[QUEUE_OFFSET(queue_capacity, queue) + QUEUE_COUNT_OFFSET];
}

inline uint queue_push(inout RWStructuredBuffer<uint> queues, uint queue_capacity, uint queue) {
    uint top = 0; // The compiler issues a warning if this is not set, even though it shouldn't need to be set.
    uint lane_count = WaveActiveCountBits(true);
    if (WaveIsFirstLane()) {
        InterlockedAdd(queues[QUEUE_OFFSET(queue_capacity, queue) + QUEUE_COUNT_OFFSET], lane_count, top);
    }
    top = WaveReadLaneFirst(top);
    return top + WavePrefixCountBits(true);

    // uint top;
    // InterlockedAdd(queues[QUEUE_OFFSET(queue_capacity, queue) + QUEUE_COUNT_OFFSET], 1, top);
    // return top;
}

inline float3 queue_get_float3(in RWStructuredBuffer<uint> queues, uint queue_capacity, uint queue, uint item, uint field) {
    return float3(asfloat(queues[QUEUE_FIELD(queue_capacity, queue, item, field + 0)]),
        asfloat(queues[QUEUE_FIELD(queue_capacity, queue, item, field + 1)]),
        asfloat(queues[QUEUE_FIELD(queue_capacity, queue, item, field + 2)]));
}

inline void queue_set_float3(inout RWStructuredBuffer<uint> queues, uint queue_capacity, uint queue, uint item, uint field, float3 value) {
    queues[QUEUE_FIELD(queue_capacity, queue, item, field + 0)] = asuint(value.x);
    queues[QUEUE_FIELD(queue_capacity, queue, item, field + 1)] = asuint(value.y);
    queues[QUEUE_FIELD(queue_capacity, queue, item, field + 2)] = asuint(value.z);
}

#endif