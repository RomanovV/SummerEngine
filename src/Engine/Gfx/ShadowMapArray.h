#pragma once

#include "Gfx/Descriptors.h"
#include "Gfx/GpuTexture.h"
#include "Gfx/GraphicsConstants.h"

#include <array>
#include <cstdint>

namespace summer::gfx {

// One sampleable depth texture array with a DSV for each cascade slice.
// The texture tracks one state for all slices; transition the whole array together.
class ShadowMapArray {
public:
    ShadowMapArray() = default;
    ~ShadowMapArray();

    ShadowMapArray(const ShadowMapArray&) = delete;
    ShadowMapArray& operator=(const ShadowMapArray&) = delete;

    bool Initialize(ID3D12Device* device, CpuDescriptorHeap& dsvHeap, GpuDescriptorHeap& srvHeap,
                    uint32_t width, uint32_t height);
    // The caller must ensure the GPU has finished using the resource and descriptor slots.
    void Shutdown();
    void TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state);

    ID3D12Resource* Resource() const { return m_texture.Resource(); }
    D3D12_CPU_DESCRIPTOR_HANDLE DSV(uint32_t slice) const;
    D3D12_GPU_DESCRIPTOR_HANDLE SRV() const { return m_srv.Gpu(); }

private:
    GpuTexture m_texture;
    std::array<DSVDescriptor, kMaxCascades> m_dsvs;
    SRVDescriptor m_srv;
};

} // namespace summer::gfx
