#include "Gfx/GpuMesh.h"
#include "Gfx/Device.h"
#include "Gfx/Helpers.h"
#include "Assets/MeshData.h"

#include <spdlog/spdlog.h>

namespace summer::gfx {

GpuMesh::~GpuMesh() {
    Shutdown();
}

void GpuMesh::Shutdown() {
    m_vertexBuffer.Shutdown();
    m_indexBuffer.Shutdown();
    m_indexCount = 0;
}

bool GpuMesh::Initialize(Device& device, const summer::assets::MeshData& meshData) {
    if (!meshData.IsValid()) {
        spdlog::error("GpuMesh::Initialize called with invalid MeshData");
        return false;
    }

    Shutdown();
    m_indexCount = meshData.indexCount();

    FrameContext& frame = device.GetFrame(0);
    if (!CheckHRESULT(frame.CommandAllocator->Reset(), "Reset allocator for mesh upload")) {
        return false;
    }
    if (!CheckHRESULT(
            device.GetCommandList()->Reset(frame.CommandAllocator, nullptr),
            "Reset command list for mesh upload")) {
        return false;
    }

    const uint64_t vertexBufferSize =
        static_cast<uint64_t>(meshData.vertices.size() * sizeof(summer::assets::Vertex));
    if (!m_vertexBuffer.CreateAndUpload(
            device,
            meshData.vertices.data(),
            vertexBufferSize,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)) {
        return false;
    }

    const uint64_t indexBufferSize = static_cast<uint64_t>(meshData.indices.size() * sizeof(uint32_t));
    if (!m_indexBuffer.CreateAndUpload(
            device,
            meshData.indices.data(),
            indexBufferSize,
            D3D12_RESOURCE_STATE_INDEX_BUFFER)) {
        m_vertexBuffer.Shutdown();
        return false;
    }

    if (!CheckHRESULT(device.GetCommandList()->Close(), "Close command list for mesh upload")) {
        m_vertexBuffer.Shutdown();
        m_indexBuffer.Shutdown();
        return false;
    }

    ID3D12CommandList* commandLists[] = {device.GetCommandList()};
    device.GetCommandQueue()->ExecuteCommandLists(1, commandLists);
    device.WaitForGpu();

    // Safe to release staging buffers once the GPU copy has completed.
    m_vertexBuffer.ReleaseUploadStaging();
    m_indexBuffer.ReleaseUploadStaging();

    m_vertexBufferView.BufferLocation = m_vertexBuffer.GetResource()->GetGPUVirtualAddress();
    m_vertexBufferView.SizeInBytes = static_cast<UINT>(vertexBufferSize);
    m_vertexBufferView.StrideInBytes = sizeof(summer::assets::Vertex);

    m_indexBufferView.BufferLocation = m_indexBuffer.GetResource()->GetGPUVirtualAddress();
    m_indexBufferView.SizeInBytes = static_cast<UINT>(indexBufferSize);
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

    spdlog::info("GpuMesh uploaded ({} indices)", m_indexCount);
    return true;
}

void GpuMesh::Bind(ID3D12GraphicsCommandList* commandList) const {
    if (!commandList || m_indexCount == 0) {
        return;
    }
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
}

void GpuMesh::DrawSubMesh(
    ID3D12GraphicsCommandList* commandList,
    uint32_t indexOffset,
    uint32_t indexCount) const {
    if (!commandList || indexCount == 0) {
        return;
    }
    commandList->DrawIndexedInstanced(indexCount, 1, indexOffset, 0, 0);
}

void GpuMesh::Draw(ID3D12GraphicsCommandList* commandList) const {
    if (!commandList || m_indexCount == 0) {
        return;
    }
    Bind(commandList);
    DrawSubMesh(commandList, 0, m_indexCount);
}

} // namespace summer::gfx
