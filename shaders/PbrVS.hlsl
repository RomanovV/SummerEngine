// Per-object constants (root 32-bit constants).
cbuffer ObjectConstants : register(b0)
{
    float4x4 model;        // object -> world
    float4x4 normalMatrix; // inverse-transpose of the upper 3x3 (as 4x4), for normals/tangents
};

// Per-frame constants (subset needed by the VS).
cbuffer FrameConstants : register(b1)
{
    float4x4 viewProj;
    float3 cameraPos;
    float _pad0;
    // remaining light data unused in the VS
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT; // xyz = tangent, w = handedness
    float2 uv       : TEXCOORD0;
};

struct PSInput
{
    float4 clipPos  : SV_Position;
    float3 worldPos : WORLDPOS;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv       : TEXCOORD0;
};

PSInput main(VSInput input)
{
    PSInput output;

    float4 worldPos = mul(float4(input.position, 1.0f), model);
    output.worldPos = worldPos.xyz;
    output.clipPos = mul(worldPos, viewProj);

    output.normal = normalize(mul(float4(input.normal, 0.0f), normalMatrix).xyz);
    output.tangent = float4(
        normalize(mul(float4(input.tangent.xyz, 0.0f), normalMatrix).xyz),
        input.tangent.w);
    output.uv = input.uv;
    return output;
}
