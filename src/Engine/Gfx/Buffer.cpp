#include "Gfx/Buffer.h"
#include "Gfx/Device.h"
#include "Gfx/Helpers.h"

#include <spdlog/spdlog.h>

namespace summer::gfx {

Buffer::~Buffer() {
    Shutdown();
}

void Buffer::Shutdown() {
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

    Shutdown();
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

    if (!CheckHRESULT(
            d3dDevice->CreateCommittedResource(
                &defaultHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_resource)),
            "CreateCommittedResource DEFAULT buffer")) {
        return false;
    }

    D3D12_HEAP_PROPERTIES uploadHeapProps{};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc = bufferDesc;
    ID3D12Resource* uploadResource = nullptr;
    if (!CheckHRESULT(
            d3dDevice->CreateCommittedResource(
                &uploadHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &uploadDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&uploadResource)),
            "CreateCommittedResource UPLOAD buffer")) {
        return false;
    }

    void* mapped = nullptr;
    if (!CheckHRESULT(uploadResource->Map(0, nullptr, &mapped), "Map upload buffer")) {
        uploadResource->Release();
        return false;
    }
    memcpy(mapped, data, static_cast<size_t>(size));
    uploadResource->Unmap(0, nullptr);

    ID3D12GraphicsCommandList* commandList = device.GetCommandList();
    commandList->CopyResource(m_resource, uploadResource);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_resource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = finalState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    uploadResource->Release();
    return true;
}

} // namespace summer::gfx
