#pragma once

#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace summer::gfx {

// Owns a committed 2D texture resource and tracks its current resource state, so callers can
// request a transition without hand-tracking barriers. This is the building block composed by
// RenderTarget and DepthStencil (and later G-buffer attachments / shadow maps).
class GpuTexture {
public:
    struct Desc {
        uint32_t width = 0;
        uint32_t height = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN; // resource format (may be typeless for depth+SRV)
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
        uint16_t mipLevels = 1;
        bool hasClearValue = false;      // RTs/DSs want an optimized clear value
        D3D12_CLEAR_VALUE clearValue{};
    };

    GpuTexture() = default;
    ~GpuTexture();

    GpuTexture(const GpuTexture&) = delete;
    GpuTexture& operator=(const GpuTexture&) = delete;

    bool Create(ID3D12Device* device, const Desc& desc);
    void Release();

    // Records a transition barrier from the tracked state to newState (no-op if already there).
    void Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES newState);

    ID3D12Resource* Resource() const { return m_resource; }
    D3D12_RESOURCE_STATES State() const { return m_state; }
    // Keep tracking in sync when a transition is recorded outside this object (e.g. batched barriers).
    void SetTrackedState(D3D12_RESOURCE_STATES state) { m_state = state; }

    uint32_t Width() const { return m_desc.width; }
    uint32_t Height() const { return m_desc.height; }
    DXGI_FORMAT Format() const { return m_desc.format; }

private:
    ID3D12Resource* m_resource = nullptr;
    D3D12_RESOURCE_STATES m_state = D3D12_RESOURCE_STATE_COMMON;
    Desc m_desc{};
};

} // namespace summer::gfx
