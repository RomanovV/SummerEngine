#pragma once

#include "Gfx/Buffer.h"

#include <cstdint>
#include <d3d12.h>

namespace summer::assets {
struct MeshData;
}

namespace summer::gfx {

class Device;

// GPU-resident mesh built from CPU MeshData.
class GpuMesh {
public:
    GpuMesh() = default;
    ~GpuMesh();

    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;

    bool Initialize(Device& device, const summer::assets::MeshData& meshData);
    void Shutdown();

    void Draw(ID3D12GraphicsCommandList* commandList) const;

    uint32_t GetIndexCount() const { return m_indexCount; }

private:
    Buffer m_vertexBuffer;
    Buffer m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};
    uint32_t m_indexCount = 0;
};

} // namespace summer::gfx
