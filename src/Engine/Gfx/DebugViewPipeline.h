#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace summer::gfx {

class Device;

// Fullscreen (or viewport) visualization of G-buffer channels. Bindings:
//   t0..t3  SRV tables  - albedo, normal, material, depth
//   b0      32-bit constants - visualization mode
//   s0      static sampler - point, clamp
class DebugViewPipeline {
public:
    enum class Mode : uint32_t {
        Albedo = 0,
        Normal,
        Occlusion,
        Metallic,
        Roughness,
        Emissive,
        Depth,
    };

    DebugViewPipeline() = default;
    ~DebugViewPipeline();

    DebugViewPipeline(const DebugViewPipeline&) = delete;
    DebugViewPipeline& operator=(const DebugViewPipeline&) = delete;

    bool Initialize(
        Device& device,
        DXGI_FORMAT rtvFormat,
        const char* vertexShaderPath,
        const char* pixelShaderPath);
    void Shutdown();

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        D3D12_GPU_DESCRIPTOR_HANDLE albedo,
        D3D12_GPU_DESCRIPTOR_HANDLE normal,
        D3D12_GPU_DESCRIPTOR_HANDLE material,
        D3D12_GPU_DESCRIPTOR_HANDLE depth,
        Mode mode);

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
