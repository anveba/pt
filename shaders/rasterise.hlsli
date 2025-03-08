#ifndef RASTERISE_HLSLI_INCLUDED
#define RASTERISE_HLSLI_INCLUDED

struct UniformBufferObject
{
    float4x4 mvp;
    float3 view_pos;
    float3 inv_light_dir_norm;
    float3x3 normal;
};

cbuffer ubo : register(b0, space0) { UniformBufferObject ubo; }

struct vs_in {
    [[vk::location(0)]] float3 position : LOCATION0;
    [[vk::location(1)]] float3 normal : LOCATION1;
    [[vk::location(2)]] float2 uv : LOCATION2;

    [[vk::location(3)]] float4 transform_col0 : LOCATION3;
    [[vk::location(4)]] float4 transform_col1 : LOCATION4;
    [[vk::location(5)]] float4 transform_col2 : LOCATION5;
    [[vk::location(6)]] float4 transform_col3 : LOCATION6;

    [[vk::location(7)]] float3 normal_col0 : LOCATION7;
    [[vk::location(8)]] float3 normal_col1 : LOCATION8;
    [[vk::location(9)]] float3 normal_col2 : LOCATION9;
};

struct vs_out {
    float4 position : SV_POSITION; // required output of VS
    float3 world_position : WORLD_POS;
    float3 normal : NORMAL;
    float2 uv : UV;
};

#endif