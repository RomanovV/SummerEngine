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
class PbrPipeline;

// Renderer-owned procedural fallback textures, used for any material slot a model lacks.
// Every pointer must be valid so bindings always resolve.
struct MaterialDefaults {
    Texture2D* baseColor = nullptr;   // white
    Texture2D* normal = nullptr;      // flat (128,128,255)
    Texture2D* metalRough = nullptr;  // roughness=1, metallic=0
    Texture2D* occlusion = nullptr;   // white
    Texture2D* emissive = nullptr;    // black
};

// A renderable model: one GPU mesh (possibly multiple submeshes) plus the textures and
// resolved PBR materials it references. Owns its imported textures; default textures are borrowed.
class Model {
public:
    Model() = default;
    ~Model() = default;

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Uploads geometry and loads/resolves materials. Missing texture slots (or models with no
    // materials) fall back to the provided defaults.
    bool Load(
        Device& device,
        GpuDescriptorHeap& srvHeap,
        const summer::assets::MeshData& meshData,
        const MaterialDefaults& defaults);

    void Shutdown();

    // Binds the mesh and draws every submesh with its material (textures + factors).
    // The per-object constants must already be set on the pipeline by the caller.
    void Draw(ID3D12GraphicsCommandList* commandList, PbrPipeline& pipeline) const;

    // Mutable access to resolved materials (e.g. ImGui overrides). Order matches import order.
    std::vector<Material>& Materials() { return m_materials; }
    const std::vector<Material>& Materials() const { return m_materials; }

private:
    GpuMesh m_mesh;
    std::vector<summer::assets::SubMesh> m_subMeshes;
    std::vector<Material> m_materials;
    std::vector<std::unique_ptr<Texture2D>> m_ownedTextures;
    MaterialDefaults m_defaults;
};

} // namespace summer::gfx
