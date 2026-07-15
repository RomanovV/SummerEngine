#pragma once

#include "Gfx/Descriptors.h"
#include "Gfx/GpuTexture.h"

#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace summer::gfx {

class CpuDescriptorHeap;
class GpuDescriptorHeap;

// A depth-stencil buffer: one GpuTexture + a DSV, and (optionally) a shader-visible SRV so the
// depth can be sampled later (e.g. deferred world-position reconstruction). When an SRV heap is
// supplied, the resource is created with a typeless format so both a depth DSV and a float SRV can
// alias it. The DSV heap slot comes from a global DSV manager; the SRV from the shared heap.
class DepthStencil {
public:
    DepthStencil() = default;
    ~DepthStencil();

    DepthStencil(const DepthStencil&) = delete;
    DepthStencil& operator=(const DepthStencil&) = delete;

    // Pass srvHeap != nullptr to also create a sampleable SRV (typeless resource).
    bool Initialize(
        ID3D12Device* device,
        CpuDescriptorHeap& dsvHeap,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT,
        GpuDescriptorHeap* srvHeap = nullptr);
    void Shutdown();

    // Caller must ensure the GPU is idle (Device::WaitForGpu) before resizing.
    bool Resize(ID3D12Device* device, uint32_t width, uint32_t height);

    // Records a barrier to the requested state (tracked internally by the GpuTexture).
    void TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state);

    ID3D12Resource* Resource() const { return m_texture.Resource(); }
    D3D12_CPU_DESCRIPTOR_HANDLE DSV() const { return m_dsv.Cpu(); }
    DXGI_FORMAT DsvFormat() const { return m_dsvFormat; }

    bool HasSRV() const { return m_srv.IsValid(); }
    D3D12_GPU_DESCRIPTOR_HANDLE SRV() const { return m_srv.Gpu(); }

private:
    bool CreateResourceAndViews(ID3D12Device* device);

    CpuDescriptorHeap* m_dsvHeap = nullptr;
    GpuDescriptorHeap* m_srvHeap = nullptr; // optional (sampleable depth)
    GpuTexture m_texture;
    DSVDescriptor m_dsv;
    SRVDescriptor m_srv; // valid only when m_srvHeap != nullptr
    DXGI_FORMAT m_dsvFormat = DXGI_FORMAT_D32_FLOAT;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace summer::gfx
