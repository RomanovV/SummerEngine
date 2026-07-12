#pragma once

#include "Assets/MeshData.h"
#include "Gfx/GpuMesh.h"
#include "Gfx/Material.h"
#include "Gfx/Texture2D.h"

#include <memory>
#include <vector>

namespace summer::gfx {

class Device;
class GpuDescriptorHeap;
class BasicPipeline;

// A renderable model: one GPU mesh (possibly multiple submeshes) plus the textures and
// resolved materials it references. Owns its imported textures; the fallback base color
// texture is borrowed (owned by the Renderer).
class Model {
public:
    Model() = default;
    ~Model() = default;

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Uploads geometry and loads/resolves materials. Submeshes without a base color texture
    // (or models with no materials) fall back to fallbackBaseColor.
    bool Load(
        Device& device,
        GpuDescriptorHeap& srvHeap,
        const summer::assets::MeshData& meshData,
        Texture2D* fallbackBaseColor);

    void Shutdown();

    // Binds the mesh and draws every submesh with its material's base color texture.
    // The per-object MVP must already be set on the pipeline by the caller.
    void Draw(ID3D12GraphicsCommandList* commandList, BasicPipeline& pipeline) const;

private:
    GpuMesh m_mesh;
    std::vector<summer::assets::SubMesh> m_subMeshes;
    std::vector<Material> m_materials;
    std::vector<std::unique_ptr<Texture2D>> m_ownedTextures;
    Texture2D* m_fallbackBaseColor = nullptr;
};

} // namespace summer::gfx
