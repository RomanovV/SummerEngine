#pragma once

#include "Gfx/DepthStencil.h"
#include "Gfx/RenderTarget.h"

#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace summer::gfx {

class CpuDescriptorHeap;
class GpuDescriptorHeap;

// Deferred G-buffer. Composes color RenderTargets plus one sampleable DepthStencil; it does not
// inherit RenderTarget. The forward pass keeps its own depth until the geometry pass is wired up.
//
// Slot layout (the geometry pixel shader writes these, the lighting pass samples them):
//   Albedo   R8G8B8A8_UNORM        RGB linear albedo, A occlusion
//   Normal   R16G16B16A16_FLOAT    RGB world-space normal (unpacked, can be negative)
//   Material R8G8B8A8_UNORM        R metallic, G roughness, B emissive strength
//   Depth    D32_FLOAT + R32_FLOAT SRV for world-position reconstruction
class GBuffer {
public:
    enum Slot : uint32_t {
        Albedo = 0,
        Normal = 1,
        Material = 2,
        ColorCount = 3,
    };

    static constexpr DXGI_FORMAT kAlbedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT kNormalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT kMaterialFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;

    GBuffer() = default;
    ~GBuffer();

    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;

    bool Initialize(
        ID3D12Device* device,
        CpuDescriptorHeap& rtvHeap,
        CpuDescriptorHeap& dsvHeap,
        GpuDescriptorHeap& srvHeap,
        uint32_t width,
        uint32_t height);
    void Shutdown();

    // Caller must ensure the GPU is idle (Device::WaitForGpu) before resizing.
    bool Resize(ID3D12Device* device, uint32_t width, uint32_t height);

    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }
    uint32_t GetColorCount() const { return static_cast<uint32_t>(m_colors.size()); }

    RenderTarget& Color(uint32_t slot) { return m_colors[slot]; }
    const RenderTarget& Color(uint32_t slot) const { return m_colors[slot]; }
    DepthStencil& Depth() { return m_depth; }
    const DepthStencil& Depth() const { return m_depth; }

    // Fills out[0..ColorCount) with the color RTVs, in slot order, for OMSetRenderTargets.
    void CopyColorRtvs(D3D12_CPU_DESCRIPTOR_HANDLE* out) const;

    void TransitionColorsTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state);
    void TransitionDepthTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state);

    // Clears every color target with its optimized clear color and the depth to 1.
    // Color targets must already be in RENDER_TARGET; depth must be in DEPTH_WRITE.
    void Clear(ID3D12GraphicsCommandList* commandList) const;

private:
    std::vector<RenderTarget> m_colors;
    DepthStencil m_depth;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace summer::gfx
