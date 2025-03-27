
RWTexture2D<float4> source_image : register(u0);
RWTexture2D<float4> result_image : register(u1);

struct UniformBufferObject {
    uint width;
    uint height;
    uint src_is_srgb;
    uint dst_is_srgb;
};

cbuffer ubo : register(b2) { UniformBufferObject ubo; };

// Reference: https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl

static const float3x3 srgb_to_rrt_mat =
{
    {0.59719, 0.35458, 0.04823},
    {0.07600, 0.90834, 0.01566},
    {0.02840, 0.13383, 0.83777}
};

static const float3x3 odt_to_srgb_mat =
{
    { 1.60475, -0.53108, -0.07367},
    {-0.10208,  1.10813, -0.00605},
    {-0.00327, -0.07276,  1.07602}
};

float to_srgb(float x) {
    return x <= 0.00031308 ? 12.92 * x : 1.055 * pow(x, (1.0 / 2.4) ) - 0.055;
}

float3 to_srgb(float3 x) {
    return float3(to_srgb(x.x), to_srgb(x.y), to_srgb(x.z));
}

float from_srgb(float x)
{
    return (x <= 0.04045) ? (x / 12.92) : pow((x + 0.055) / 1.055, 2.4);
}

float3 from_srgb(float3 x) {
    return float3(from_srgb(x.x), from_srgb(x.y), from_srgb(x.z));
}

float3 rrt_to_odt_fit(float3 v)
{
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

float3 aces(float3 col)
{
    if (!ubo.src_is_srgb)
        col = to_srgb(col);
    col = mul(srgb_to_rrt_mat, col);
    col = rrt_to_odt_fit(col);
    col = mul(odt_to_srgb_mat, col);
    if (!ubo.dst_is_srgb)
        col = from_srgb(col);

    return saturate(col);
}

[numthreads(16, 16, 1)] 
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    int2 coord = dispatch_id.xy;

    if (coord.x >= ubo.width || coord.y >= ubo.height)
        return;

    float4 input = source_image[coord];
    
    result_image[coord] = float4(aces(input.rgb), input.a);
}