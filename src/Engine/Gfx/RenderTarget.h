#pragma once

#include "Gfx/Descriptors.h"
#include "Gfx/GpuTexture.h"

#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace summer::gfx {

class CpuDescriptorHeap;
class GpuDescriptorHeap;

// A color render target that is also sampleable: one GpuTexture + an RTV + a shader-visible SRV.
// Reusable for the HDR scene target, G-buffer attachments, post-process buffers, shadow-map color,
// etc. Descriptor slots are borrowed from the global heaps passed in; this owns no heap.
class RenderTarget {
public:
    RenderTarget() = default;
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    // clearColor is optional (defaults to opaque black) and is baked into the resource's optimized
    // clear value; call ClearColor() to clear with the matching value.
    bool Initialize(
        ID3D12Device* device,
        CpuDescriptorHeap& rtvHeap,
        GpuDescriptorHeap& srvHeap,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        const float clearColor[4] = nullptr);
    void Shutdown();

    // Caller must ensure the GPU is idle (Device::WaitForGpu) before resizing.
    bool Resize(ID3D12Device* device, uint32_t width, uint32_t height);

    // Records a barrier to the requested state (state is tracked internally by the GpuTexture).
    void TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state);

    ID3D12Resource* Resource() const { return m_texture.Resource(); }
    DXGI_FORMAT Format() const { return m_format; }
    D3D12_CPU_DESCRIPTOR_HANDLE RTV() const { return m_rtv.Cpu(); }
    D3D12_GPU_DESCRIPTOR_HANDLE SRV() const { return m_srv.Gpu(); }
    const float* ClearColor() const { return m_clearColor; }

private:
    bool CreateResourceAndViews(ID3D12Device* device);

    CpuDescriptorHeap* m_rtvHeap = nullptr;
    GpuDescriptorHeap* m_srvHeap = nullptr;
    GpuTexture m_texture;
    RTVDescriptor m_rtv;
    SRVDescriptor m_srv;
    DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    float m_clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace summer::gfx
