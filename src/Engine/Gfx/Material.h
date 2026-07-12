#pragma once

#include <DirectXMath.h>

namespace summer::gfx {

class Texture2D;

// GPU-side PBR material (metallic-roughness workflow). Texture pointers are borrowed:
// they point either at a Model-owned imported texture or a Renderer-owned procedural default,
// so every slot is always bound to a valid SRV.
struct Material {
    Texture2D* baseColor = nullptr;
    Texture2D* normal = nullptr;
    Texture2D* metalRough = nullptr;   // G=roughness, B=metallic (ORM: R=occlusion)
    Texture2D* occlusion = nullptr;
    Texture2D* emissive = nullptr;

    DirectX::XMFLOAT4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 emissiveFactor{0.0f, 0.0f, 0.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
};

} // namespace summer::gfx
