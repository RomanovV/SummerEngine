#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace summer::gfx {

class Device;

// Fullscreen pass that resolves the HDR scene target to the swapchain: exposure -> tonemap -> gamma.
// Bindings: t0 = HDR SRV (descriptor table), b0 = 32-bit constants (exposure + operator).
class TonemapPipeline {
public:
    enum class Operator : uint32_t { Reinhard = 0, ACES = 1 };

    TonemapPipeline() = default;
    ~TonemapPipeline();

    TonemapPipeline(const TonemapPipeline&) = delete;
    TonemapPipeline& operator=(const TonemapPipeline&) = delete;

    bool Initialize(
        Device& device,
        DXGI_FORMAT rtvFormat,
        const char* vertexShaderPath,
        const char* pixelShaderPath);
    void Shutdown();

    // Records a fullscreen draw (3 vertices, no vertex buffer). The shared SRV heap must be bound.
    void Draw(
        ID3D12GraphicsCommandList* commandList,
        D3D12_GPU_DESCRIPTOR_HANDLE hdrSrv,
        float exposure,
        Operator op);

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
