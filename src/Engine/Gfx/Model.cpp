#include "Gfx/Model.h"
#include "Gfx/BasicPipeline.h"
#include "Gfx/Device.h"
#include "Gfx/GpuDescriptorHeap.h"

#include <spdlog/spdlog.h>

#include <unordered_map>

namespace summer::gfx {

bool Model::Load(
    Device& device,
    GpuDescriptorHeap& srvHeap,
    const summer::assets::MeshData& meshData,
    Texture2D* fallbackBaseColor) {
    if (!meshData.IsValid()) {
        spdlog::error("Model::Load called with invalid MeshData");
        return false;
    }

    Shutdown();
    m_fallbackBaseColor = fallbackBaseColor;

    if (!m_mesh.Initialize(device, meshData)) {
        return false;
    }

    m_subMeshes = meshData.subMeshes;

    // Load each unique texture once, then resolve one gfx::Material per imported material.
    std::unordered_map<std::string, Texture2D*> textureCache;
    auto loadTexture = [&](const std::string& path) -> Texture2D* {
        auto cached = textureCache.find(path);
        if (cached != textureCache.end()) {
            return cached->second;
        }
        auto texture = std::make_unique<Texture2D>();
        if (!texture->CreateFromFile(device, srvHeap, path, /*srgb=*/true)) {
            spdlog::warn("Model: failed to load base color '{}', using fallback", path);
            textureCache.emplace(path, nullptr);
            return nullptr;
        }
        Texture2D* raw = texture.get();
        m_ownedTextures.push_back(std::move(texture));
        textureCache.emplace(path, raw);
        return raw;
    };

    m_materials.reserve(meshData.materials.size());
    for (const summer::assets::MaterialData& materialData : meshData.materials) {
        Material material;
        material.baseColorFactor = DirectX::XMFLOAT4(
            materialData.baseColorFactor[0],
            materialData.baseColorFactor[1],
            materialData.baseColorFactor[2],
            materialData.baseColorFactor[3]);

        Texture2D* baseColor = nullptr;
        if (materialData.HasBaseColorTexture()) {
            baseColor = loadTexture(materialData.baseColorTexturePath);
        }
        material.baseColor = baseColor ? baseColor : m_fallbackBaseColor;
        m_materials.push_back(material);
    }

    // Ensure there's always at least one material to reference.
    if (m_materials.empty()) {
        Material material;
        material.baseColor = m_fallbackBaseColor;
        m_materials.push_back(material);
    }

    return true;
}

void Model::Shutdown() {
    m_ownedTextures.clear();
    m_materials.clear();
    m_subMeshes.clear();
    m_mesh.Shutdown();
    m_fallbackBaseColor = nullptr;
}

void Model::Draw(ID3D12GraphicsCommandList* commandList, BasicPipeline& pipeline) const {
    if (!commandList) {
        return;
    }

    m_mesh.Bind(commandList);

    for (const summer::assets::SubMesh& subMesh : m_subMeshes) {
        const uint32_t materialIndex =
            subMesh.materialIndex < m_materials.size() ? subMesh.materialIndex : 0;
        const Material& material = m_materials[materialIndex];

        const Texture2D* baseColor =
            material.baseColor ? material.baseColor : m_fallbackBaseColor;
        if (baseColor && baseColor->IsValid()) {
            pipeline.SetBaseColorTexture(commandList, baseColor->GetSrvGpu());
        }

        m_mesh.DrawSubMesh(commandList, subMesh.indexOffset, subMesh.indexCount);
    }
}

} // namespace summer::gfx
