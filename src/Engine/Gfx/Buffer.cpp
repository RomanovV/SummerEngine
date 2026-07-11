#include "Gfx/Buffer.h"
#include "Gfx/Device.h"
#include "Gfx/Helpers.h"

#include <cstring>
#include <spdlog/spdlog.h>

namespace summer::gfx {

Buffer::~Buffer() {
    Shutdown();
}

void Buffer::ReleaseUploadStaging() {
    if (m_uploadStaging) {
        m_uploadStaging->Release();
        m_uploadStaging = nullptr;
    }
}

void Buffer::Shutdown() {
    ReleaseUploadStaging();

    if (m_resource) {
        m_resource->Release();
        m_resource = nullptr;
    }
    m_size = 0;
}

bool Buffer::CreateAndUpload(
    Device& device,
    const void* data,
    uint64_t size,
    D3D12_RESOURCE_STATES finalState) {
    if (!data || size == 0) {
        return false;
    }

    if (m_resource) {
        spdlog::error("Buffer::CreateAndUpload called on an already-initialized buffer");
        return false;
    }

    m_size = size;

    ID3D12Device* d3dDevice = device.GetD3D12Device();

    D3D12_HEAP_PROPERTIES defaultHeapProps{};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = size;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // Buffers are effectively created in COMMON regardless of the InitialState passed here.
    if (!CheckHRESULT(
            d3dDevice->CreateCommittedResource(
                &defaultHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&m_resource)),
            "CreateCommittedResource DEFAULT buffer")) {
        return false;
    }

    D3D12_HEAP_PROPERTIES uploadHeapProps{};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    if (!CheckHRESULT(
            d3dDevice->CreateCommittedResource(
                &uploadHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_uploadStaging)),
            "CreateCommittedResource UPLOAD buffer")) {
        Shutdown();
        return false;
    }

    void* mapped = nullptr;
    if (!CheckHRESULT(m_uploadStaging->Map(0, nullptr, &mapped), "Map upload buffer")) {
        Shutdown();
        return false;
    }
    memcpy(mapped, data, static_cast<size_t>(size));
    m_uploadStaging->Unmap(0, nullptr);

    ID3D12GraphicsCommandList* commandList = device.GetCommandList();

    D3D12_RESOURCE_BARRIER barriers[2]{};

    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = m_resource;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barriers[0]);
    commandList->CopyResource(m_resource, m_uploadStaging);

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = m_resource;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = finalState;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barriers[1]);

    // m_uploadStaging stays alive until GpuMesh finishes executing this command list.
    return true;
}

} // namespace summer::gfx
