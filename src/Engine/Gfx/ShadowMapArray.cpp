#include "Gfx/ShadowMapArray.h"

#include <cassert>

namespace summer::gfx {

ShadowMapArray::~ShadowMapArray() {
    Shutdown();
}

bool ShadowMapArray::Initialize(
    ID3D12Device* device, CpuDescriptorHeap& dsvHeap, GpuDescriptorHeap& srvHeap,
    uint32_t width, uint32_t height) {
    Shutdown();

    GpuTexture::Desc desc{};
    desc.width = width;
    desc.height = height;
    desc.arraySize = static_cast<uint16_t>(kMaxCascades);
    desc.format = DXGI_FORMAT_R32_TYPELESS;
    desc.flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    desc.initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    desc.mipLevels = 1;
    desc.hasClearValue = true;
    desc.clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    desc.clearValue.DepthStencil.Depth = 1.0f;
    desc.clearValue.DepthStencil.Stencil = 0;

    if (!m_texture.Create(device, desc)) {
        Shutdown();
        return false;
    }
    for (uint32_t i = 0; i < kMaxCascades; ++i) {
        if (!m_dsvs[i].CreateTexture2DArraySlice(device, dsvHeap, m_texture.Resource(), DXGI_FORMAT_D32_FLOAT, i)) {
            Shutdown();
            return false;
        }
    }
    if (!m_srv.CreateTexture2DArray(device, srvHeap, m_texture.Resource(), DXGI_FORMAT_R32_FLOAT,
                                    0, kMaxCascades)) {
        Shutdown();
        return false;
    }
    return true;
}

void ShadowMapArray::Shutdown() {
    for (auto& dsv : m_dsvs) {
        dsv.Release();
    }
    m_srv.Release();
    m_texture.Release();
}

void ShadowMapArray::TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state) {
    m_texture.Transition(commandList, state);
}

D3D12_CPU_DESCRIPTOR_HANDLE ShadowMapArray::DSV(uint32_t slice) const {
    assert(slice < kMaxCascades);
    return slice < kMaxCascades ? m_dsvs[slice].Cpu() : D3D12_CPU_DESCRIPTOR_HANDLE{};
}

} // namespace summer::gfx
