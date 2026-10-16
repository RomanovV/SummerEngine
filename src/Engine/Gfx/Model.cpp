#include "Gfx/Model.h"
#include "Gfx/Device.h"
#include "Gfx/GeometryPipeline.h"
#include "Gfx/GpuDescriptorHeap.h"
#include "Gfx/PbrPipeline.h"

#include <spdlog/spdlog.h>

#include <unordered_map>

namespace summer::gfx {

bool Model::Load(
    Device& device,
    GpuDescriptorHeap& srvHeap,
    const summer::assets::MeshData& meshData,
    const MaterialDefaults& defaults) {
    if (!meshData.IsValid()) {
        spdlog::error("Model::Load called with invalid MeshData");
        return false;
    }

    Shutdown();
    m_defaults = defaults;

    if (!m_mesh.Initialize(device, meshData)) {
        return false;
    }

    m_subMeshes = meshData.subMeshes;

    // Load each unique texture once (keyed by path + colorspace), then resolve materials.
    std::unordered_map<std::string, Texture2D*> textureCache;
    auto loadTexture = [&](const std::string& path, bool srgb) -> Texture2D* {
        if (path.empty()) {
            return nullptr;
        }
        const std::string key = (srgb ? "s:" : "l:") + path;
        auto cached = textureCache.find(key);
        if (cached != textureCache.end()) {
            return cached->second;
        }
        auto texture = std::make_unique<Texture2D>();
        if (!texture->CreateFromFile(device, srvHeap, path, srgb)) {
            spdlog::warn("Model: failed to load texture '{}', using default", path);
            textureCache.emplace(key, nullptr);
            return nullptr;
        }
        Texture2D* raw = texture.get();
        m_ownedTextures.push_back(std::move(texture));
        textureCache.emplace(key, raw);
        return raw;
    };

    m_materials.reserve(meshData.materials.size());
    for (const summer::assets::MaterialData& src : meshData.materials) {
        Material material;
        material.baseColorFactor = DirectX::XMFLOAT4(
            src.baseColorFactor[0], src.baseColorFactor[1],
            src.baseColorFactor[2], src.baseColorFactor[3]);
        material.emissiveFactor =
            DirectX::XMFLOAT3(src.emissiveFactor[0], src.emissiveFactor[1], src.emissiveFactor[2]);
        material.metallicFactor = src.metallicFactor;
        material.roughnessFactor = src.roughnessFactor;
        material.normalScale = src.normalScale;
        material.occlusionStrength = src.occlusionStrength;

        // Color textures use an sRGB view; data textures (normal/metalRough/occlusion) stay linear.
        Texture2D* baseColor = loadTexture(src.baseColorTexturePath, /*srgb=*/true);
        Texture2D* normal = loadTexture(src.normalTexturePath, /*srgb=*/false);
        Texture2D* metalRough = loadTexture(src.metalRoughTexturePath, /*srgb=*/false);
        Texture2D* occlusion = loadTexture(src.occlusionTexturePath, /*srgb=*/false);
        Texture2D* emissive = loadTexture(src.emissiveTexturePath, /*srgb=*/true);

        material.baseColor = baseColor ? baseColor : m_defaults.baseColor;
        material.normal = normal ? normal : m_defaults.normal;
        material.metalRough = metalRough ? metalRough : m_defaults.metalRough;
        material.occlusion = occlusion ? occlusion : m_defaults.occlusion;
        material.emissive = emissive ? emissive : m_defaults.emissive;
        m_materials.push_back(material);
    }

    // Ensure there's always at least one material to reference.
    if (m_materials.empty()) {
        Material material;
        material.baseColor = m_defaults.baseColor;
        material.normal = m_defaults.normal;
        material.metalRough = m_defaults.metalRough;
        material.occlusion = m_defaults.occlusion;
        material.emissive = m_defaults.emissive;
        m_materials.push_back(material);
    }

    return true;
}

void Model::Shutdown() {
    m_ownedTextures.clear();
    m_materials.clear();
    m_subMeshes.clear();
    m_mesh.Shutdown();
    m_defaults = {};
}

void Model::Draw(ID3D12GraphicsCommandList* commandList, PbrPipeline& pipeline) const {
    if (!commandList) {
        return;
    }

    m_mesh.Bind(commandList);

    for (const summer::assets::SubMesh& subMesh : m_subMeshes) {
        const uint32_t materialIndex =
            subMesh.materialIndex < m_materials.size() ? subMesh.materialIndex : 0;
        pipeline.SetMaterial(commandList, m_materials[materialIndex]);
        m_mesh.DrawSubMesh(commandList, subMesh.indexOffset, subMesh.indexCount);
    }
}

void Model::Draw(ID3D12GraphicsCommandList* commandList, GeometryPipeline& pipeline) const {
    if (!commandList) {
        return;
    }

    m_mesh.Bind(commandList);

    for (const summer::assets::SubMesh& subMesh : m_subMeshes) {
        const uint32_t materialIndex =
            subMesh.materialIndex < m_materials.size() ? subMesh.materialIndex : 0;
        pipeline.SetMaterial(commandList, m_materials[materialIndex]);
        m_mesh.DrawSubMesh(commandList, subMesh.indexOffset, subMesh.indexCount);
    }
}

} // namespace summer::gfx
