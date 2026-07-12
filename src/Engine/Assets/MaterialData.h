#pragma once

#include <string>

namespace summer::assets {

// CPU-side material description imported from a model file. Metallic-roughness (glTF) workflow:
// base color + a packed occlusion/roughness/metallic map (ORM: R=occlusion, G=roughness, B=metallic)
// + tangent-space normal map + emissive. Texture paths are resolved (absolute) or empty when absent.
struct MaterialData {
    std::string name;

    std::string baseColorTexturePath;
    std::string normalTexturePath;
    std::string metalRoughTexturePath;   // glTF metallicRoughness (G=rough, B=metal); often ORM
    std::string occlusionTexturePath;    // usually the same file as metalRough (R channel)
    std::string emissiveTexturePath;

    float baseColorFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float emissiveFactor[3] = {0.0f, 0.0f, 0.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;

    bool HasBaseColorTexture() const { return !baseColorTexturePath.empty(); }
    bool HasNormalTexture() const { return !normalTexturePath.empty(); }
    bool HasMetalRoughTexture() const { return !metalRoughTexturePath.empty(); }
    bool HasOcclusionTexture() const { return !occlusionTexturePath.empty(); }
    bool HasEmissiveTexture() const { return !emissiveTexturePath.empty(); }
};

} // namespace summer::assets
