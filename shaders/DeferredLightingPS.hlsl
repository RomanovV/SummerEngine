static const float PI = 3.14159265359f;
static const uint kMaxPointLights = 4;

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
};

Texture2D gAlbedo   : register(t0);
Texture2D gNormal   : register(t1);
Texture2D gMaterial : register(t2);
Texture2D gDepth    : register(t3);
SamplerState gSampler : register(s0);

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
    float3 V = normalize(cameraPos - worldPos);

    float3 color = 0.0f;

    {
        float3 L = normalize(-dirLight.direction);
        float3 radiance = dirLight.color * dirLight.intensity;
        color += BRDF(N, V, L, radiance, albedo, metallic, roughness);
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

    float3 ambientTerm = ambient * albedo * occlusion;
    float3 finalColor = color * occlusion + ambientTerm + emissive;
    return float4(finalColor, 1.0f);
}