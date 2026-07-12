// Resolves the HDR scene target to the (UNORM) swapchain: exposure -> tonemap -> gamma encode.

Texture2D gHdr        : register(t0);
SamplerState gSampler : register(s0);

cbuffer TonemapConstants : register(b0)
{
    float exposure;
    uint op; // 0 = Reinhard, 1 = ACES
    float2 _pad;
};

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float3 ReinhardTonemap(float3 c)
{
    return c / (1.0f + c);
}

// Narkowicz 2015 ACES approximation.
float3 ACESFilm(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 LinearToSrgb(float3 c)
{
    // Backbuffer is UNORM (not _SRGB), so encode gamma manually.
    return pow(saturate(c), 1.0f / 2.2f);
}

float4 main(PSInput input) : SV_Target
{
    float3 hdr = gHdr.Sample(gSampler, input.uv).rgb;
    hdr *= exposure;

    float3 mapped = (op == 1u) ? ACESFilm(hdr) : ReinhardTonemap(hdr);
    return float4(LinearToSrgb(mapped), 1.0f);
}
