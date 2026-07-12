#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace summer::gfx {

class Device;

// Loads compiled DXIL (.cso), builds root signature + PSO for the basic mesh pass.
class BasicPipeline {
public:
    BasicPipeline() = default;
    ~BasicPipeline();

    BasicPipeline(const BasicPipeline&) = delete;
    BasicPipeline& operator=(const BasicPipeline&) = delete;

    bool Initialize(
        Device& device,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        const char* vertexShaderPath,
        const char* pixelShaderPath);
    void Shutdown();

    void Bind(ID3D12GraphicsCommandList* commandList);

    // Pushes the per-object model-view-projection matrix as root constants (b0).
    void SetModelViewProj(
        ID3D12GraphicsCommandList* commandList,
        const DirectX::XMFLOAT4X4& mvp);

    // Binds the base color texture SRV (must live in the currently-bound shader-visible heap).
    void SetBaseColorTexture(
        ID3D12GraphicsCommandList* commandList,
        D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle);

    ID3D12RootSignature* GetRootSignature() const { return m_rootSignature; }

private:
    bool LoadShaderBytecode(const char* path, std::vector<uint8_t>& outBytecode);
    bool CreateRootSignature(Device& device);
    bool CreatePipelineState(
        Device& device,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        const std::vector<uint8_t>& vsBytecode,
        const std::vector<uint8_t>& psBytecode);

    ID3D12RootSignature* m_rootSignature = nullptr;
    ID3D12PipelineState* m_pipelineState = nullptr;
};

} // namespace summer::gfx
