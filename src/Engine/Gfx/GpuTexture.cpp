#include "Gfx/GpuTexture.h"
#include "Gfx/Helpers.h"

namespace summer::gfx {

GpuTexture::~GpuTexture() {
    Release();
}

bool GpuTexture::Create(ID3D12Device* device, const Desc& desc) {
    Release();
    if (!device || desc.width == 0 || desc.height == 0) {
        return false;
    }
    m_desc = desc;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = desc.width;
    resourceDesc.Height = desc.height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = desc.mipLevels;
    resourceDesc.Format = desc.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = desc.flags;

    const D3D12_CLEAR_VALUE* clear = desc.hasClearValue ? &desc.clearValue : nullptr;

    if (!CheckHRESULT(
            device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                desc.initialState,
                clear,
                IID_PPV_ARGS(&m_resource)),
            "CreateCommittedResource (GpuTexture)")) {
        return false;
    }

    m_state = desc.initialState;
    return true;
}

void GpuTexture::Release() {
    if (m_resource) {
        m_resource->Release();
        m_resource = nullptr;
    }
    m_state = D3D12_RESOURCE_STATE_COMMON;
    m_desc = {};
}

void GpuTexture::Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES newState) {
    if (!m_resource || !commandList || m_state == newState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_resource;
    barrier.Transition.StateBefore = m_state;
    barrier.Transition.StateAfter = newState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    m_state = newState;
}

} // namespace summer::gfx
