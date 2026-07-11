cbuffer FrameConstants : register(b0)
{
    float4x4 mvp;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), mvp);
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}
