#pragma once

#include <DirectXMath.h>
#include <cstdint>

namespace summer::gfx {

// Keep in sync with the matching structs in shaders/PbrPS.hlsl (HLSL cbuffer packing rules).
inline constexpr uint32_t kMaxPointLights = 4;

struct DirectionalLightGpu {
    DirectX::XMFLOAT3 direction{-0.4f, -0.8f, 0.3f}; // toward the scene
    float intensity = 3.0f;
    DirectX::XMFLOAT3 color{1.0f, 0.98f, 0.95f};
    float _pad = 0.0f;
};

struct PointLightGpu {
    DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
    float radius = 10.0f;
    DirectX::XMFLOAT3 color{1.0f, 1.0f, 1.0f};
    float intensity = 0.0f; // 0 = effectively disabled
};

// Per-frame constants bound at b1. alignas(256) satisfies D3D12 CBV alignment.
struct alignas(256) FrameConstants {
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT3 cameraPos{0.0f, 0.0f, 0.0f};
    float _pad0 = 0.0f;
    DirectionalLightGpu dirLight;
    DirectX::XMFLOAT3 ambient{0.03f, 0.03f, 0.04f};
    int pointLightCount = 0;
    PointLightGpu pointLights[kMaxPointLights];
};

} // namespace summer::gfx
