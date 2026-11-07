static const float PI = 3.14159265359f;
static const uint kMaxPointLights = 4;
static const uint kMaxCascades = 8; // Should mirror C++ definition
static const float3 kCascadeColors[kMaxCascades] = {
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
    0.5f, 0.0f, 0.5f,
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 0.0f,
    1.0f, 0.5f, 0.0f,
    1.0f, 0.0f, 1.0f,
};

struct DirectionalLight {
    float3 direction;
    float intensity;
    float3 color;
    float _pad;
};

struct PointLight {
    float3 position;
    float radius;
    float3 color;
    float intensity;
};

cbuffer FrameConstants : register(b1) {
    float4x4 viewProj;
    float3 cameraPos;
    float _pad0;
    DirectionalLight dirLight;
    float3 ambient;
    int pointLightCount;
    PointLight pointLights[kMaxPointLights];
    float4x4 invViewProj;
    float4 shadowParams;
    float4x4 view;
    float4 cascadeFarSplits[kMaxCascades];
    uint4 cascadeDebug;
    float4x4 cascadeLightViewProj[kMaxCascades];
};

Texture2D gAlbedo   : register(t0);
Texture2D gNormal   : register(t1);
Texture2D gMaterial : register(t2);
Texture2D gDepth    : register(t3);
Texture2DArray gShadowMap : register(t4);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

struct PSInput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

#include "PbrCommon.hlsli"

float3 WorldPositionFromDepth(float2 uv, float depth) {
    float4 clip = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f);
    float4 world = mul(clip, invViewProj);
    return world.xyz / world.w;
}

uint SelectCascade(float3 worldPos) {
    float viewZ = mul(float4(worldPos, 1.0f), view).z;
    for (int i=0;i<cascadeDebug.x;++i)
    {
        if (viewZ <= cascadeFarSplits[i].x) return i;
    }
    return cascadeDebug.x;
}

float DirectionalShadow(float3 worldPos, float3 N, uint cascadeIndex) {
    if (cascadeIndex >= cascadeDebug.x) return 1.0f;
    
    float3 biasedPos = worldPos + N * shadowParams.x; // shadowParams.x = NormalOffset
    float4 lightClip = mul(float4(biasedPos, 1.0f), cascadeLightViewProj[cascadeIndex]);
    float3 ndc = lightClip.xyz / lightClip.w;
    float2 uv = float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
    
    if (any(uv < 0.0f) || any(uv > 1.0f) || ndc.z < 0.0f || ndc.z > 1.0f) return 1.0f;
    
    float texel = shadowParams.z; // 1/shadow map size
    float refDepth = ndc.z - shadowParams.y; // shadowParams.y = depth bias
    float lit = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float2 sampleUv = uv + float2(x, y) * texel;
            float3 sampleLocation = float3(sampleUv, float(cascadeIndex));
            lit += gShadowMap.SampleCmpLevelZero(gShadowSampler, sampleLocation, refDepth);
        }
    }
    
    return lit / 9.0f; // 3x3 kernel
}

float4 main(PSInput input) : SV_Target {
    float depth = gDepth.Sample(gSampler, input.uv).r;
    if (depth >= 0.99999f) {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float4 albedoSample = gAlbedo.Sample(gSampler, input.uv);
    float3 albedo = albedoSample.rgb;
    float occlusion = albedoSample.a;
    float3 N = normalize(gNormal.Sample(gSampler, input.uv).xyz);
    float3 packed = gMaterial.Sample(gSampler, input.uv).rgb;
    float metallic = packed.r;
    float roughness = packed.g;
    float emissive = packed.b;

    float3 worldPos = WorldPositionFromDepth(input.uv, depth);
    uint cascadeIndex = SelectCascade(worldPos);
    float3 V = normalize(cameraPos - worldPos);

    float3 color = 0.0f;

    {
        float3 L = normalize(-dirLight.direction);
        float3 radiance = dirLight.color * dirLight.intensity;
        float shadow = DirectionalShadow(worldPos, N, cascadeIndex);
        color += BRDF(N, V, L, radiance, albedo, metallic, roughness) * shadow;
    }

    for (int i = 0; i < pointLightCount; ++i) {
        float3 toLight = pointLights[i].position - worldPos;
        float distance = length(toLight);
        float3 L = toLight / max(distance, 1e-4f);
        float attenuation = 1.0f / max(distance * distance, 1e-4f);
        float window = saturate(1.0f - pow(distance / max(pointLights[i].radius, 1e-4f), 4.0f));
        attenuation *= window * window;
        float3 radiance = pointLights[i].color * pointLights[i].intensity * attenuation;
        color += BRDF(N, V, L, radiance, albedo, metallic, roughness);
    }

    // Material AO approximates indirect occlusion; direct-light visibility comes from shadows.
    float3 ambientTerm = ambient * albedo * occlusion;
    float3 finalColor = color + ambientTerm + emissive;
    if (cascadeDebug.y != 0u && cascadeIndex < cascadeDebug.x)
    {
        finalColor *= lerp(float3(1.0f, 1.0f, 1.0f), kCascadeColors[cascadeIndex], 0.65f);
    }
    return float4(finalColor, 1.0f);
}