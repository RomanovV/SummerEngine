#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace summer::gfx {

class Device;

// Fullscreen deferred lighting: samples the G-buffer + depth and writes linear HDR radiance.
// Bindings:
//   t0..t3  four SRV tables  - albedo, normal, material, depth
//   b1      CBV              - FrameConstants (camera + lights + invViewProj)
//   s0      static sampler   - point, clamp
class DeferredLightingPipeline {
public:
    DeferredLightingPipeline() = default;
    ~DeferredLightingPipeline();

    DeferredLightingPipeline(const DeferredLightingPipeline&) = delete;
    DeferredLightingPipeline& operator=(const DeferredLightingPipeline&) = delete;

    bool Initialize(
        Device& device,
        DXGI_FORMAT rtvFormat,
        const char* vertexShaderPath,
        const char* pixelShaderPath);
    void Shutdown();

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        D3D12_GPU_VIRTUAL_ADDRESS frameCb,
        D3D12_GPU_DESCRIPTOR_HANDLE albedo,
        D3D12_GPU_DESCRIPTOR_HANDLE normal,
        D3D12_GPU_DESCRIPTOR_HANDLE material,
        D3D12_GPU_DESCRIPTOR_HANDLE depth);

private:
    bool CreateRootSignature(Device& device);
    bool CreatePipelineState(
        Device& device,
        DXGI_FORMAT rtvFormat,
        const std::vector<uint8_t>& vsBytecode,
        const std::vector<uint8_t>& psBytecode);
    static bool LoadShaderBytecode(const char* path, std::vector<uint8_t>& outBytecode);

    ID3D12RootSignature* m_rootSignature = nullptr;
    ID3D12PipelineState* m_pipelineState = nullptr;
};

} // namespace summer::gfx
